#include "MediaPlayer.h"
#include "DisplayDriver.h"
#include "SystemMonitor.h"
#include "config.h"

// ============================================================================
// MediaPlayer Implementation — VJPG/VIMG via IStorageProvider
// ============================================================================

static DisplayDriver* s_display = nullptr;

MediaPlayer::~MediaPlayer() {
    stop();
}

int MediaPlayer::jpegDrawCallback(JPEGDRAW* pDraw) {
    if (s_display) s_display->pushImage(pDraw->x, pDraw->y, pDraw->iWidth, pDraw->iHeight, pDraw->pPixels);
    return 1;
}

bool MediaPlayer::init(IStorageProvider* storage, DisplayDriver* display) {
    _storage = storage;
    _display = display;
    s_display = display;
    _state = PlaybackState::IDLE;
    return true;
}

bool MediaPlayer::playSlot(uint8_t slot) {
    char slotStr[16];
    snprintf(slotStr, sizeof(slotStr), "%d", slot);
    return playItem(slotStr);
}

bool MediaPlayer::playItem(const char* identifier) {
    if (_storage == nullptr || _display == nullptr || identifier == nullptr) {
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

    if (!_storage->openForRead(identifier)) {
        _state = PlaybackState::ERROR;
        return false;
    }

    StorageItemInfo info = _storage->getItemInfo(identifier);
    if (info.type == StorageItemType::EMPTY) {
        _storage->closeRead();
        _state = PlaybackState::ERROR;
        return false;
    }

    _fps = (info.fps > 0) ? info.fps : 10;
    _totalFrames = info.totalFrames;
    _currentFrame = 0;
    strncpy(_currentId, identifier, sizeof(_currentId) - 1);
    _currentSlot = (identifier[0] >= '0' && identifier[0] <= '9') ? atoi(identifier) : -1;

    _display->turnOn();
    _display->clear();
    _display->setBacklight(BACKLIGHT_DAY_PERCENT);

    if (info.type == StorageItemType::IMAGE) {
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
            _storage->seek(0);
            _currentFrame = 0;
            return;
        }

        _currentFrame++;
        if (_totalFrames > 0 && _currentFrame >= _totalFrames) {
            _storage->seek(0);
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
        _storage->closeRead();
    }
    if (_jpegBuffer != nullptr) {
        free(_jpegBuffer);
        _jpegBuffer = nullptr;
    }
    _state = PlaybackState::IDLE;
    _currentSlot = -1;
    _currentId[0] = '\0';
    _currentFrame = 0;
}

PlaybackState MediaPlayer::getState() const {
    return _state;
}

int8_t MediaPlayer::getCurrentSlot() const {
    return _currentSlot;
}

bool MediaPlayer::decodeOneFrame() {
    if (_jpegBuffer == nullptr || _storage == nullptr) return false;

    // 1. Đọc kích thước khung hình JPEG (4 bytes header)
    uint32_t jpegSize = 0;
    if (_storage->readData((uint8_t*)&jpegSize, 4) < 4 || jpegSize == 0 || jpegSize > JPEG_BUFFER_SIZE) {
        return false;
    }

    // 2. Đọc toàn bộ dữ liệu JPEG vào RAM buffer trong 1 lệnh duy nhất
    if ((uint32_t)_storage->readData(_jpegBuffer, jpegSize) < jpegSize) {
        return false;
    }

    // 3. Khóa bus SPI và giải mã trực tiếp lên màn hình
    if (!_display->acquireSPI()) return false;

    if (_jpeg.openRAM(_jpegBuffer, jpegSize, jpegDrawCallback)) {
        _jpeg.setPixelType(RGB565_LITTLE_ENDIAN);
        LGFX* tft = _display->getTFT();
        
        tft->startWrite(); // Khóa giao dịch SPI với ST7789 để đẩy toàn bộ block MCU siêu mượt
        _jpeg.decode(0, 0, 0);
        tft->endWrite();
        
        _jpeg.close();
    }

    _display->releaseSPI();
    return true;
}

