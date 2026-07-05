export const config = {
  // 1. Kích thước Màn hình TFT (Tạm thời để 320x240 làm mặc định)
  display: {
    width: 320,
    height: 240,
    pixelFormat: 'rgb565',
  },
  
  // 2. Chuẩn Âm thanh DAC (I2S)
  audio: {
    sampleRate: 16000,
    bitDepth: 16,
    channels: 1, // Mono
    format: 's16le', // PCM 16-bit little-endian
  },

  // 3. Quản lý lưu trữ
  storage: {
    autoDeleteAfterDownload: true, // Xóa file nhị phân sau khi ESP32 tải xong
  },

  // 4. Giới hạn Video
  video: {
    maxDurationSeconds: 15,
    framesPerSecond: 10,
  }
};
