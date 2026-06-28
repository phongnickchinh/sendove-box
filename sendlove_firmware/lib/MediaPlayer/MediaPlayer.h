#ifndef MEDIA_PLAYER_H
#define MEDIA_PLAYER_H

#include <Arduino.h>
#include "driver/i2s.h"

// Forward declarations
class SDCardManager;
class DisplayDriver;

// ============================================================================
// MediaPlayer — Phát video (SPI) + âm thanh (I2S) đồng bộ
// ============================================================================
// Phục vụ Task 3: Task_MediaPlayer
//
// Cơ chế hoạt động:
// - Đọc file .bin (video RGB565) và .wav (audio PCM) từ thẻ SD
// - Sử dụng double-buffering: đọc block tiếp theo từ SD
//   trong khi block hiện tại đang được đẩy ra SPI/I2S
// - Đồng bộ hình-tiếng dựa trên frame timing
// ============================================================================

/// Trạng thái phát
enum class PlaybackState : uint8_t {
    IDLE,       // Chưa bắt đầu / đã kết thúc
    PLAYING,    // Đang phát
    ERROR       // Lỗi (file không tồn tại, lỗi đọc SD, ...)
};

/// WAV file header (44 bytes chuẩn)
struct WAVHeader {
    char     riff[4];        // "RIFF"
    uint32_t fileSize;       // Kích thước file - 8
    char     wave[4];        // "WAVE"
    char     fmt[4];         // "fmt "
    uint32_t fmtSize;        // Kích thước fmt chunk (16 cho PCM)
    uint16_t audioFormat;    // 1 = PCM
    uint16_t numChannels;    // 1 = Mono, 2 = Stereo
    uint32_t sampleRate;     // VD: 16000
    uint32_t byteRate;       // sampleRate * numChannels * bitsPerSample/8
    uint16_t blockAlign;     // numChannels * bitsPerSample/8
    uint16_t bitsPerSample;  // VD: 16
    char     data[4];        // "data"
    uint32_t dataSize;       // Kích thước dữ liệu audio
};

class MediaPlayer {
public:
    /// Khởi tạo I2S driver và liên kết với SDCard + Display
    /// @param sdCard Con trỏ tới SDCardManager
    /// @param display Con trỏ tới DisplayDriver
    /// @param bclkPin Chân I2S BCLK (Bit Clock)
    /// @param lrcPin Chân I2S LRC (Word Select)
    /// @param doutPin Chân I2S Data Out
    /// @return true nếu khởi tạo thành công
    bool init(SDCardManager* sdCard, DisplayDriver* display,
              uint8_t bclkPin, uint8_t lrcPin, uint8_t doutPin);

    /// Phát tin nhắn (video + audio đồng bộ)
    /// @param videoPath Đường dẫn file .bin trên SD
    /// @param audioPath Đường dẫn file .wav trên SD
    /// @return true nếu phát thành công
    bool playMessage(const char* videoPath, const char* audioPath);

    /// Dừng phát (nếu đang phát)
    void stop();

    /// Lấy trạng thái hiện tại
    PlaybackState getState() const;

private:
    SDCardManager*  _sdCard  = nullptr;
    DisplayDriver*  _display = nullptr;
    PlaybackState   _state   = PlaybackState::IDLE;

    // I2S config
    i2s_port_t _i2sPort = I2S_NUM_0;

    /// Khởi tạo I2S driver
    bool initI2S(uint8_t bclkPin, uint8_t lrcPin, uint8_t doutPin,
                 uint32_t sampleRate, uint8_t bitsPerSample);

    /// Dọn dẹp I2S driver
    void deinitI2S();

    /// Parse WAV header và validate
    bool parseWAVHeader(const uint8_t* headerData, WAVHeader* header);
};

#endif // MEDIA_PLAYER_H
