#ifndef MEDIA_PLAYER_H
#define MEDIA_PLAYER_H

#include <Arduino.h>
#include <JPEGDEC.h>
#include "config.h"

// Forward declarations
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
    IDLE,       // Chưa bắt đầu / đã kết thúc
    PLAYING,    // Đang phát video (lặp vô hạn)
    SHOWING,    // Đang hiển thị ảnh tĩnh
    ERROR       // Lỗi
};

class MediaPlayer {
public:
    /// Khởi tạo MediaPlayer
    /// @param nand Con trỏ tới NandStorage
    /// @param display Con trỏ tới DisplayDriver
    /// @return true nếu khởi tạo thành công
    bool init(NandStorage* nand, DisplayDriver* display);

    /// Bắt đầu phát slot (video hoặc ảnh)
    /// @param slot Index slot (0-4)
    /// @return true nếu bắt đầu thành công
    bool playSlot(uint8_t slot);

    /// Cập nhật playback — gọi mỗi vòng loop
    /// Decode 1 frame (VJPG) hoặc no-op (VIMG đã hiển thị)
    void update();

    /// Dừng phát
    void stop();

    /// Lấy trạng thái hiện tại
    PlaybackState getState() const;

    /// Lấy slot đang phát (-1 nếu idle)
    int8_t getCurrentSlot() const;

private:
    NandStorage*    _nand    = nullptr;
    DisplayDriver*  _display = nullptr;
    PlaybackState   _state   = PlaybackState::IDLE;

    JPEGDEC _jpeg;
    int8_t  _currentSlot   = -1;
    uint16_t _fps          = 10;
    uint16_t _totalFrames  = 0;
    uint16_t _currentFrame = 0;

    /// Decode và hiển thị 1 JPEG frame
    /// @return true nếu decode thành công
    bool decodeOneFrame();

    /// JPEGDEC callback — pushImage lên display
    static int jpegDrawCallback(JPEGDRAW* pDraw);
};

#endif // MEDIA_PLAYER_H
