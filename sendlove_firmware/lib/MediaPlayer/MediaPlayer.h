#ifndef MEDIA_PLAYER_H
#define MEDIA_PLAYER_H

#include <Arduino.h>
#include <JPEGDEC.h>
#include "config.h"

class NandStorage;
class DisplayDriver;

// ============================================================================
// MediaPlayer — Phát video VJPG / ảnh VIMG từ NAND Flash
// ============================================================================
// Phục vụ Task_MediaPlayer trong kiến trúc FreeRTOS.
//
// Cơ chế hoạt động:
// - Đọc JPEG frame từ NandStorage (NAND Flash W25Q128)
// - Giải mã bằng JPEGDEC → callback pushImage lên DisplayDriver
// - Hỗ trợ 2 mode: VJPG (video lặp vô hạn) và VIMG (ảnh tĩnh)
//
// Phase 1: Chỉ phát video/ảnh, chưa có I2S audio.
// ============================================================================

/// Trạng thái phát
enum class PlaybackState : uint8_t {
    IDLE,
    PLAYING,
    SHOWING,
    ERROR
};

/// Video (VJPG) and Image (VIMG) player from NAND Flash
class MediaPlayer {
public:
    /// Initialize MediaPlayer instance
    bool init(NandStorage* nand, DisplayDriver* display);

    /// Start playing media from specified slot
    bool playSlot(uint8_t slot);

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

    NandStorage*    _nand    = nullptr;
    DisplayDriver*  _display = nullptr;
    PlaybackState   _state   = PlaybackState::IDLE;

    JPEGDEC  _jpeg;
    uint8_t* _jpegBuffer    = nullptr;
    int8_t   _currentSlot   = -1;
    uint16_t _fps          = 10;
    uint16_t _totalFrames  = 0;
    uint16_t _currentFrame = 0;

    /// Decode and render single JPEG frame
    bool decodeOneFrame();

    /// Callback function for JPEGDEC pixel output
    static int jpegDrawCallback(JPEGDRAW* pDraw);
};

#endif // MEDIA_PLAYER_H
