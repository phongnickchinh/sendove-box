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

// Loa MAX98357A trên box phát mono 16-bit. 8kHz đủ cho giọng nói và giữ file
// nhỏ để vừa slot NAND (một slot chứa cả video lẫn audio).
const AUDIO_SAMPLE_RATE = 8000;
const AUDIO_MAX_SECONDS = 15; // Khớp với trần 15s của video ở encodeVideoToBin

export const extractAudioFromVideo = async (videoBlob, onProgress) => {
  // Giải mã offline thay vì play() realtime: không phụ thuộc autoplay policy,
  // không mất mẫu khi tab bị throttle, và chạy nhanh hơn thời lượng thật.
  const arrayBuffer = await videoBlob.arrayBuffer();

  const decodeCtx = new (window.AudioContext || window.webkitAudioContext)();
  let decoded;
  try {
    decoded = await decodeCtx.decodeAudioData(arrayBuffer);
  } catch (err) {
    // Trình duyệt không giải mã được audio track của container này.
    // Video vẫn gửi được, chỉ là không có tiếng.
    console.error('Không giải mã được audio track của video', err);
    return null;
  } finally {
    decodeCtx.close();
  }

  if (onProgress) onProgress(40);

  if (!decoded.length || !decoded.duration) {
    console.error('Video không có audio track');
    return null;
  }

  // Resample về 8kHz mono. OfflineAudioContext lo cả downmix (destination 1 kênh)
  // lẫn nội suy tần số, chính xác hơn tự viết tay.
  const duration = Math.min(decoded.duration, AUDIO_MAX_SECONDS);
  const frames = Math.ceil(duration * AUDIO_SAMPLE_RATE);
  const OfflineCtx = window.OfflineAudioContext || window.webkitOfflineAudioContext;
  const offlineCtx = new OfflineCtx(1, frames, AUDIO_SAMPLE_RATE);

  const source = offlineCtx.createBufferSource();
  source.buffer = decoded;

  // Nén đỉnh trước khi ghi WAV. Ampli MAX98357A trên box dùng chung nguồn với
  // đèn nền; mỗi đỉnh transient của giọng nói làm nó kéo dòng đột ngột → sụt áp
  // → tiếng rè và màn hình nhấp nháy. Ngưỡng đặt ở -6dB nên file vốn nhỏ tiếng
  // gần như không bị động tới — chỉ các đỉnh thật sự cao mới bị ghim lại.
  const compressor = offlineCtx.createDynamicsCompressor();
  compressor.threshold.value = -6;
  compressor.knee.value = 6;
  compressor.ratio.value = 4;
  compressor.attack.value = 0.003;
  compressor.release.value = 0.15;

  source.connect(compressor);
  compressor.connect(offlineCtx.destination);
  source.start(0);

  const rendered = await offlineCtx.startRendering();
  logAndCapPeak(rendered);

  if (onProgress) onProgress(100);

  return audioBufferToWavBlob(rendered);
};

// Trần biên độ gửi xuống box. Chỉ hạ xuống, KHÔNG bao giờ nâng lên: nâng đỉnh
// là nâng đúng dòng đỉnh đang gây sụt áp.
const AUDIO_PEAK_CEILING = 0.7;

function logAndCapPeak(buffer) {
  const data = buffer.getChannelData(0);
  let peak = 0;
  for (let i = 0; i < data.length; i++) {
    const a = Math.abs(data[i]);
    if (a > peak) peak = a;
  }

  if (peak === 0) {
    console.warn('[VOICE] audio toàn số 0 sau khi render');
    return;
  }

  const gain = Math.min(1, AUDIO_PEAK_CEILING / peak);
  if (gain < 1) {
    for (let i = 0; i < data.length; i++) data[i] *= gain;
  }
  console.log(`[VOICE] đỉnh sau nén ${peak.toFixed(3)} → ${(peak * gain).toFixed(3)} (gain ${gain.toFixed(2)})`);
}
