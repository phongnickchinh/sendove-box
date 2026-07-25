#include "MediaPlayer.h"
#include "NandStorage.h"
#include "DisplayDriver.h"
#include "config.h"

// ============================================================================
// MediaPlayer Implementation — VJPG/VIMG from NAND Flash
// ============================================================================

// Static pointer cho JPEGDEC callback (JPEGDEC không hỗ trợ user data pointer)
static DisplayDriver* s_display = nullptr;

// JPEG buffer: 48KB — đủ cho 1 JPEG frame 240x240 @ quality 70
static uint8_t s_jpegBuffer[48 * 1024];

// ============================================================================
// JPEGDEC Callback
// ============================================================================

int MediaPlayer::jpegDrawCallback(JPEGDRAW* pDraw) {
    if (s_display) {
        s_display->pushImage(pDraw->x, pDraw->y, pDraw->iWidth, pDraw->iHeight,
                             pDraw->pPixels);
    }
    return 1; // Tiếp tục decode
}

// ============================================================================
// Init
// ============================================================================

bool MediaPlayer::init(NandStorage* nand, DisplayDriver* display) {
    _nand = nand;
    _display = display;
    s_display = display; // Set static pointer cho callback

    _state = PlaybackState::IDLE;

    Serial.println(F("[Player] Initialized (JPEGDEC + NAND)"));
    return true;
}

// ============================================================================
// Playback Control
// ============================================================================

bool MediaPlayer::playSlot(uint8_t slot) {
    if (_nand == nullptr || _display == nullptr) {
        Serial.println(F("[Player] ERROR: NAND or Display not initialized"));
        _state = PlaybackState::ERROR;
        return false;
    }

    if (!_nand->isSlotValid(slot)) {
        Serial.printf("[Player] ERROR: Slot %d is invalid/empty\n", slot);
        _state = PlaybackState::ERROR;
        return false;
    }

    // Dừng playback hiện tại nếu có
    stop();

    // Lấy thông tin slot
    SlotEntry info = _nand->getSlotInfo(slot);
    _fps = info.fps;
    _totalFrames = info.totalFrames;
    _currentFrame = 0;
    _currentSlot = slot;

    // Mở slot cho sequential read
    if (!_nand->openSlot(slot)) {
        _state = PlaybackState::ERROR;
        return false;
    }

    // Bật màn hình & xóa màn hình sạch trước khi phát slot mới
    _display->turnOn();
    _display->clear();
    _display->setBacklight(BACKLIGHT_DAY_PERCENT);

    if (_nand->isSlotImage(slot)) {
        // VIMG: Decode 1 frame rồi giữ nguyên
        Serial.printf("[Player] Showing image from slot %d\n", slot);
        _state = PlaybackState::SHOWING;
        decodeOneFrame();
    } else {
        // VJPG: Bắt đầu phát video
        Serial.printf("[Player] Playing video from slot %d (%u frames @ %u FPS)\n",
                      slot, _totalFrames, _fps);
        _state = PlaybackState::PLAYING;
    }

    return true;
}

void MediaPlayer::update() {
    if (_state == PlaybackState::PLAYING) {
        uint32_t frameStart = millis();

        bool ok = decodeOneFrame();

        if (!ok) {
            // Hết video hoặc lỗi decode → loop lại từ đầu
            _nand->seekSlot(0);
            _currentFrame = 0;
            return;
        }

        _currentFrame++;
        if (_currentFrame >= _totalFrames) {
            // Loop video
            _nand->seekSlot(0);
            _currentFrame = 0;
        }

        // Frame timing
        uint32_t elapsed = millis() - frameStart;
        uint32_t targetMs = (_fps > 0) ? (1000 / _fps) : FRAME_DURATION_MS;
        if (elapsed < targetMs) {
            vTaskDelay(pdMS_TO_TICKS(targetMs - elapsed));
        }
    }
    else if (_state == PlaybackState::SHOWING) {
        // VIMG: Ảnh tĩnh đã hiển thị, chỉ yield
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    else {
        // IDLE hoặc ERROR: yield
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void MediaPlayer::stop() {
    if (_state == PlaybackState::PLAYING || _state == PlaybackState::SHOWING) {
        _nand->closeSlot();
    }
    _state = PlaybackState::IDLE;
    _currentSlot = -1;
    _currentFrame = 0;
}

PlaybackState MediaPlayer::getState() const {
    return _state;
}

int8_t MediaPlayer::getCurrentSlot() const {
    return _currentSlot;
}

// ============================================================================
// JPEG Decode Pipeline
// ============================================================================

bool MediaPlayer::decodeOneFrame() {
    // 1. Đọc JPEG size (4 bytes, little-endian)
    uint32_t jpegSize = 0;
    int bytesRead = _nand->readData((uint8_t*)&jpegSize, 4);

    if (bytesRead < 4 || jpegSize == 0 || jpegSize > sizeof(s_jpegBuffer)) {
        return false; // End of data hoặc frame quá lớn
    }

    // 2. Đọc JPEG data
    bytesRead = _nand->readData(s_jpegBuffer, jpegSize);
    if ((uint32_t)bytesRead < jpegSize) {
        return false; // Đọc thiếu
    }

    // 3. Decode JPEG → pushImage callback → display
    // Acquire SPI mutex 1 lần cho toàn bộ quá trình decode + render
    if (!_display->acquireSPI()) {
        return false;
    }

    if (_jpeg.openRAM(s_jpegBuffer, jpegSize, jpegDrawCallback)) {
        _jpeg.setPixelType(RGB565_LITTLE_ENDIAN);

        // Bọc trong startWrite() / endWrite() để LovyanGFX giữ luồng SPI ghi liên tục cho toàn bộ MCU blocks.
        // Điều này ngăn chặn hiện tượng nháy từng khối (MCU flicker) và dư ảnh ở góc dưới màn hình.
        _display->getTFT()->startWrite();
        _jpeg.decode(0, 0, 0);
        _display->getTFT()->endWrite();

        _jpeg.close();
    }

    _display->releaseSPI();

    return true;
}
