#ifndef MEDIA_PLAYER_H
#define MEDIA_PLAYER_H

#include <Arduino.h>
#include <JPEGDEC.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "IStorageProvider.h"
#include "AudioPlayer.h"
#include "config.h"

class DisplayDriver;

// ============================================================================
// MediaPlayer — Phát video VJPG / ảnh VIMG từ IStorageProvider (NAND / SD)
// ============================================================================
// Phục vụ Task_MediaPlayer trong kiến trúc FreeRTOS.
//
// Cơ chế hoạt động:
// - Đọc JPEG frame từ IStorageProvider
// - Giải mã bằng JPEGDEC → callback pushImage lên DisplayDriver
// - Hỗ trợ 2 mode: VJPG (video lặp vô hạn) và VIMG (ảnh tĩnh)
// ============================================================================

/// Trạng thái phát
enum class PlaybackState : uint8_t {
    IDLE,
    PLAYING,
    SHOWING,
    ERROR
};

/// Video (VJPG) and Image (VIMG) player from Storage Provider
class MediaPlayer {
public:
    ~MediaPlayer();

    /// Initialize MediaPlayer instance
    bool init(IStorageProvider* storage, DisplayDriver* display);

    /// Start playing media from item ID
    bool playItem(const char* identifier);

    /// Update playback loop frame timing
    void update();

    /// Stop current playback
    void stop();

    /// Test I2S speaker beep
    void testAudioBeep();

    /// Một hồi bíp báo thức (block ~0.6s). Gọi khi player đang IDLE.
    void alarmBeep();


    /// Get current playback state
    PlaybackState getState() const;

    /// Get current active slot index (-1 if IDLE)
    int8_t getCurrentSlot() const;

private:
    static constexpr size_t JPEG_BUFFER_SIZE = 32 * 1024;

    // Khoảng nghỉ tối thiểu giữa hai lần giải mã. Chặn trường hợp hai frame
    // dính liền nhau — đó là lúc dòng tiêu thụ vọt lên và làm sụt áp.
    static constexpr uint32_t FRAME_MIN_IDLE_MS = 2;

    IStorageProvider* _storage = nullptr;
    DisplayDriver*    _display = nullptr;
    PlaybackState     _state   = PlaybackState::IDLE;
    SemaphoreHandle_t _playerMutex = nullptr;

    // 17.884 byte — thành phần to nhất của cả appCtx (24.508 byte RAM tĩnh), nhưng
    // chỉ sống trong _jpeg->decode(). Để nó là member trực tiếp nghĩa là giữ 17,9KB
    // BSS suốt đời máy, đúng loại RAM mà mbedTLS cần khối ~16KB liền mạch, đúng lúc
    // hộp đứng ở màn hình chờ bắt tay TLS (§21). Cấp/giải phóng cùng nhịp với
    // _jpegBuffer: đỉnh RAM lúc phát không đổi, chỉ lúc chờ mới dư ra.
    // An toàn: openRAM() mở đầu bằng memset(&_jpeg, 0, sizeof(JPEGIMAGE)) nên đối
    // tượng cấp trên heap với rác vẫn đúng — nó không dựa vào BSS được xoá sẵn.
    JPEGDEC* _jpeg          = nullptr;
    uint8_t* _jpegBuffer    = nullptr;
    int8_t   _currentSlot   = -1;
    char     _currentId[32] = "";
    uint16_t _fps          = 10;
    uint16_t _totalFrames  = 0;
    uint16_t _currentFrame = 0;
    uint32_t _nextFrameDeadline = 0;   // Mốc millis() của frame kế; cộng dồn để không trôi
    uint32_t _currentDataSize = 0;
    uint32_t _currentAudioSize = 0;   // Byte audio nối sau video (từ SlotEntry.audioSize)
    uint32_t _frameBaseOffset = 0;
    bool     _readFrameSizeHeader = true;
    bool     _lastFrameSkipped = false;  // Không bỏ hai frame liên tiếp

    bool     _isSlbxRgb565  = false;
    uint16_t _slbxWidth     = 128;
    uint16_t _slbxHeight    = 160;

    /// Decode and render single JPEG frame.
    /// skipRender = true: vẫn nuốt đúng số byte của frame để giữ vị trí file,
    /// nhưng bỏ phần đắt nhất: giải mã JPEG và đẩy nguyên frame qua SPI.
    bool decodeOneFrame(bool skipRender);

    /// Callback function for JPEGDEC pixel output
    static int jpegDrawCallback(JPEGDRAW* pDraw);

    AudioPlayer _audio;
};

#endif // MEDIA_PLAYER_H
