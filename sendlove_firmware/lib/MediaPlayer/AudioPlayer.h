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

    /// Tìm AUDC header trong storage bắt đầu từ byte thứ videoDataSize.
    /// Trả về true nếu tìm thấy audio hợp lệ.
    bool loadFromStorage(IStorageProvider* storage, uint32_t videoDataSize);

    /// Nạp trước 2 DMA buffer đầu tiên để tránh tiếng click khi bắt đầu.
    void prefill();

    /// Gọi mỗi frame để refill I2S DMA buffer. Non-blocking.
    void tick();

    bool hasAudio()     const { return _hasAudio; }
    bool isInitialized() const { return _initialized; }

private:
    // 10 bytes: "AUDC" (4) + sampleRate uint16 (2) + pcmSize uint32 (4)
    static constexpr size_t AUDC_HEADER_SIZE = 10;

    IStorageProvider* _storage      = nullptr;
    bool              _hasAudio     = false;
    bool              _initialized  = false;

    uint32_t _audioPcmOffset = 0; // Offset tuyệt đối trong slot: sau video + 10 bytes AUDC header
    uint32_t _audioPcmSize   = 0; // Tổng bytes PCM
    uint32_t _audioCursor    = 0; // Bytes đã đưa vào DMA

    // Buffer đọc 1 chunk PCM từ NAND (stack-allocated, tránh malloc)
    uint8_t _chunk[AUDIO_PCM_CHUNK_SIZE];

    /// Đọc 1 chunk PCM từ NAND và ghi vào I2S (Mono → Stereo expand)
    void fillChunk();
};

#endif // AUDIO_PLAYER_H
