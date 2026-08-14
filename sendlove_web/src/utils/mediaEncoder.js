/**
 * mediaEncoder.js
 * 
 * Mã hóa Video, Image thành file .bin cho ESP32.
 * Định dạng mới (Tối ưu dung lượng): Header SLBX 16-byte + (Frame Size + JPEG Data)
 */

const HEADER_MAGIC = [0x53, 0x4C, 0x42, 0x58]; // 'SLBX'
const VERSION = 0x01;
const TARGET_WIDTH = 240;
const TARGET_HEIGHT = 240;

function createHeader(type, fps, totalFrames) {
  const header = new Uint8Array(16);
  header.set(HEADER_MAGIC, 0);
  header[4] = VERSION;
  header[5] = type; // 0x01 = video, 0x02 = image
  
  // Width (240) - Little Endian
  header[6] = TARGET_WIDTH & 0xFF;
  header[7] = (TARGET_WIDTH >> 8) & 0xFF;
  
  // Height (240) - Little Endian
  header[8] = TARGET_HEIGHT & 0xFF;
  header[9] = (TARGET_HEIGHT >> 8) & 0xFF;
  
  header[10] = fps;
  
  // Total frames - Little Endian
  header[11] = totalFrames & 0xFF;
  header[12] = (totalFrames >> 8) & 0xFF;
  
  // Bytes 13-15 are reserved (0x00)
  return header;
}

// Hàm lấy Blob JPEG từ Canvas
const getJpegBlob = (canvas, quality = 0.7) => {
  return new Promise((resolve) => {
    canvas.toBlob((blob) => resolve(blob), 'image/jpeg', quality);
  });
};

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

// Gói dữ liệu JPEG với kích thước (4 bytes little-endian) ở đầu
const packJpegFrame = async (jpegBlob) => {
  const arrayBuffer = await jpegBlob.arrayBuffer();
  const dataSize = arrayBuffer.byteLength;
  
  const sizeHeader = new Uint8Array(4);
  sizeHeader[0] = dataSize & 0xFF;
  sizeHeader[1] = (dataSize >> 8) & 0xFF;
  sizeHeader[2] = (dataSize >> 16) & 0xFF;
  sizeHeader[3] = (dataSize >> 24) & 0xFF;
  
  return new Blob([sizeHeader, arrayBuffer], { type: 'application/octet-stream' });
};

export const encodeImageToBin = async (imageBlob) => {
  // Ảnh từ component ImageInput đã được crop sẵn vuông 240x240
  const bitmap = await createImageBitmap(imageBlob);
  const canvas = document.createElement('canvas');
  canvas.width = TARGET_WIDTH;
  canvas.height = TARGET_HEIGHT;
  const ctx = canvas.getContext('2d');
  
  ctx.drawImage(bitmap, 0, 0, TARGET_WIDTH, TARGET_HEIGHT);
  
  const jpegBlob = await getJpegBlob(canvas, 0.8);
  const packedFrame = await packJpegFrame(jpegBlob);
  
  const header = createHeader(0x02, 1, 1);
  const blob = new Blob([header, packedFrame], { type: 'application/octet-stream' });
  
  return {
    binBlob: blob,
    thumbBlob: jpegBlob, // Thumbnail cũng là JPEG
    frameCount: 1,
    duration: 0
  };
};

export const encodeVideoToBin = async (videoBlob, onProgress) => {
  return new Promise((resolve, reject) => {
    const video = document.createElement('video');
    video.src = URL.createObjectURL(videoBlob);
    video.muted = true;
    video.setAttribute('playsinline', ''); // Hỗ trợ mobile
    
    video.onloadeddata = async () => {
      const fps = 15; // Target FPS
      const duration = Math.min(video.duration, 15); // Max 15 seconds
      const totalFrames = Math.floor(duration * fps);
      
      const canvas = document.createElement('canvas');
      canvas.width = TARGET_WIDTH;
      canvas.height = TARGET_HEIGHT;
      const ctx = canvas.getContext('2d');
      
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
        
        // Nén Canvas thành JPEG
        const jpegBlob = await getJpegBlob(canvas, 0.7); // Quality 70% giống file Python
        
        if (currentFrame === Math.floor(totalFrames / 2)) {
          thumbBlob = jpegBlob;
        }
        
        // Đóng gói Header Size (4 bytes) + JPEG Data
        const packedFrame = await packJpegFrame(jpegBlob);
        frames.push(packedFrame);
        
        if (onProgress) {
          onProgress(Math.round((currentFrame / totalFrames) * 100));
        }
        
        currentFrame++;
        captureFrame();
      };
      
      video.onerror = () => reject(new Error('Failed to seek video'));
      
      captureFrame();
    };
    
    video.onerror = () => reject(new Error('Failed to load video'));
  });
};

function audioBufferToWavBlob(buffer) {
  const numChannels = buffer.numberOfChannels;
  const sampleRate = buffer.sampleRate;
  const format = 1; // PCM
  const bitDepth = 16;
  
  const result = new Float32Array(buffer.length);
  buffer.copyFromChannel(result, 0, 0); // Assuming mono
  
  // Calculate size
  const dataLength = result.length * (bitDepth / 8);
  const bufferLength = 44 + dataLength;
  const arrayBuffer = new ArrayBuffer(bufferLength);
  const view = new DataView(arrayBuffer);
  
  // Write WAV header
  const writeString = (view, offset, string) => {
    for (let i = 0; i < string.length; i++) {
      view.setUint8(offset + i, string.charCodeAt(i));
    }
  };
  
  writeString(view, 0, 'RIFF');
  view.setUint32(4, 36 + dataLength, true);
  writeString(view, 8, 'WAVE');
  writeString(view, 12, 'fmt ');
  view.setUint32(16, 16, true); // Format chunk size
  view.setUint16(20, format, true);
  view.setUint16(22, numChannels, true);
  view.setUint32(24, sampleRate, true);
  view.setUint32(28, sampleRate * numChannels * (bitDepth / 8), true); // Byte rate
  view.setUint16(32, numChannels * (bitDepth / 8), true); // Block align
  view.setUint16(34, bitDepth, true);
  writeString(view, 36, 'data');
  view.setUint32(40, dataLength, true);
  
  // Write PCM data
  let offset = 44;
  for (let i = 0; i < result.length; i++, offset += 2) {
    let s = Math.max(-1, Math.min(1, result[i]));
    view.setInt16(offset, s < 0 ? s * 0x8000 : s * 0x7FFF, true);
  }
  
  return new Blob([view], { type: 'audio/wav' });
}

export const extractAudioFromVideo = (videoBlob, onProgress) => {
  return new Promise((resolve, reject) => {
    const video = document.createElement('video');
    video.src = URL.createObjectURL(videoBlob);
    video.muted = false; // Phải bật tiếng để Web Audio API bắt được sóng âm
    video.setAttribute('playsinline', '');
    
    video.onloadeddata = () => {
      try {
        const audioCtx = new (window.AudioContext || window.webkitAudioContext)({ sampleRate: 16000 });
        const source = audioCtx.createMediaElementSource(video);
        
        const processor = audioCtx.createScriptProcessor(4096, 1, 1);
        
        // Dùng GainNode để mute đầu ra loa ngoài, tránh làm ồn người dùng
        const gainNode = audioCtx.createGain();
        gainNode.gain.value = 0;
        
        source.connect(processor);
        processor.connect(gainNode);
        gainNode.connect(audioCtx.destination);
        
        let audioChunks = [];
        let totalLength = 0;
        
        processor.onaudioprocess = (e) => {
          const inputData = e.inputBuffer.getChannelData(0);
          const chunk = new Float32Array(inputData);
          audioChunks.push(chunk);
          totalLength += chunk.length;
          
          if (onProgress && video.duration) {
            onProgress(Math.round((video.currentTime / video.duration) * 100));
          }
        };
        
        video.onended = () => {
          source.disconnect();
          processor.disconnect();
          gainNode.disconnect();
          audioCtx.close();
          URL.revokeObjectURL(video.src);
          
          if (totalLength === 0) {
            console.warn('Không bắt được sóng âm nào từ video');
            return resolve(null);
          }
          
          // Gộp chunks
          const mergedBuffer = new Float32Array(totalLength);
          let offset = 0;
          for (let chunk of audioChunks) {
            mergedBuffer.set(chunk, offset);
            offset += chunk.length;
          }
          
          // Chuyển sang AudioBuffer để đưa vào hàm convert WAV
          const offlineCtx = new (window.OfflineAudioContext || window.webkitOfflineAudioContext)(1, totalLength, 16000);
          const finalBuffer = offlineCtx.createBuffer(1, totalLength, 16000);
          finalBuffer.copyToChannel(mergedBuffer, 0);
          
          resolve(audioBufferToWavBlob(finalBuffer));
        };
        
        video.onerror = () => {
          URL.revokeObjectURL(video.src);
          reject(new Error('Lỗi khi play video để trích xuất âm thanh'));
        };
        
        // Play để thu âm
        video.play().catch((e) => {
          URL.revokeObjectURL(video.src);
          console.warn('Trình duyệt chặn autoplay có tiếng, không thể trích xuất âm thanh', e);
          resolve(null); // Fallback: Bỏ qua âm thanh nếu bị chặn
        });
      } catch (err) {
        console.warn('Lỗi setup trích xuất âm thanh', err);
        resolve(null);
      }
    };
    
    video.onerror = () => {
      URL.revokeObjectURL(video.src);
      reject(new Error('Không thể tải file video'));
    };
  });
};
