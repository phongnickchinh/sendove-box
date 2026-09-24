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

/**
 * Trần an toàn của mọi đoạn cắt — đúng trần duration backend chấp nhận
 * (validation.middleware.ts confirmMessageSchema max 60). Trần theo loại hộp
 * (15s NAND / 60s SD) do màn chọn đoạn áp, không đặt ở đây.
 */
const HARD_MAX_SECONDS = 60;

/** Đoạn [start, end) hợp lệ trong một media dài `total` giây. */
function segmentOf(total, { start = 0, end } = {}) {
  const s = Math.max(0, Math.min(start, total));
  const e = Math.min(end ?? total, total, s + HARD_MAX_SECONDS);
  return { start: s, duration: Math.max(0, e - s) };
}

export const encodeVideoToBin = async (videoBlob, onProgress, range) => {
  return new Promise((resolve, reject) => {
    const video = document.createElement('video');
    video.src = URL.createObjectURL(videoBlob);
    video.muted = true;
    video.setAttribute('playsinline', ''); // Hỗ trợ mobile

    video.onloadeddata = async () => {
      const fps = 15; // Target FPS
      // Chỉ mã hoá đoạn người dùng đã chọn ở VideoInput (không còn cắt cứng 15s đầu).
      const { start, duration } = segmentOf(video.duration, range);
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
        
        video.currentTime = start + currentFrame / fps;
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

/**
 * Trần PCM firmware hiện tại nạp được cho MỘT file âm thanh
 * (AUDIO_MAX_PCM_BYTES = 600000, config.h) — 16 kHz chỉ chứa được ~18,7s.
 * Lời nhắn thoại dài hơn thì hạ xuống 8 kHz (~37,5s) để hộp đang chạy vẫn
 * phát trọn. Quá 37,5s thì chỉ còn cách nâng trần phía firmware.
 */
export const FW_AUDIO_PCM_BYTES = 600000;

export function voiceSampleRate(durationSec) {
  return durationSec * 16000 * 2 <= FW_AUDIO_PCM_BYTES ? 16000 : 8000;
}

/** Giải mã mọi thứ trình duyệt phát được (webm/ogg/mp3/m4a/wav/mp4) ra AudioBuffer. */
export async function decodeAudioBlob(blob) {
  const arrayBuffer = await blob.arrayBuffer();
  const ctx = new (window.AudioContext || window.webkitAudioContext)();
  try {
    return await ctx.decodeAudioData(arrayBuffer);
  } finally {
    ctx.close();
  }
}

/**
 * Cắt [start, end) của một AudioBuffer, downmix mono, resample, qua bộ nén
 * đỉnh (xem renderSegment) → WAV PCM16. Dùng cho lời nhắn thoại (thu trực tiếp
 * hoặc file tải lên) và nhạc nền của tin tĩnh.
 */
export async function encodeAudioSegment(buffer, range, sampleRate) {
  const { start, duration } = segmentOf(buffer.duration, range);
  const rate = sampleRate || voiceSampleRate(duration);
  const rendered = await renderSegment(buffer, start, duration, rate);
  return { wavBlob: audioBufferToWavBlob(rendered), duration: Math.round(duration), sampleRate: rate };
}

/**
 * Nhạc báo thức (thiết kế 2026-09-24): 16 kHz mono (user chốt, mở lại "giữ 8kHz" RIÊNG cho
 * nhạc), 5–60 giây (hộp kêu tối đa 1 phút), file gốc ≤ 15 MB — decodeAudioData giải mã
 * cả bài vào RAM, file 10 phút trên điện thoại làm sập tab.
 */
export const ALARM_MUSIC = {
  RATE: 16000,
  MIN_S: 5,
  MAX_S: 60,
  MAX_FILE_BYTES: 15 * 1024 * 1024,
  FADE_S: 0.05,
};

/**
 * Cắt đoạn nhạc báo thức → file hộp phát thẳng từ thẻ: "AUDC" + u16 tần số + u32 cỡ WAV
 * (little-endian, đúng AudioPlayer::parseAudc) rồi WAV PCM16. Tin thoại thì firmware tự
 * thêm AUDC lúc tải; nhạc tải thẳng xuống thẻ nên web phải đóng gói sẵn.
 * Dùng chung renderSegment (nén + trần đỉnh 0.7) như tin thoại: đỉnh cao làm ampli kéo
 * dòng đột ngột → sụt áp. Fade 50ms hai đầu để hộp phát lặp không nghe "tạch".
 */
export async function encodeAlarmMusic(buffer, range) {
  const { start, duration } = segmentOf(buffer.duration, range);
  const rendered = await renderSegment(buffer, start, duration, ALARM_MUSIC.RATE);

  const data = rendered.getChannelData(0);
  const fade = Math.min(Math.floor(ALARM_MUSIC.FADE_S * ALARM_MUSIC.RATE), Math.floor(data.length / 2));
  for (let i = 0; i < fade; i++) {
    const g = i / fade;
    data[i] *= g;
    data[data.length - 1 - i] *= g;
  }

  const wav = new Uint8Array(await audioBufferToWavBlob(rendered).arrayBuffer());
  const out = new Uint8Array(10 + wav.length);
  out.set([0x41, 0x55, 0x44, 0x43], 0); // "AUDC"
  const view = new DataView(out.buffer);
  view.setUint16(4, ALARM_MUSIC.RATE, true);
  view.setUint32(6, wav.length, true);
  out.set(wav, 10);
  return {
    blob: new Blob([out], { type: 'application/octet-stream' }),
    durationMs: Math.round(duration * 1000),
  };
}

/** File .aud đã lưu → Blob WAV trình duyệt phát được (bỏ 10 byte AUDC ở đầu). */
export async function audFileToWavBlob(arrayBuffer) {
  return new Blob([arrayBuffer.slice(10)], { type: 'audio/wav' });
}

export const extractAudioFromVideo = async (videoBlob, onProgress, range) => {
  // Giải mã offline thay vì play() realtime: không phụ thuộc autoplay policy,
  // không mất mẫu khi tab bị throttle, và chạy nhanh hơn thời lượng thật.
  let decoded;
  try {
    decoded = await decodeAudioBlob(videoBlob);
  } catch (err) {
    // Trình duyệt không giải mã được audio track của container này.
    // Video vẫn gửi được, chỉ là không có tiếng.
    console.error('Không giải mã được audio track của video', err);
    return null;
  }

  if (onProgress) onProgress(40);

  if (!decoded.length || !decoded.duration) {
    console.error('Video không có audio track');
    return null;
  }

  // Cùng đoạn với phần hình (encodeVideoToBin) để tiếng khớp khung.
  const { start, duration } = segmentOf(decoded.duration, range);
  const rendered = await renderSegment(decoded, start, duration, AUDIO_SAMPLE_RATE);

  if (onProgress) onProgress(100);

  return audioBufferToWavBlob(rendered);
};

/**
 * Resample về `rate` mono. OfflineAudioContext lo cả downmix (destination 1
 * kênh) lẫn nội suy tần số, chính xác hơn tự viết tay.
 */
async function renderSegment(decoded, start, duration, rate) {
  const frames = Math.max(1, Math.ceil(duration * rate));
  const OfflineCtx = window.OfflineAudioContext || window.webkitOfflineAudioContext;
  const offlineCtx = new OfflineCtx(1, frames, rate);

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
  source.start(0, start, duration);

  const rendered = await offlineCtx.startRendering();
  logAndCapPeak(rendered);
  return rendered;
}

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
