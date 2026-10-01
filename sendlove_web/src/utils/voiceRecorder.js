/**
 * Records voice with MediaRecorder and returns a 16-bit mono 16 kHz PCM WAV,
 * directly compatible with the ESP32's I2S output.
 */
import { audioBufferToWavBlob } from './mediaEncoder';

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
    // MediaRecorder yields raw webm/ogg; stop() converts it to WAV via AudioContext.
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
        
        // Stop the tracks to release the microphone.
        this.stream.getTracks().forEach(track => track.stop());

        // Convert the chunks (usually webm/ogg) to 16 kHz PCM WAV.
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
        
        const wavBlob = audioBufferToWavBlob(renderedBuffer);
        
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
}
