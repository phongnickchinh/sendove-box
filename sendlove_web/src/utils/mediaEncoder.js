/**
 * Encodes video, images and audio into the files the box plays.
 * Visual format: 16-byte SLBX header + repeated (u32 frame size + JPEG data).
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

// Prefix the JPEG data with its size (u32 little-endian).
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
  // ImageInput already cropped the image to a 240x240 square.
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
    thumbBlob: jpegBlob, // the thumbnail is the same JPEG
    frameCount: 1,
    duration: 0
  };
};

/**
 * Hard cap for any clip — the max duration the backend accepts
 * (validation.middleware.ts confirmMessageSchema). The per-storage cap
 * (15s NAND / 60s SD) is applied by the range picker, not here.
 */
const HARD_MAX_SECONDS = 60;

/** Clamp [start, end) to a valid segment of a media `total` seconds long. */
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
    video.setAttribute('playsinline', ''); // required on mobile

    video.onloadeddata = async () => {
      const fps = 15; // Target FPS
      // Encode only the segment the user picked in VideoInput.
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
        
        const jpegBlob = await getJpegBlob(canvas, 0.7);
        
        if (currentFrame === Math.floor(totalFrames / 2)) {
          thumbBlob = jpegBlob;
        }
        
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

/** Mono AudioBuffer (Float32) → 16-bit PCM WAV Blob. */
export function audioBufferToWavBlob(buffer) {
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

// The box's MAX98357A plays 16-bit mono. 8 kHz is enough for speech and keeps
// the file small enough for a NAND slot (one slot holds both video and audio).
const AUDIO_SAMPLE_RATE = 8000;

/**
 * PCM budget the current firmware can load for ONE audio file
 * (AUDIO_MAX_PCM_BYTES = 600000, config.h) — only ~18.7s at 16 kHz. Longer
 * voice messages drop to 8 kHz (~37.5s) so the box still plays them in full.
 * Beyond 37.5s the firmware cap itself has to be raised.
 */
export const FW_AUDIO_PCM_BYTES = 600000;

export function voiceSampleRate(durationSec) {
  return durationSec * 16000 * 2 <= FW_AUDIO_PCM_BYTES ? 16000 : 8000;
}

/** Decode anything the browser can play (webm/ogg/mp3/m4a/wav/mp4) into an AudioBuffer. */
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
 * Cut [start, end) from an AudioBuffer, downmix to mono, resample, normalize
 * loudness and limit peaks (see renderSegment) → WAV PCM16. Used for voice
 * messages (recorded or uploaded) and background music of still messages.
 */
export async function encodeAudioSegment(buffer, range, sampleRate) {
  const { start, duration } = segmentOf(buffer.duration, range);
  const rate = sampleRate || voiceSampleRate(duration);
  const rendered = await renderSegment(buffer, start, duration, rate);
  return { wavBlob: audioBufferToWavBlob(rendered), duration: Math.round(duration), sampleRate: rate };
}

/**
 * Alarm music: 16 kHz mono (product decision — the "stay at 8 kHz" rule is
 * lifted for music ONLY), 5–60 seconds (the box rings for at most a minute),
 * source file ≤ 15 MB — decodeAudioData decodes the whole track into RAM and
 * a 10-minute file crashes the tab on a phone.
 */
export const ALARM_MUSIC = {
  RATE: 16000,
  MIN_S: 5,
  MAX_S: 60,
  MAX_FILE_BYTES: 15 * 1024 * 1024,
  FADE_S: 0.05,
};

/**
 * Cut an alarm-music clip into the file the box plays straight from the card:
 * "AUDC" + u16 sample rate + u32 WAV size (little-endian, matching
 * AudioPlayer::parseAudc), then WAV PCM16. For voice messages the firmware adds
 * AUDC on download; music goes straight to the card, so the web must wrap it.
 * Shares renderSegment (loudness normalization + compression + peak ceiling)
 * with voice messages so alarm and message volume are comparable. High peaks
 * make the amp draw current spikes → voltage sag. A 50ms fade on both ends
 * keeps the loop from clicking.
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

/** Stored .aud file → WAV Blob the browser can play (drops the 10-byte AUDC header). */
export async function audFileToWavBlob(arrayBuffer) {
  return new Blob([arrayBuffer.slice(10)], { type: 'audio/wav' });
}

export const extractAudioFromVideo = async (videoBlob, onProgress, range) => {
  // Decode offline instead of real-time play(): no autoplay-policy dependency,
  // no dropped samples when the tab is throttled, and faster than real time.
  let decoded;
  try {
    decoded = await decodeAudioBlob(videoBlob);
  } catch (err) {
    // The browser can't decode this container's audio track.
    // The video still sends, just without sound.
    console.error('Không giải mã được audio track của video', err);
    return null;
  }

  if (onProgress) onProgress(40);

  if (!decoded.length || !decoded.duration) {
    console.error('Video không có audio track');
    return null;
  }

  // Same segment as the picture (encodeVideoToBin) so audio lines up with frames.
  const { start, duration } = segmentOf(decoded.duration, range);
  const rendered = await renderSegment(decoded, start, duration, AUDIO_SAMPLE_RATE);

  if (onProgress) onProgress(100);

  return audioBufferToWavBlob(rendered);
};

/**
 * Loudness normalization (product decision): everything sent to the box — voice
 * messages, video audio, alarm music — lands at the same loudness, so the
 * volume % on the box is relative to one speaker reference level, not to how
 * loud the source was recorded.
 *
 * Loudness = RMS (dBFS) over 400ms blocks, skipping silent ones (absolute gate
 * at -60 dB, relative gate 10 dB below the mean — same idea as LUFS) so pauses
 * between sentences don't cause over-boosting. Measured after a high-pass at
 * the profile's lowHz: the box's small speaker barely reproduces bass, so a
 * bass-heavy track must not count as "loud".
 *
 * "Really loud" (product decision — at -20 dB every file was quieter than the
 * alarm beep): -10 dB target, heavy compression, drop the bass the speaker
 * can't reproduce (it only costs headroom and amp current), then a limiter
 * pins peaks near 0 dBFS. To make it quieter: lower the profile's targetDb
 * first, AUDIO_PEAK_CEILING second.
 */
// Voice messages / video audio: speech must stay clear; don't compress into distortion.
const PROFILE_VOICE = { targetDb: -10, lowHz: 250, presenceDb: 0 };
// Alarm music: the goal is waking someone up, not sounding good. The beep is a
// 1.6 kHz tone — right where the ear is most sensitive and the small speaker is
// strongest — so at equal RMS it still sounds louder than music. Hence: cut more
// bass (< 400 Hz), boost +6 dB around PRESENCE_HZ, and pin to -6 dB RMS (only
// ~6 dB crest factor: sounds "flat", acceptable for an alarm).
const PROFILE_ALARM = { targetDb: -6, lowHz: 400, presenceDb: 6 };
const PRESENCE_HZ = 2000;
// A very quiet recording is mostly background noise: boosting past this only amplifies noise.
const LOUDNESS_MAX_BOOST_DB = 30;

/**
 * Resample to mono at `rate`. OfflineAudioContext handles both the downmix
 * (1-channel destination) and the sample-rate interpolation.
 * Pass 1 only resamples, to measure loudness; pass 2 cuts bass, applies the
 * normalization gain and compresses. Loudness is re-measured after compression:
 * the browser's DynamicsCompressorNode adds its OWN makeup gain (~+4 dB on
 * Chrome) that can't be disabled, so a final factor pulls it back to target
 * before limiting.
 */
async function renderSegment(decoded, start, duration, rate, profile = PROFILE_VOICE) {
  const { targetDb, lowHz } = profile;
  const plain = await renderPass(decoded, start, duration, rate, { process: false });
  const loudness = await measureLoudness(plain, lowHz);
  const gainDb = loudness === null ? 0 : Math.min(LOUDNESS_MAX_BOOST_DB, targetDb - loudness);
  const rendered = await renderPass(decoded, start, duration, rate, { process: true, gainDb, profile });
  const data = rendered.getChannelData(0);
  // Limiting peaks drops loudness below target (peaky speech loses ~3 dB):
  // iterate measure → compensate → limit a few times to converge on the target.
  for (let pass = 0; pass < 5; pass++) {
    const after = await measureLoudness(rendered, lowHz);
    if (after === null || Math.abs(targetDb - after) < 0.3) break;
    const fix = 10 ** ((targetDb - after) / 20);
    for (let i = 0; i < data.length; i++) data[i] *= fix;
    limitPeaks(data, rate, AUDIO_PEAK_CEILING);
  }
  limitPeaks(data, rate, AUDIO_PEAK_CEILING);  // the loop above may exit before limiting even once
  logAndCapPeak(rendered, loudness, gainDb);
  return rendered;
}

/**
 * 5ms look-ahead limiter: the gain at each sample is the minimum over the
 * window ahead (ducks before the peak, no clipping), releasing back to 1 in
 * ~80ms. Pins peaks without scaling the WHOLE clip the way logAndCapPeak does —
 * scaling everything would throw away the loudness just normalized.
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
    // Below lowHz the speaker makes no sound; that content only eats headroom
    // (peaks hit the ceiling sooner) and amp current. Removing it makes the
    // audible part louder under the same ceiling.
    const highpass = offlineCtx.createBiquadFilter();
    highpass.type = 'highpass';
    highpass.frequency.value = profile.lowHz;
    highpass.Q.value = Math.SQRT1_2;

    // Boost the band where the ear and the speaker are strongest (0 dB = no-op).
    const presence = offlineCtx.createBiquadFilter();
    presence.type = 'peaking';
    presence.frequency.value = PRESENCE_HZ;
    presence.Q.value = 0.7;
    presence.gain.value = profile.presenceDb;

    const gain = offlineCtx.createGain();
    gain.gain.value = 10 ** (gainDb / 20);

    // Heavy compression shrinks the crest factor, so average loudness can go
    // higher under the same peak ceiling. The MAX98357A shares its supply with
    // the backlight; if the box crackles or the screen flickers during playback,
    // lower the profile's targetDb (accepted risk).
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

/** Gated loudness (dBFS) over 400ms blocks; null = the whole clip is silent. */
async function measureLoudness(buffer, lowHz) {
  const rate = buffer.sampleRate;
  const OfflineCtx = window.OfflineAudioContext || window.webkitOfflineAudioContext;
  const ctx = new OfflineCtx(1, buffer.length, rate);
  const source = ctx.createBufferSource();
  source.buffer = buffer;
  // Filter for MEASURING only; the file that gets sent keeps its full band.
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
  const relGate = mean(loud) * 0.1; // 10 dB below the mean
  const kept = loud.filter((e) => e > relGate);
  return 10 * Math.log10(mean(kept));
}

// Peak amplitude ceiling sent to the box (limitPeaks enforces it; logAndCapPeak
// is the last safety net). Product decision: as loud as possible (0.98 = -0.2
// dBFS). It used to be 0.7 to limit voltage sag from the amp sharing the
// backlight supply — if the box crackles or the screen flickers during playback,
// this is the second knob to lower, after the profile's targetDb.
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
