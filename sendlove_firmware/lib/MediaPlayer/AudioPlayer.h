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

    /// Nhạc báo thức: file độc lập trên thẻ (AUDC + WAV + PCM), giữ handle đọc ngẫu nhiên
    /// của SDCardManager tới stop(). loop = hết bài quay lại đầu.
    bool loadFromFile(class SDCardManager* card, const char* path, bool loop);

    /// Xoa DMA roi nap day truoc khi phat (goi ca luc bat dau lan khi loop).
    void prefill();

    /// Gọi mỗi frame để refill I2S DMA buffer. Non-blocking.
    void tick();

    bool hasAudio()     const { return _hasAudio; }
    bool isInitialized() const { return _initialized; }

    /// Âm lượng 0..100 (Settings::volumeGainQ15). Đổi giữa lúc đang phát thì hệ số
    /// trượt dần trong vài chục ms (không "bụp"). 100 = đi đúng đường cũ, không nhân.
    /// immediate = true: nhảy thẳng tới mức mới (đầu bài, chưa có mẫu nào ra loa).
    void setVolume(uint8_t vol, bool immediate = false);

private:
    // 10 bytes: "AUDC" (4) + sampleRate uint16 (2) + pcmSize uint32 (4)
    static constexpr size_t AUDC_HEADER_SIZE = 10;
    // Header RIFF/WAVE chuẩn 44 byte do mediaEncoder.js sinh ra
    static constexpr size_t WAV_HEADER_SIZE = 44;

    IStorageProvider* _storage      = nullptr;
    class SDCardManager* _card      = nullptr;  // nguồn file (nhạc báo thức), thay cho _storage
    bool              _loop         = false;
    bool              _hasAudio     = false;
    bool              _initialized  = false;

    uint32_t _audioPcmOffset = 0; // Offset tuyệt đối trong slot: sau video + 10 bytes AUDC header
    uint32_t _audioPcmSize   = 0; // Tổng bytes PCM
    uint32_t _audioCursor    = 0; // Bytes đã đưa vào DMA
    uint32_t _sampleRate     = AUDIO_SAMPLE_RATE; // Đọc từ header AUDC, không hardcode

    // Hệ số âm lượng Q15. 32768 = đúng bằng đường cũ (bỏ qua phép nhân).
    int32_t _gain       = 32768;  // đang dùng
    int32_t _gainTarget = 32768;  // đích; fillChunk() trượt _gain tới đây
    bool    _muted      = false;  // âm lượng 0: tắt ampli (PIN_AMP_SD) nếu có nối

    /// Bật/tắt ampli qua SD_MODE. Không làm gì nếu PIN_AMP_SD < 0.
    void setAmp(bool on);

    /// Đọc tại offset tuyệt đối từ nguồn hiện tại (slot tin nhắn hoặc file nhạc).
    int readSrc(uint32_t offset, uint8_t* buf, uint32_t len);
    /// Dò header AUDC tại audioStartOffset (+ WAV nếu có), chốt _audioPcmOffset/_Size.
    bool parseAudc(uint32_t audioStartOffset, uint32_t appendedSize, uint32_t maxPcm);

    // Buffer đọc PCM từ storage. Cỡ ĐỌC (AUDIO_READ_CHUNK_SIZE) tách khỏi cỡ
    // giãn mẫu (AUDIO_PCM_CHUNK_SIZE) vì trên thẻ SD mỗi lượt đọc là một fread
    // xuyên VFS/FATFS — xem config.h.
    uint8_t _chunk[AUDIO_READ_CHUNK_SIZE];
    // Mono -> Stereo (x2) rồi lặp mỗi mẫu AUDIO_OVERSAMPLE lần (I2S mở ở tốc độ
    // file x AUDIO_OVERSAMPLE, xem config.h) để BCLK đủ cao cho MAX98357A.
    int16_t _stereo[AUDIO_PCM_CHUNK_SIZE / 2 * 2 * AUDIO_OVERSAMPLE];

    /// Đọc 1 chunk PCM từ NAND và ghi vào I2S (Mono → Stereo expand)
    /// Tra ve true neu DMA nhan het chunk (con cho, nen nap tiep).
    bool fillChunk();
};

#endif // AUDIO_PLAYER_H
