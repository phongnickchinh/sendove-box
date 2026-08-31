#include "MediaPlayer.h"
#include "DisplayDriver.h"
#include "SystemMonitor.h"
#include "config.h"
#include "ScreenLogger.h"

// ============================================================================
// MediaPlayer Implementation — VJPG/VIMG via IStorageProvider
// ============================================================================

static DisplayDriver* s_display = nullptr;

static void dumpHexBytes(const char* tag, const uint8_t* data, size_t len) {
    if (tag == nullptr || data == nullptr || len == 0) return;
}

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

    DLOG("[PLAY] req: %s", identifier);

    if (_jpegBuffer == nullptr) {
        _jpegBuffer = (uint8_t*)malloc(JPEG_BUFFER_SIZE);
        if (_jpegBuffer == nullptr) {
            DLOG("[PLAY] err: JPEG buf alloc");
            _state = PlaybackState::ERROR;
            if (_playerMutex) xSemaphoreGiveRecursive(_playerMutex);
            return false;
        }
    }

    if (!_storage->openForRead(identifier)) {
        if (_display) { _display->showMessage("Slot Open FAIL!"); delay(2000); }
        DLOG("[PLAY] err: open %s", identifier);
        _state = PlaybackState::ERROR;
        if (_playerMutex) xSemaphoreGiveRecursive(_playerMutex);
        return false;
    }

    StorageItemInfo info = _storage->getItemInfo(identifier);
    DLOG("[PLAY] slot%d type=%d size=%lu", atoi(identifier), (int)info.type, (unsigned long)info.dataSize);
    _currentDataSize = info.dataSize;
    _currentAudioSize = info.audioSize;
    _frameBaseOffset = 0;
    _readFrameSizeHeader = true;

    if (_currentDataSize == 0) {
        DLOG("[PLAY] warn: dataSize=0");
    }

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
    dumpHexBytes("[MediaPlayer] Header dump:", hdrCheck, sizeof(hdrCheck));

    if (memcmp(hdrCheck + 4, "SLBX", 4) == 0) {
        uint8_t mediaType    = hdrCheck[9];  // type: 0x01=video, 0x02=image
        uint16_t w           = 0;
        uint16_t h           = 0;
        uint8_t  fps         = hdrCheck[14]; // 1 byte
        uint16_t totalFrames = 0;
        memcpy(&w,           hdrCheck + 10, sizeof(w));
        memcpy(&h,           hdrCheck + 12, sizeof(h));
        memcpy(&totalFrames, hdrCheck + 15, sizeof(totalFrames));

        _frameBaseOffset = 20;
        _fps             = (fps > 0) ? fps : 15;
        _totalFrames     = (totalFrames > 0) ? totalFrames : 1;

        // Tự động phát hiện payload là JPEG hay Raw RGB565 bằng cách peek 7 bytes tại offset 20
        // JPEG format: [4-byte size][FF D8 FF ...]  → bytes[4..6] == JPEG magic
        // RGB565 format: pixel data thô, không có JPEG magic
        uint8_t peek[7] = {0};
        _storage->seek(20);
        _storage->readData(peek, sizeof(peek));
        _storage->seek(20); // Rewind về đầu payload

        const bool isJpegPayload = (peek[4] == 0xFF && peek[5] == 0xD8 && peek[6] == 0xFF);

        if (isJpegPayload) {
            _isSlbxRgb565        = false;
            _readFrameSizeHeader = true;
            DLOG("[PLAY] SLBX JPEG: %d fps", _fps);
        } else {
            _isSlbxRgb565        = true;
            _readFrameSizeHeader = false;
            _slbxWidth           = (w > 0) ? w : 128;
            _slbxHeight          = (h > 0) ? h : 160;
            DLOG("[PLAY] SLBX RGB565");
        }
    } else if (memcmp(hdrCheck + 4, "SLOT", 4) == 0 ||
               memcmp(hdrCheck + 4, "VJPG", 4) == 0 ||
               memcmp(hdrCheck + 4, "VIMG", 4) == 0) {
        // Tệp container pre-encoded: Skip 4-byte reserve + 16-byte container header -> Seek đến offset 20!
        _frameBaseOffset = 20;
        _readFrameSizeHeader = true;
        _storage->seek(20);
    } else {
        // Tệp Raw JPEG: Seek về offset 0 để đọc 4-byte frame size header
        _frameBaseOffset = 0;
        _readFrameSizeHeader = true;
        _storage->seek(0);
    }

    DLOG("[PLAY] setup OK: frames=%d", _totalFrames);

    // Khởi tạo I2S nếu chưa có (chỉ init 1 lần trong vòng đời MediaPlayer)
    if (!_audio.isInitialized()) {
        _audio.init();
    }

    // Tìm audio (AUDC header) ngay sau phần video data
    // _currentDataSize là kích thước video (từ SlotEntry.dataSize)
    // AUDC header nằm ở offset _currentDataSize tính từ đầu slot
    bool hasAudio = _audio.loadFromStorage(_storage, _currentDataSize, _currentAudioSize);
    if (hasAudio) {
        _audio.prefill(); // Nạp 2 DMA buffer trước để tránh tiếng click đầu bài
    }

    // Seek về đầu phần video để bắt đầu phát
    _storage->seek(_frameBaseOffset);

    if (info.type == StorageItemType::IMAGE) {
        _state = PlaybackState::SHOWING;
        decodeOneFrame();
    } else {
        _state = PlaybackState::PLAYING;
        _nextFrameDeadline = millis();
    }

    if (_playerMutex) xSemaphoreGiveRecursive(_playerMutex);
    return true;
}

void MediaPlayer::update() {
    if (_playerMutex) xSemaphoreTakeRecursive(_playerMutex, portMAX_DELAY);

    if (_state == PlaybackState::PLAYING) {
        // 1. Tick audio TRƯỚC decode JPEG — nạp DMA buffer nếu cần (non-blocking)
        _audio.tick();

        if (!decodeOneFrame()) {
            _storage->seek(_frameBaseOffset);
            _currentFrame = 0;
            _nextFrameDeadline = millis();
            if (_playerMutex) xSemaphoreGiveRecursive(_playerMutex);
            return;
        }

        _currentFrame++;
        if (_totalFrames > 0 && _currentFrame >= _totalFrames) {
            _storage->seek(_frameBaseOffset);
            _currentFrame = 0;
            // Restart audio khi video loop về đầu
            if (_audio.hasAudio()) {
                _audio.loadFromStorage(_storage, _currentDataSize, _currentAudioSize);
                _audio.prefill();
                _storage->seek(_frameBaseOffset);
            }
            _nextFrameDeadline = millis();
        }

        uint32_t targetMs = (_fps > 0) ? (1000 / _fps) : FRAME_DURATION_MS;

        // Pacer cộng dồn mốc thay vì "ngủ nếu còn dư": I2S chạy bằng clock phần
        // cứng và không bao giờ chờ, nên với cách cũ mỗi frame chậm đẩy video
        // tụt lại vĩnh viễn so với audio. Cộng dồn thì một frame chậm được bù
        // ngay ở frame sau, miễn là trung bình còn dưới ngân sách.
        _nextFrameDeadline += targetMs;
        uint32_t now = millis();
        int32_t remain = (int32_t)(_nextFrameDeadline - now);

        // Trễ quá 4 frame nghĩa là phần cứng không theo kịp thật sự; bám lại mốc
        // hiện tại để khỏi chạy đuổi vô hạn (chỉ ăn CPU mà không đuổi kịp).
        // Mỗi lần bám lại là một khoảng trôi bị xoá đi -> video tụt sau audio
        // đúng bằng chừng ấy. Ghi log để biết ngay: không dòng nào = throughput
        // đủ, còn in ra đều đều nghĩa là đọc frame vẫn chậm hơn ngân sách.
        if (remain < -(int32_t)(targetMs * 4)) {
            DLOG("[PLAY] late resync: -%ldms", (long)(-remain));
            _nextFrameDeadline = now;
        }

        if (_playerMutex) xSemaphoreGiveRecursive(_playerMutex);
        if (remain > 0) vTaskDelay(pdMS_TO_TICKS((uint32_t)remain));
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
    // Dừng I2S audio
    _audio.stop();
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

void MediaPlayer::testAudioBeep() {
    _audio.testBeep();
}

PlaybackState MediaPlayer::getState() const {
    return _state;
}

int8_t MediaPlayer::getCurrentSlot() const {
    return _currentSlot;
}

bool MediaPlayer::decodeOneFrame() {
    if (_jpegBuffer == nullptr || _storage == nullptr) return false;

    // Serial.printf("[MediaPlayer] decodeOneFrame: slot=%d state=%d totalFrames=%u baseOffset=%lu readHeader=%d\n",
    //               _currentSlot, (int)_state, _totalFrames, (unsigned long)_frameBaseOffset,
    //               _readFrameSizeHeader ? 1 : 0);

    // XỬ LÝ 1: Tệp SLBX Raw RGB565 (Render trực tiếp khung hình pixel lên LCD không qua JPEGDEC)
    if (_isSlbxRgb565) {
        uint32_t bytesPerLine = _slbxWidth * 2;
        uint32_t linesPerChunk = JPEG_BUFFER_SIZE / bytesPerLine;
        if (linesPerChunk == 0) linesPerChunk = 1; // Safeguard

        int x = (SCREEN_WIDTH > _slbxWidth) ? (SCREEN_WIDTH - _slbxWidth) / 2 : 0;
        int y = (SCREEN_HEIGHT > _slbxHeight) ? (SCREEN_HEIGHT - _slbxHeight) / 2 : 0;
        int currentY = y;

        uint32_t remainingLines = _slbxHeight;
        while (remainingLines > 0) {
            uint32_t linesToRead = (remainingLines > linesPerChunk) ? linesPerChunk : remainingLines;
            uint32_t bytesToRead = linesToRead * bytesPerLine;

            // Đọc NAND trước (SPI chưa bị khóa bởi LCD)
            int readBytes = _storage->readData(_jpegBuffer, bytesToRead);
            if ((uint32_t)readBytes < bytesToRead) {
                DLOG("[PLAY] RGB short read");
                return false;
            }

            // Lấy SPI mutex chỉ trong lúc push lên LCD
            if (!_display->acquireSPI()) return false;
            _display->pushImage(x, currentY, _slbxWidth, linesToRead, (const uint16_t*)_jpegBuffer);
            _display->releaseSPI();

            currentY += linesToRead;
            remainingLines -= linesToRead;
        }

        return true;
    }

    // XỬ LÝ 2: Tệp JPEG / MJPEG (Giải mã qua JPEGDEC)
    uint32_t jpegSize = 0;

    if (_readFrameSizeHeader) {
        // 1. Đọc kích thước khung hình JPEG (4 bytes header)
        uint8_t sizeBytes[4] = {0};
        int sizeRead = _storage->readData(sizeBytes, sizeof(sizeBytes));
        if (sizeRead < 4) {
            DLOG("[PLAY] ERR: jpeg size short");
            return false;
        }
        memcpy(&jpegSize, sizeBytes, sizeof(jpegSize));

        if (jpegSize == 0 || jpegSize > JPEG_BUFFER_SIZE) {
            DLOG("[PLAY] ERR: BAD jpegSize");
            dumpHexBytes("[MediaPlayer] bad-size bytes:", sizeBytes, sizeof(sizeBytes));
            if (_display) {
                char dbg[40];
                snprintf(dbg, sizeof(dbg), "Bad jpegSize: %lu", (unsigned long)jpegSize);
                _display->showMessage(dbg);
                delay(2000);
            }
            return false;
        }
    } else {
        if (_currentDataSize <= _frameBaseOffset) {
            DLOG("[PLAY] ERR: invalid 1-frame");
            return false;
        }

        jpegSize = _currentDataSize - _frameBaseOffset;
        if (jpegSize > JPEG_BUFFER_SIZE) jpegSize = JPEG_BUFFER_SIZE;
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

        // Serial.printf("[MediaPlayer] JPEG decode result=%d size=%lu\n", decodeRes, (unsigned long)jpegSize);
        _jpeg.close();
    } else {
        DLOG("[PLAY] ERR: openRAM");
    }

    _display->releaseSPI();
    return true;
}
