#ifndef MEDIA_PLAYER_H
#define MEDIA_PLAYER_H

#include <Arduino.h>
#include <JPEGDEC.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "IStorageProvider.h"
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

    /// Start playing media from specified slot / item ID
    bool playSlot(uint8_t slot);
    bool playItem(const char* identifier);

    /// Update playback loop frame timing
    void update();

    /// Stop current playback
    void stop();

    /// Get current playback state
    PlaybackState getState() const;

    /// Get current active slot index (-1 if IDLE)
    int8_t getCurrentSlot() const;

private:
    static constexpr size_t JPEG_BUFFER_SIZE = 48 * 1024;

    IStorageProvider* _storage = nullptr;
    DisplayDriver*    _display = nullptr;
    PlaybackState     _state   = PlaybackState::IDLE;
    SemaphoreHandle_t _playerMutex = nullptr;

    JPEGDEC  _jpeg;
    uint8_t* _jpegBuffer    = nullptr;
    int8_t   _currentSlot   = -1;
    char     _currentId[32] = "";
    uint16_t _fps          = 10;
    uint16_t _totalFrames  = 0;
    uint16_t _currentFrame = 0;

    bool     _isSlbxRgb565  = false;
    uint16_t _slbxWidth     = 128;
    uint16_t _slbxHeight    = 160;

    /// Decode and render single JPEG frame
    bool decodeOneFrame();

    /// Callback function for JPEGDEC pixel output
    static int jpegDrawCallback(JPEGDRAW* pDraw);
};

#endif // MEDIA_PLAYER_H

