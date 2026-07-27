#include "MediaPlayer.h"
#include "NandStorage.h"
#include "DisplayDriver.h"
#include "config.h"

// ============================================================================
// MediaPlayer Implementation — VJPG/VIMG from NAND Flash
// ============================================================================

// Static pointer cho JPEGDEC callback (JPEGDEC không hỗ trợ user data pointer)
static DisplayDriver* s_display = nullptr;

MediaPlayer::~MediaPlayer() {
    stop();
}

int MediaPlayer::jpegDrawCallback(JPEGDRAW* pDraw) {
    if (s_display) s_display->pushImage(pDraw->x, pDraw->y, pDraw->iWidth, pDraw->iHeight, pDraw->pPixels);
    return 1;
}

bool MediaPlayer::init(NandStorage* nand, DisplayDriver* display) {
    _nand = nand;
    _display = display;
    s_display = display;
    _state = PlaybackState::IDLE;
    return true;
}

bool MediaPlayer::playSlot(uint8_t slot) {
    if (_nand == nullptr || _display == nullptr || !_nand->isSlotValid(slot)) {
        _state = PlaybackState::ERROR;
        return false;
    }

    stop();

    if (_jpegBuffer == nullptr) {
        _jpegBuffer = (uint8_t*)malloc(JPEG_BUFFER_SIZE);
        if (_jpegBuffer == nullptr) {
            Serial.println(F("[MediaPlayer] ERROR: Failed to allocate JPEG buffer"));
            _state = PlaybackState::ERROR;
            return false;
        }
    }

    SlotEntry info = _nand->getSlotInfo(slot);
    _fps = info.fps;
    _totalFrames = info.totalFrames;
    _currentFrame = 0;
    _currentSlot = slot;

    if (!_nand->openSlot(slot)) {
        _state = PlaybackState::ERROR;
        return false;
    }

    _display->turnOn();
    _display->clear();
    _display->setBacklight(BACKLIGHT_DAY_PERCENT);

    if (_nand->isSlotImage(slot)) {
        _state = PlaybackState::SHOWING;
        decodeOneFrame();
    } else {
        _state = PlaybackState::PLAYING;
    }

    return true;
}

void MediaPlayer::update() {
    if (_state == PlaybackState::PLAYING) {
        uint32_t frameStart = millis();

        if (!decodeOneFrame()) {
            _nand->seekSlot(0);
            _currentFrame = 0;
            return;
        }

        _currentFrame++;
        if (_currentFrame >= _totalFrames) {
            _nand->seekSlot(0);
            _currentFrame = 0;
        }

        uint32_t elapsed = millis() - frameStart;
        uint32_t targetMs = (_fps > 0) ? (1000 / _fps) : FRAME_DURATION_MS;
        if (elapsed < targetMs) vTaskDelay(pdMS_TO_TICKS(targetMs - elapsed));
    } else {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void MediaPlayer::stop() {
    if (_state == PlaybackState::PLAYING || _state == PlaybackState::SHOWING) {
        _nand->closeSlot();
    }
    if (_jpegBuffer != nullptr) {
        free(_jpegBuffer);
        _jpegBuffer = nullptr;
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

#include "SystemMonitor.h"

bool MediaPlayer::decodeOneFrame() {
    if (_jpegBuffer == nullptr) return false;

    uint32_t jpegSize = 0;
    int bytesRead = _nand->readData((uint8_t*)&jpegSize, 4);

    if (bytesRead < 4 || jpegSize == 0 || jpegSize > JPEG_BUFFER_SIZE) return false;

    bytesRead = _nand->readData(_jpegBuffer, jpegSize);
    if ((uint32_t)bytesRead < jpegSize) return false;

    if (!_display->acquireSPI()) return false;

    if (_jpeg.openRAM(_jpegBuffer, jpegSize, jpegDrawCallback)) {
        _jpeg.setPixelType(RGB565_LITTLE_ENDIAN);
        LGFX* tft = _display->getTFT();
        tft->startWrite();
        _jpeg.decode(0, 0, 0);

        // Draw temperature overlay on top of video frame
        float tempC = SystemMonitor::getChipTemperature();
        int tempInt = (int)(tempC + 0.5f);
        char tempBuf[16];
        snprintf(tempBuf, sizeof(tempBuf), "%d'C", tempInt);

        tft->setTextDatum(lgfx::bottom_left);
        tft->setTextColor(TFT_WHITE, TFT_BLACK);
        tft->setFont(&fonts::FreeSansBold9pt7b);
        tft->drawString(tempBuf, 8, 234);
        tft->setFont(nullptr);

        tft->endWrite();
        _jpeg.close();
    }

    _display->releaseSPI();
    return true;
}
