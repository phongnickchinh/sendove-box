/**
 * voiceRecorder.js
 * 
 * Sử dụng Web Audio API (MediaRecorder) để ghi âm giọng nói.
 * Trả về file WAV chuẩn PCM 16-bit, mono, 16kHz tương thích trực tiếp với ESP32 I2S.
 */

export class VoiceRecorder {
  constructor() {
    this.mediaRecorder = null;
    this.audioChunks = [];
    this.stream = null;
    this.audioContext = null;
    this.analyser = null;
    this.dataArray = null;
    this.startTime = 0;
  }

  async start() {
    this.stream = await navigator.mediaDevices.getUserMedia({ audio: { channelCount: 1 } });
    
    // Set up AudioContext for visualization and resampling
    this.audioContext = new (window.AudioContext || window.webkitAudioContext)();
    const source = this.audioContext.createMediaStreamSource(this.stream);
    
    this.analyser = this.audioContext.createAnalyser();
    this.analyser.fftSize = 256;
    source.connect(this.analyser);
    
    const bufferLength = this.analyser.frequencyBinCount;
    this.dataArray = new Uint8Array(bufferLength);

    this.audioChunks = [];
    // Sử dụng MediaRecorder để lấy raw webm/ogg, sau đó ta sẽ chuyển sang wav bằng AudioContext khi stop
    this.mediaRecorder = new MediaRecorder(this.stream);
    
    this.mediaRecorder.ondataavailable = (e) => {
      if (e.data.size > 0) this.audioChunks.push(e.data);
    };

    this.startTime = Date.now();
    this.mediaRecorder.start();
  }

  getWaveformData() {
    if (!this.analyser || !this.dataArray) return new Uint8Array(0);
    this.analyser.getByteFrequencyData(this.dataArray);
    return this.dataArray;
  }

  async stop() {
    return new Promise((resolve) => {
      if (!this.mediaRecorder || this.mediaRecorder.state === 'inactive') {
        resolve(null);
        return;
      }

      this.mediaRecorder.onstop = async () => {
        const durationSec = (Date.now() - this.startTime) / 1000;
        
        // Dừng tracks để giải phóng mic
        this.stream.getTracks().forEach(track => track.stop());
        
        // Chuyển chunks (thường là webm/ogg) sang WAV PCM 16kHz
        const blob = new Blob(this.audioChunks, { type: this.mediaRecorder.mimeType });
        const arrayBuffer = await blob.arrayBuffer();
        
        // Decode audio data using an OfflineAudioContext to resample to 16kHz
        const tempContext = new (window.AudioContext || window.webkitAudioContext)();
        const decodedAudio = await tempContext.decodeAudioData(arrayBuffer);
        
        const targetSampleRate = 16000;
        const offlineContext = new OfflineAudioContext(
          1, 
          decodedAudio.duration * targetSampleRate, 
          targetSampleRate
        );
        
        const source = offlineContext.createBufferSource();
        source.buffer = decodedAudio;
        source.connect(offlineContext.destination);
        source.start(0);
        
        const renderedBuffer = await offlineContext.startRendering();
        
        // Convert to WAV format
        const wavBlob = this._audioBufferToWav(renderedBuffer);
        
        if (this.audioContext) {
          this.audioContext.close();
        }
        
        resolve({
          wavBlob,
          duration: Math.round(durationSec)
        });
      };

      this.mediaRecorder.stop();
    });
  }

  // Helper chuyển AudioBuffer (Float32) sang WAV (PCM 16-bit)
  _audioBufferToWav(buffer) {
    const numChannels = buffer.numberOfChannels;
    const sampleRate = buffer.sampleRate;
    const format = 1; // PCM
    const bitDepth = 16;
    
    let result;
    if (numChannels === 2) {
      result = this._interleave(buffer.getChannelData(0), buffer.getChannelData(1));
    } else {
      result = buffer.getChannelData(0);
    }
    
    return this._encodeWAV(result, format, sampleRate, numChannels, bitDepth);
  }

  _interleave(inputL, inputR) {
    const length = inputL.length + inputR.length;
    const result = new Float32Array(length);
    let index = 0, inputIndex = 0;
    while (index < length) {
      result[index++] = inputL[inputIndex];
      result[index++] = inputR[inputIndex];
      inputIndex++;
    }
    return result;
  }

  _encodeWAV(samples, format, sampleRate, numChannels, bitDepth) {
    const bytesPerSample = bitDepth / 8;
    const blockAlign = numChannels * bytesPerSample;
    const buffer = new ArrayBuffer(44 + samples.length * bytesPerSample);
    const view = new DataView(buffer);

    // RIFF chunk descriptor
    this._writeString(view, 0, 'RIFF');
    view.setUint32(4, 36 + samples.length * bytesPerSample, true);
    this._writeString(view, 8, 'WAVE');

    // FMT sub-chunk
    this._writeString(view, 12, 'fmt ');
    view.setUint32(16, 16, true); // Subchunk1Size
    view.setUint16(20, format, true); // AudioFormat
    view.setUint16(22, numChannels, true);
    view.setUint32(24, sampleRate, true);
    view.setUint32(28, sampleRate * blockAlign, true); // ByteRate
    view.setUint16(32, blockAlign, true);
    view.setUint16(34, bitDepth, true);

    // Data sub-chunk
    this._writeString(view, 36, 'data');
    view.setUint32(40, samples.length * bytesPerSample, true);

    // Write PCM samples
    this._floatTo16BitPCM(view, 44, samples);

    return new Blob([view], { type: 'audio/wav' });
  }

  _writeString(view, offset, string) {
    for (let i = 0; i < string.length; i++) {
      view.setUint8(offset + i, string.charCodeAt(i));
    }
  }

  _floatTo16BitPCM(output, offset, input) {
    for (let i = 0; i < input.length; i++, offset += 2) {
      const s = Math.max(-1, Math.min(1, input[i]));
      output.setInt16(offset, s < 0 ? s * 0x8000 : s * 0x7FFF, true);
    }
  }
}
