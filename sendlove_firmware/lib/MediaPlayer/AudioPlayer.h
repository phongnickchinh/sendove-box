#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include <Arduino.h>
#include <driver/i2s.h>
#include "IStorageProvider.h"
#include "config.h"

// ============================================================================
// AudioPlayer — Phát PCM raw 16-bit Mono qua I2S DMA (MAX98357A)
// ============================================================================
// Thiết kế: Non-blocking tick-based.
// Mỗi frame video, gọi tick() 1 lần để nạp thêm dữ liệu vào DMA buffer.
// I2S DMA tự phát liên tục trên phần cứng, không block CPU.
// ============================================================================

class AudioPlayer {
public:
    /// Khởi tạo I2S driver với các hằng số từ config.h
    bool init();

    /// Dừng I2S và giải phóng driver
    void stop();

    /// Phát một đoạn bip ngắn để test loa khi boot
    void testBeep();

    /// Bíp sin ~1.6kHz dài durationMs, BLOCK tới khi phát xong (+200ms xả DMA).
    /// Chỉ gọi khi không phát tin (dùng chung I2S với tick()).
    void beep(uint32_t durationMs);

    /// Tìm AUDC header trong storage bắt đầu từ byte thứ videoDataSize.
    /// appendedSize = số byte audio bảng slot ghi nhận (gồm cả header AUDC), 0 = không rõ.
    /// Trả về true nếu tìm thấy audio hợp lệ.
    bool loadFromStorage(IStorageProvider* storage, uint32_t videoDataSize, uint32_t appendedSize = 0);

    /// Xoa DMA roi nap day truoc khi phat (goi ca luc bat dau lan khi loop).
    void prefill();

    /// Gọi mỗi frame để refill I2S DMA buffer. Non-blocking.
    void tick();

    bool hasAudio()     const { return _hasAudio; }
    bool isInitialized() const { return _initialized; }

private:
    // 10 bytes: "AUDC" (4) + sampleRate uint16 (2) + pcmSize uint32 (4)
    static constexpr size_t AUDC_HEADER_SIZE = 10;
    // Header RIFF/WAVE chuẩn 44 byte do mediaEncoder.js sinh ra
    static constexpr size_t WAV_HEADER_SIZE = 44;

    IStorageProvider* _storage      = nullptr;
    bool              _hasAudio     = false;
    bool              _initialized  = false;

    uint32_t _audioPcmOffset = 0; // Offset tuyệt đối trong slot: sau video + 10 bytes AUDC header
    uint32_t _audioPcmSize   = 0; // Tổng bytes PCM
    uint32_t _audioCursor    = 0; // Bytes đã đưa vào DMA
    uint32_t _sampleRate     = AUDIO_SAMPLE_RATE; // Đọc từ header AUDC, không hardcode

    // Buffer đọc 1 chunk PCM từ NAND (stack-allocated, tránh malloc)
    uint8_t _chunk[AUDIO_PCM_CHUNK_SIZE];
    // Mono -> Stereo (x2) rồi lặp mỗi mẫu AUDIO_OVERSAMPLE lần (I2S mở ở tốc độ
    // file x AUDIO_OVERSAMPLE, xem config.h) để BCLK đủ cao cho MAX98357A.
    int16_t _stereo[AUDIO_PCM_CHUNK_SIZE / 2 * 2 * AUDIO_OVERSAMPLE];

    /// Đọc 1 chunk PCM từ NAND và ghi vào I2S (Mono → Stereo expand)
    /// Tra ve true neu DMA nhan het chunk (con cho, nen nap tiep).
    bool fillChunk();
};

#endif // AUDIO_PLAYER_H
