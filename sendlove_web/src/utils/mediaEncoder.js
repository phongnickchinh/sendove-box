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
 * Cắt [start, end) của một AudioBuffer, downmix mono, resample, chuẩn hoá độ to,
 * qua bộ nén đỉnh (xem renderSegment) → WAV PCM16. Dùng cho lời nhắn thoại (thu trực tiếp
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
 * Dùng chung renderSegment (chuẩn hoá độ to + nén + trần đỉnh) như tin thoại: cùng độ
 * to thì âm lượng báo thức và tin nhắn mới so được với nhau. Đỉnh cao làm ampli kéo
 * dòng đột ngột → sụt áp. Fade 50ms hai đầu để hộp phát lặp không nghe "tạch".
 */
export async function encodeAlarmMusic(buffer, range) {
  const { start, duration } = segmentOf(buffer.duration, range);
  const rendered = await renderSegment(buffer, start, duration, ALARM_MUSIC.RATE, PROFILE_ALARM);

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
 * Chuẩn hoá độ to (user chốt 2026-09-25): mọi đoạn gửi xuống hộp — tin thoại, tiếng video,
 * nhạc báo thức — về cùng một độ to, để % âm lượng trên hộp là % so với một mức chuẩn của
 * loa chứ không phụ thuộc file gốc thu to hay nhỏ.
 *
 * Độ to = RMS (dBFS) của các khối 400ms, bỏ khối im lặng (cổng tuyệt đối -60 dB và cổng
 * tương đối -10 dB dưới trung bình, cùng ý với LUFS) để khoảng lặng giữa câu không làm file
 * bị nâng quá tay. Đo sau lọc thông cao SPEAKER_LOW_HZ: loa nhỏ của hộp gần như không
 * phát được âm trầm, bài nhiều bass không được tính là "to".
 *
 * "To thật to" (user chốt 2026-09-25, sau khi nghe bản -20 dB: mọi file đều nhỏ hơn tiếng bíp,
 * "ru ngủ người dùng"; user tự nghe rồi hạ dần nếu cần): mức -10 dB, nén mạnh, bỏ hẳn phần
 * trầm loa không phát được (chỉ tốn biên độ và dòng ampli), rồi limiter ghim đỉnh sát 0 dBFS.
 * Muốn nhỏ lại: hạ targetDb của profile trước, rồi mới hạ AUDIO_PEAK_CEILING.
 */
// Tin thoại / tiếng video: vẫn phải nghe rõ lời, không nén tới mức méo.
const PROFILE_VOICE = { targetDb: -10, lowHz: 250, presenceDb: 0 };
// Nhạc báo thức (user nghe: bản -10 dB vẫn thua tiếng bíp khi cả hai 100%): mục đích là đánh
// thức, không phải nghe hay. Bíp là một âm 1,6 kHz — đúng vùng tai nhạy nhất và loa nhỏ kêu
// khoẻ nhất — nên cùng RMS nó vẫn to hơn nhạc. Nhạc báo thức vì vậy: bỏ thêm trầm (< 400 Hz),
// nâng +6 dB quanh PRESENCE_HZ để dồn năng lượng về vùng đó, rồi ghim tới -6 dB RMS (khoảng
// cách đỉnh / trung bình chỉ còn ~6 dB: nghe "dẹt", chấp nhận cho báo thức).
const PROFILE_ALARM = { targetDb: -6, lowHz: 400, presenceDb: 6 };
const PRESENCE_HZ = 2000;
// File thu quá nhỏ phần lớn là tiếng ồn nền: nâng hơn mức này chỉ nghe thấy ồn.
const LOUDNESS_MAX_BOOST_DB = 30;

/**
 * Resample về `rate` mono. OfflineAudioContext lo cả downmix (destination 1
 * kênh) lẫn nội suy tần số, chính xác hơn tự viết tay.
 * Lượt 1 chỉ resample để đo độ to; lượt 2 lọc trầm, nhân hệ số chuẩn hoá rồi nén. Đo lại sau
 * nén: DynamicsCompressorNode của trình duyệt TỰ cộng độ lợi bù (makeup gain, đo được ~+4 dB
 * trên Chrome), không tắt được, nên phải kéo về mức chuẩn bằng một hệ số cuối, rồi limiter.
 */
async function renderSegment(decoded, start, duration, rate, profile = PROFILE_VOICE) {
  const { targetDb, lowHz } = profile;
  const plain = await renderPass(decoded, start, duration, rate, { process: false });
  const loudness = await measureLoudness(plain, lowHz);
  const gainDb = loudness === null ? 0 : Math.min(LOUDNESS_MAX_BOOST_DB, targetDb - loudness);
  const rendered = await renderPass(decoded, start, duration, rate, { process: true, gainDb, profile });
  const data = rendered.getChannelData(0);
  // Limiter hạ đỉnh thì độ to tụt dưới mức chuẩn (giọng nói nhiều đỉnh nhọn tụt ~3 dB):
  // lặp đo → bù → ghim đỉnh vài lượt để tiến sát mức chuẩn.
  for (let pass = 0; pass < 5; pass++) {
    const after = await measureLoudness(rendered, lowHz);
    if (after === null || Math.abs(targetDb - after) < 0.3) break;
    const fix = 10 ** ((targetDb - after) / 20);
    for (let i = 0; i < data.length; i++) data[i] *= fix;
    limitPeaks(data, rate, AUDIO_PEAK_CEILING);
  }
  limitPeaks(data, rate, AUDIO_PEAK_CEILING);  // vòng trên có thể thoát trước khi ghim lượt nào
  logAndCapPeak(rendered, loudness, gainDb);
  return rendered;
}

/**
 * Limiter nhìn trước 5ms: hệ số tại mỗi mẫu = nhỏ nhất trong cửa sổ phía trước (hạ kịp trước
 * đỉnh, không cắt méo), nhả về 1 trong ~80ms. Ghim đỉnh mà không phải hạ CẢ bài như
 * logAndCapPeak — hạ cả bài là mất độ to vừa chuẩn hoá.
 */
function limitPeaks(data, rate, ceiling) {
  const look = Math.max(1, Math.round(0.005 * rate));
  const release = Math.exp(-1 / (0.08 * rate));
  const need = new Float32Array(data.length);
  for (let i = 0; i < data.length; i++) {
    const a = Math.abs(data[i]);
    need[i] = a > ceiling ? ceiling / a : 1;
  }
  let g = 1;
  for (let i = 0; i < data.length; i++) {
    let target = 1;
    const end = Math.min(data.length, i + look + 1);
    for (let j = i; j < end; j++) if (need[j] < target) target = need[j];
    g = target < g ? target : target + (g - target) * release;
    data[i] *= g;
  }
}

async function renderPass(decoded, start, duration, rate, { process, gainDb = 0, profile = PROFILE_VOICE }) {
  const frames = Math.max(1, Math.ceil(duration * rate));
  const OfflineCtx = window.OfflineAudioContext || window.webkitOfflineAudioContext;
  const offlineCtx = new OfflineCtx(1, frames, rate);

  const source = offlineCtx.createBufferSource();
  source.buffer = decoded;

  if (!process) {
    source.connect(offlineCtx.destination);
  } else {
    // Phần dưới lowHz loa không phát ra tiếng, chỉ chiếm biên độ (làm đỉnh chạm
    // trần sớm) và kéo dòng ampli. Bỏ đi thì cùng trần đỉnh, phần nghe được to hơn.
    const highpass = offlineCtx.createBiquadFilter();
    highpass.type = 'highpass';
    highpass.frequency.value = profile.lowHz;
    highpass.Q.value = Math.SQRT1_2;

    // Nâng vùng tai nhạy / loa kêu khoẻ (0 dB = không đổi gì).
    const presence = offlineCtx.createBiquadFilter();
    presence.type = 'peaking';
    presence.frequency.value = PRESENCE_HZ;
    presence.Q.value = 0.7;
    presence.gain.value = profile.presenceDb;

    const gain = offlineCtx.createGain();
    gain.gain.value = 10 ** (gainDb / 20);

    // Nén mạnh để khoảng cách đỉnh / trung bình nhỏ lại: cùng trần đỉnh thì độ to trung bình
    // lên được cao. Ampli MAX98357A dùng chung nguồn với đèn nền; nếu breadboard rè / nháy
    // màn khi phát, hạ LOUDNESS_TARGET_DB (user nhận rủi ro, 2026-09-25).
    const compressor = offlineCtx.createDynamicsCompressor();
    compressor.threshold.value = -24;
    compressor.knee.value = 12;
    compressor.ratio.value = 6;
    compressor.attack.value = 0.003;
    compressor.release.value = 0.15;

    source.connect(highpass);
    highpass.connect(presence);
    presence.connect(gain);
    gain.connect(compressor);
    compressor.connect(offlineCtx.destination);
  }
  source.start(0, start, duration);
  return offlineCtx.startRendering();
}

/** Độ to (dBFS) theo khối 400ms có cổng im lặng; null = cả đoạn im lặng. */
async function measureLoudness(buffer, lowHz) {
  const rate = buffer.sampleRate;
  const OfflineCtx = window.OfflineAudioContext || window.webkitOfflineAudioContext;
  const ctx = new OfflineCtx(1, buffer.length, rate);
  const source = ctx.createBufferSource();
  source.buffer = buffer;
  // Chỉ lọc lúc ĐO; file gửi xuống giữ nguyên dải tần.
  const highpass = ctx.createBiquadFilter();
  highpass.type = 'highpass';
  highpass.frequency.value = lowHz;
  highpass.Q.value = Math.SQRT1_2;
  source.connect(highpass);
  highpass.connect(ctx.destination);
  source.start();
  const data = (await ctx.startRendering()).getChannelData(0);

  const block = Math.max(1, Math.round(0.4 * rate));
  const energies = [];
  for (let i = 0; i < data.length; i += block) {
    const end = Math.min(i + block, data.length);
    let sum = 0;
    for (let j = i; j < end; j++) sum += data[j] * data[j];
    energies.push(sum / (end - i));
  }
  const mean = (arr) => arr.reduce((s, e) => s + e, 0) / arr.length;
  const loud = energies.filter((e) => e > 1e-6); // -60 dB
  if (!loud.length) return null;
  const relGate = mean(loud) * 0.1; // -10 dB dưới trung bình
  const kept = loud.filter((e) => e > relGate);
  return 10 * Math.log10(mean(kept));
}

// Trần biên độ gửi xuống box (limitPeaks ghim, logAndCapPeak là lưới an toàn cuối).
// 0,7 (-3,1 dBFS) → 0,98 (-0,2 dBFS) ngày 2026-09-25: user muốn to hết mức, tự nghe rồi hạ
// dần. Trước đây 0,7 để đỡ sụt áp do ampli dùng chung nguồn đèn nền — nếu hộp rè / nháy màn
// khi phát thì đây là nút thứ hai cần hạ, sau LOUDNESS_TARGET_DB.
const AUDIO_PEAK_CEILING = 0.98;

function logAndCapPeak(buffer, loudness, gainDb) {
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
  console.log(`[VOICE] độ to ${loudness === null ? 'im lặng' : loudness.toFixed(1) + ' dB'}, `
    + `chuẩn hoá ${gainDb >= 0 ? '+' : ''}${gainDb.toFixed(1)} dB; `
    + `đỉnh sau nén ${peak.toFixed(3)} → ${(peak * gain).toFixed(3)} (gain ${gain.toFixed(2)})`);
}
