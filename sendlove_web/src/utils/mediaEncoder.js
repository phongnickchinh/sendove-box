/**
 * mediaEncoder.js
 * 
 * Mã hóa Video, Image, GIF thành RGB565 raw format (.bin) cho ESP32.
 * Định dạng: Header 16-byte + Raw Frames.
 */

const HEADER_MAGIC = [0x53, 0x4C, 0x42, 0x58]; // 'SLBX'
const VERSION = 0x01;
const TARGET_WIDTH = 128;
const TARGET_HEIGHT = 160;

function createHeader(type, fps, totalFrames) {
  const header = new Uint8Array(16);
  header.set(HEADER_MAGIC, 0);
  header[4] = VERSION;
  header[5] = type; // 0x01 = video/gif, 0x02 = image
  
  // Width (128) - Little Endian
  header[6] = TARGET_WIDTH & 0xFF;
  header[7] = (TARGET_WIDTH >> 8) & 0xFF;
  
  // Height (160) - Little Endian
  header[8] = TARGET_HEIGHT & 0xFF;
  header[9] = (TARGET_HEIGHT >> 8) & 0xFF;
  
  header[10] = fps;
  
  // Total frames - Little Endian
  header[11] = totalFrames & 0xFF;
  header[12] = (totalFrames >> 8) & 0xFF;
  
  // Bytes 13-15 are reserved (0x00)
  return header;
}

function rgbaToRgb565(imageData) {
  const data = imageData.data;
  const pixels = new Uint16Array(TARGET_WIDTH * TARGET_HEIGHT);
  for (let i = 0, j = 0; i < data.length; i += 4, j++) {
    const r = data[i];
    const g = data[i + 1];
    const b = data[i + 2];
    // RGB565: R(5) G(6) B(5)
    pixels[j] = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
  }
  // Convert to little-endian bytes for ESP32
  const bytes = new Uint8Array(pixels.length * 2);
  for (let i = 0; i < pixels.length; i++) {
    bytes[i * 2] = pixels[i] & 0xFF;
    bytes[i * 2 + 1] = (pixels[i] >> 8) & 0xFF;
  }
  return bytes;
}

function drawScaledCropped(ctx, source, sourceWidth, sourceHeight) {
  const targetRatio = TARGET_WIDTH / TARGET_HEIGHT;
  const sourceRatio = sourceWidth / sourceHeight;
  
  let drawWidth = sourceWidth;
  let drawHeight = sourceHeight;
  let offsetX = 0;
  let offsetY = 0;

  if (sourceRatio > targetRatio) {
    drawWidth = sourceHeight * targetRatio;
    offsetX = (sourceWidth - drawWidth) / 2;
  } else {
    drawHeight = sourceWidth / targetRatio;
    offsetY = (sourceHeight - drawHeight) / 2;
  }

  ctx.clearRect(0, 0, TARGET_WIDTH, TARGET_HEIGHT);
  ctx.drawImage(source, offsetX, offsetY, drawWidth, drawHeight, 0, 0, TARGET_WIDTH, TARGET_HEIGHT);
}

export const encodeImageToBin = async (imageBlob) => {
  const bitmap = await createImageBitmap(imageBlob);
  const canvas = document.createElement('canvas');
  canvas.width = TARGET_WIDTH;
  canvas.height = TARGET_HEIGHT;
  const ctx = canvas.getContext('2d', { willReadFrequently: true });
  
  drawScaledCropped(ctx, bitmap, bitmap.width, bitmap.height);
  
  const imageData = ctx.getImageData(0, 0, TARGET_WIDTH, TARGET_HEIGHT);
  const rgb565Data = rgbaToRgb565(imageData);
  
  const header = createHeader(0x02, 1, 1);
  const blob = new Blob([header, rgb565Data], { type: 'application/octet-stream' });
  
  // Extract thumbnail
  const thumbBlob = await new Promise(resolve => canvas.toBlob(resolve, 'image/jpeg', 0.8));
  
  return {
    binBlob: blob,
    thumbBlob,
    frameCount: 1,
    duration: 0
  };
};

export const encodeVideoToBin = async (videoBlob, onProgress) => {
  return new Promise((resolve, reject) => {
    const video = document.createElement('video');
    video.src = URL.createObjectURL(videoBlob);
    video.muted = true;
    
    video.onloadeddata = async () => {
      const fps = 15; // Target FPS for ESP32
      const duration = Math.min(video.duration, 15); // Max 15 seconds
      const totalFrames = Math.floor(duration * fps);
      
      const canvas = document.createElement('canvas');
      canvas.width = TARGET_WIDTH;
      canvas.height = TARGET_HEIGHT;
      const ctx = canvas.getContext('2d', { willReadFrequently: true });
      
      const frames = [];
      let currentFrame = 0;
      let thumbBlob = null;
      
      const captureFrame = async () => {
        if (currentFrame >= totalFrames) {
          const header = createHeader(0x01, fps, totalFrames);
          const binBlob = new Blob([header, ...frames], { type: 'application/octet-stream' });
          URL.revokeObjectURL(video.src);
          resolve({
            binBlob,
            thumbBlob,
            frameCount: totalFrames,
            duration: Math.round(duration)
          });
          return;
        }
        
        video.currentTime = currentFrame / fps;
      };
      
      video.onseeked = async () => {
        drawScaledCropped(ctx, video, video.videoWidth, video.videoHeight);
        
        if (currentFrame === Math.floor(totalFrames / 2)) {
          thumbBlob = await new Promise(res => canvas.toBlob(res, 'image/jpeg', 0.8));
        }
        
        const imageData = ctx.getImageData(0, 0, TARGET_WIDTH, TARGET_HEIGHT);
        frames.push(rgbaToRgb565(imageData));
        
        if (onProgress) {
          onProgress(Math.round((currentFrame / totalFrames) * 100));
        }
        
        currentFrame++;
        captureFrame();
      };
      
      captureFrame();
    };
    
    video.onerror = () => reject(new Error('Failed to load video'));
  });
};

export const extractAudioFromVideo = async (videoBlob) => {
  const arrayBuffer = await videoBlob.arrayBuffer();
  const tempContext = new (window.AudioContext || window.webkitAudioContext)();
  
  try {
    const decodedAudio = await tempContext.decodeAudioData(arrayBuffer);
    const targetSampleRate = 16000;
    const offlineContext = new OfflineAudioContext(1, decodedAudio.duration * targetSampleRate, targetSampleRate);
    
    const source = offlineContext.createBufferSource();
    source.buffer = decodedAudio;
    source.connect(offlineContext.destination);
    source.start(0);
    
    const renderedBuffer = await offlineContext.startRendering();
    tempContext.close();
    
    // We import voiceRecorder here dynamically or use a shared helper to avoid circular dependency
    // But for simplicity, we can just return the AudioBuffer and let voiceRecorder build WAV
    return renderedBuffer;
  } catch (e) {
    console.warn('No audio track found in video or failed to decode', e);
    return null; // Video might not have audio
  }
};
