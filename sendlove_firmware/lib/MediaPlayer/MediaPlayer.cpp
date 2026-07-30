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
    if (_playerMutex != nullptr) {
        vSemaphoreDelete(_playerMutex);
        _playerMutex = nullptr;
    }
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
    if (_playerMutex == nullptr) {
        _playerMutex = xSemaphoreCreateRecursiveMutex();
    }
    return true;
}

bool MediaPlayer::playSlot(uint8_t slot) {
    char slotStr[16];
    snprintf(slotStr, sizeof(slotStr), "%d", slot);
    return playItem(slotStr);
}

bool MediaPlayer::playItem(const char* identifier) {
    if (_playerMutex) xSemaphoreTakeRecursive(_playerMutex, portMAX_DELAY);

    if (_storage == nullptr || _display == nullptr || identifier == nullptr) {
        _state = PlaybackState::ERROR;
        if (_playerMutex) xSemaphoreGiveRecursive(_playerMutex);
        return false;
    }

    stop();

    if (_jpegBuffer == nullptr) {
        _jpegBuffer = (uint8_t*)malloc(JPEG_BUFFER_SIZE);
        if (_jpegBuffer == nullptr) {
            Serial.println(F("[MediaPlayer] ERROR: Failed to allocate JPEG buffer"));
            _state = PlaybackState::ERROR;
            if (_playerMutex) xSemaphoreGiveRecursive(_playerMutex);
            return false;
        }
    }

    if (!_storage->openForRead(identifier)) {
        if (_display) { _display->showMessage("Slot Open FAIL!"); delay(2000); }
        Serial.printf("[MediaPlayer] ERROR: Cannot open slot '%s' for read\n", identifier);
        _state = PlaybackState::ERROR;
        if (_playerMutex) xSemaphoreGiveRecursive(_playerMutex);
        return false;
    }

    StorageItemInfo info = _storage->getItemInfo(identifier);
    Serial.printf("[MediaPlayer] Slot %s info: type=%d size=%lu fps=%u frames=%u\n",
                  identifier, (int)info.type, (unsigned long)info.dataSize, info.fps, info.totalFrames);

    if (info.type == StorageItemType::EMPTY) {
        _storage->closeRead();
        _state = PlaybackState::ERROR;
        if (_playerMutex) xSemaphoreGiveRecursive(_playerMutex);
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

    _isSlbxRgb565 = false;

    // Tự động kiểm tra header tại offset 4 (SLBX / SLOT / VJPG / VIMG)
    uint8_t hdrCheck[20] = {0};
    _storage->readData(hdrCheck, 20);

    if (memcmp(hdrCheck + 4, "SLBX", 4) == 0) {
        uint8_t formatType = hdrCheck[9]; // Byte index 9 trong payload header (index 5 + 4 offset)
        uint16_t w = *(uint16_t*)(hdrCheck + 10);
        uint16_t h = *(uint16_t*)(hdrCheck + 12);

        if (formatType == 2 && w > 0 && h > 0) {
            _isSlbxRgb565 = true;
            _slbxWidth = w;
            _slbxHeight = h;
            _storage->seek(20); // Skip 4-byte offset reserve + 16-byte SLBX header
            Serial.printf("[MediaPlayer] SLBX Raw RGB565 media detected (%dx%d). Seeking to offset 20.\n", w, h);
        } else {
            _storage->seek(20);
            Serial.println(F("[MediaPlayer] SLBX JPEG media detected. Seeking to offset 20."));
        }
    } else if (memcmp(hdrCheck + 4, "SLOT", 4) == 0 ||
               memcmp(hdrCheck + 4, "VJPG", 4) == 0 ||
               memcmp(hdrCheck + 4, "VIMG", 4) == 0) {
        // Tệp container pre-encoded: Skip 4-byte reserve + 16-byte container header -> Seek đến offset 20!
        _storage->seek(20);
        Serial.println(F("[MediaPlayer] Container header detected. Seeking to offset 20 for Frame 1."));
    } else {
        // Tệp Raw JPEG: Seek về offset 0 để đọc 4-byte frame size header
        _storage->seek(0);
    }

    if (info.type == StorageItemType::IMAGE) {
        _state = PlaybackState::SHOWING;
        decodeOneFrame();
    } else {
        _state = PlaybackState::PLAYING;
    }

    if (_playerMutex) xSemaphoreGiveRecursive(_playerMutex);
    return true;
}

void MediaPlayer::update() {
    if (_playerMutex) xSemaphoreTakeRecursive(_playerMutex, portMAX_DELAY);

    if (_state == PlaybackState::PLAYING) {
        uint32_t frameStart = millis();

        if (!decodeOneFrame()) {
            _storage->seek(20);
            _currentFrame = 0;
            if (_playerMutex) xSemaphoreGiveRecursive(_playerMutex);
            return;
        }

        _currentFrame++;
        if (_totalFrames > 0 && _currentFrame >= _totalFrames) {
            _storage->seek(20);
            _currentFrame = 0;
        }

        uint32_t elapsed = millis() - frameStart;
        uint32_t targetMs = (_fps > 0) ? (1000 / _fps) : FRAME_DURATION_MS;
        
        if (_playerMutex) xSemaphoreGiveRecursive(_playerMutex);
        if (elapsed < targetMs) vTaskDelay(pdMS_TO_TICKS(targetMs - elapsed));
    } else {
        if (_playerMutex) xSemaphoreGiveRecursive(_playerMutex);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void MediaPlayer::stop() {
    if (_playerMutex) xSemaphoreTakeRecursive(_playerMutex, portMAX_DELAY);

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

    if (_playerMutex) xSemaphoreGiveRecursive(_playerMutex);
}

PlaybackState MediaPlayer::getState() const {
    return _state;
}

int8_t MediaPlayer::getCurrentSlot() const {
    return _currentSlot;
}

bool MediaPlayer::decodeOneFrame() {
    if (_jpegBuffer == nullptr || _storage == nullptr) return false;

    // XỬ LÝ 1: Tệp SLBX Raw RGB565 (Render trực tiếp khung hình pixel lên LCD không qua JPEGDEC)
    if (_isSlbxRgb565) {
        uint32_t payloadSize = (uint32_t)_slbxWidth * _slbxHeight * 2;
        if (payloadSize > JPEG_BUFFER_SIZE) payloadSize = JPEG_BUFFER_SIZE;

        int readBytes = _storage->readData(_jpegBuffer, payloadSize);
        if ((uint32_t)readBytes < payloadSize) {
            Serial.printf("[MediaPlayer] SLBX RGB565 read short: %d/%u\n", readBytes, payloadSize);
            return false;
        }

        int x = (SCREEN_WIDTH > _slbxWidth) ? (SCREEN_WIDTH - _slbxWidth) / 2 : 0;
        int y = (SCREEN_HEIGHT > _slbxHeight) ? (SCREEN_HEIGHT - _slbxHeight) / 2 : 0;

        if (!_display->acquireSPI()) return false;
        _display->pushImage(x, y, _slbxWidth, _slbxHeight, (const uint16_t*)_jpegBuffer);
        _display->releaseSPI();

        Serial.printf("[MediaPlayer] SLBX RGB565 frame rendered successfully (%dx%d at %d,%d).\n", _slbxWidth, _slbxHeight, x, y);
        return true;
    }

    // XỬ LÝ 2: Tệp JPEG / MJPEG (Giải mã qua JPEGDEC)
    // 1. Đọc kích thước khung hình JPEG (4 bytes header)
    uint32_t jpegSize = 0;
    if (_storage->readData((uint8_t*)&jpegSize, 4) < 4 || jpegSize == 0 || jpegSize > JPEG_BUFFER_SIZE) {
        if (_display) {
            char dbg[32];
            snprintf(dbg, sizeof(dbg), "Bad jpegSz: %lu", (unsigned long)jpegSize);
            _display->showMessage(dbg);
            delay(2000);
        }
        return false;
    }

    // 2. Đọc toàn bộ dữ liệu JPEG vào RAM buffer trong 1 lệnh duy nhất
    int readBytes = _storage->readData(_jpegBuffer, jpegSize);
    if ((uint32_t)readBytes < jpegSize) {
        if (_display) {
            char dbg[40];
            snprintf(dbg, sizeof(dbg), "Read short: %d/%lu", readBytes, (unsigned long)jpegSize);
            _display->showMessage(dbg);
            delay(2000);
        }
        return false;
    }

    // 3. Khóa bus SPI và giải mã trực tiếp lên màn hình
    if (!_display->acquireSPI()) return false;

    if (_jpeg.openRAM(_jpegBuffer, jpegSize, jpegDrawCallback)) {
        _jpeg.setPixelType(RGB565_LITTLE_ENDIAN);
        LGFX* tft = _display->getTFT();

        tft->startWrite(); // Khóa giao dịch SPI với ST7789 để đẩy toàn bộ block MCU siêu mượt
        int decodeRes = _jpeg.decode(0, 0, 0);
        tft->endWrite();

        _jpeg.close();
    } else {
        Serial.println(F("[MediaPlayer] JPEG openRAM failed!"));
    }

    _display->releaseSPI();
    return true;
}
