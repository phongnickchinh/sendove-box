#include "MediaPlayer.h"

#include <new>  // std::nothrow for the heap-allocated JPEGDEC

#include "DisplayDriver.h"
#include "SystemMonitor.h"
#include "config.h"
#include "ScreenLogger.h"
#include "Settings.h"
#include "SdStore.h"

static DisplayDriver* s_display = nullptr;

static void dumpHexBytes(const char* tag, const uint8_t* data, size_t len) {
    if (tag == nullptr || data == nullptr || len == 0) return;
}

// ---- Vietnamese ASCII folding (temporary: the font only has ASCII 32-126) ----

// U+1EA0-1EF9 and the Ă/Đ/Ơ/Ư pairs alternate even=upper/odd=lower within a run.
struct AsciiFoldAltRange { uint16_t start; uint16_t end; char base; };
static const AsciiFoldAltRange ASCII_FOLD_ALT_RANGES[] = {
    {0x1EA0, 0x1EB7, 'A'}, {0x1EB8, 0x1EC7, 'E'}, {0x1EC8, 0x1ECB, 'I'},
    {0x1ECC, 0x1EE3, 'O'}, {0x1EE4, 0x1EF1, 'U'}, {0x1EF2, 0x1EF9, 'Y'},
    {0x0102, 0x0103, 'A'}, {0x0110, 0x0111, 'D'}, {0x01A0, 0x01A1, 'O'}, {0x01AF, 0x01B0, 'U'},
};

// Latin-1 Supplement: each run is a single case, mapped to one fixed character.
struct AsciiFoldFlatRange { uint16_t start; uint16_t end; char out; };
static const AsciiFoldFlatRange ASCII_FOLD_FLAT_RANGES[] = {
    {0x00C0, 0x00C3, 'A'}, {0x00E0, 0x00E3, 'a'},
    {0x00C8, 0x00CA, 'E'}, {0x00E8, 0x00EA, 'e'},
    {0x00CC, 0x00CD, 'I'}, {0x00EC, 0x00ED, 'i'},
    {0x00D2, 0x00D5, 'O'}, {0x00F2, 0x00F5, 'o'},
    {0x00D9, 0x00DA, 'U'}, {0x00F9, 0x00FA, 'u'},
    {0x00DD, 0x00DD, 'Y'}, {0x00FD, 0x00FD, 'y'},
};

// Decodes one UTF-8 character of 1-3 bytes (covers all of Vietnamese).
static uint16_t decodeUtf8Char(const char* s, size_t remaining, uint8_t* outBytesConsumed) {
    uint8_t b0 = (uint8_t)s[0];
    if (b0 < 0x80) { *outBytesConsumed = 1; return b0; }
    if ((b0 & 0xE0) == 0xC0 && remaining >= 2) {
        *outBytesConsumed = 2;
        return (uint16_t)(((b0 & 0x1F) << 6) | ((uint8_t)s[1] & 0x3F));
    }
    if ((b0 & 0xF0) == 0xE0 && remaining >= 3) {
        *outBytesConsumed = 3;
        return (uint16_t)(((b0 & 0x0F) << 12) | (((uint8_t)s[1] & 0x3F) << 6) | ((uint8_t)s[2] & 0x3F));
    }
    // Malformed or 4-byte UTF-8: skip.
    *outBytesConsumed = 1;
    return 0xFFFF;
}

// 0 = unmappable (dropped).
static char asciiFoldCodepoint(uint16_t cp) {
    if (cp < 0x80) return (char)cp;
    for (const auto& r : ASCII_FOLD_ALT_RANGES) {
        if (cp >= r.start && cp <= r.end) {
            bool isUpper = (((cp - r.start) % 2) == 0);
            return isUpper ? r.base : (char)(r.base + 32);
        }
    }
    for (const auto& r : ASCII_FOLD_FLAT_RANGES) {
        if (cp >= r.start && cp <= r.end) return r.out;
    }
    return 0;
}

/// UTF-8 -> closest ASCII; unmappable characters are dropped.
static void asciiFoldVietnamese(const char* utf8In, char* asciiOut, size_t maxOut) {
    if (asciiOut == nullptr || maxOut == 0) return;
    if (utf8In == nullptr) { asciiOut[0] = '\0'; return; }

    size_t inLen = strlen(utf8In);
    size_t i = 0, o = 0;
    while (i < inLen && o < maxOut - 1) {
        uint8_t bytesConsumed = 1;
        uint16_t cp = decodeUtf8Char(utf8In + i, inLen - i, &bytesConsumed);
        char c = asciiFoldCodepoint(cp);
        if (c != 0) asciiOut[o++] = c;
        i += bytesConsumed;
    }
    asciiOut[o] = '\0';
}

MediaPlayer::~MediaPlayer() {
    stop();
    if (_jpegBuffer != nullptr) {
        free(_jpegBuffer);
        _jpegBuffer = nullptr;
    }
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
    // _jpegBuffer (32KB) and I2S (24KB DMA) are NOT set up here: they are allocated
    // on demand and freed in stop(), leaving the heap to the TLS handshake.
    return true;
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
            // Yield 100ms so the IDLE task can reclaim freed memory, then retry.
            DLOG("[PLAY] JPEG buf retry (yield IDLE)...");
            if (_playerMutex) xSemaphoreGiveRecursive(_playerMutex);
            vTaskDelay(pdMS_TO_TICKS(100));
            if (_playerMutex) xSemaphoreTakeRecursive(_playerMutex, portMAX_DELAY);
            _jpegBuffer = (uint8_t*)malloc(JPEG_BUFFER_SIZE);
        }
        if (_jpegBuffer == nullptr) {
            DLOG("[PLAY] err: JPEG buf alloc fail");
            _state = PlaybackState::ERROR;
            if (_playerMutex) xSemaphoreGiveRecursive(_playerMutex);
            return false;
        }
    }

    // The decoder (17.9KB) shares _jpegBuffer's lifetime.
    if (_jpeg == nullptr) {
        _jpeg = new (std::nothrow) JPEGDEC();
        if (_jpeg == nullptr) {
            DLOG("[PLAY] err: JPEGDEC alloc fail");
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

    // Audio/text-only message: an empty slot with the dataSize=4 sentinel. Skip the
    // header probe and decodeOneFrame(), which would read unwritten storage.
    bool isStaticNoImage = (info.type == StorageItemType::IMAGE && _currentDataSize <= 4);

    _display->turnOn();
    _display->clear();
    _display->setBacklight(Settings::currentBacklight());

    _isSlbxRgb565 = false;

    if (isStaticNoImage) {
        _frameBaseOffset = 0;
        _readFrameSizeHeader = true;
    } else {
    // Container magic at offset 4 (SLBX / SLOT / VJPG / VIMG)
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

        // JPEG payload = [4-byte size][FF D8 FF ...]; anything else is raw RGB565.
        uint8_t peek[7] = {0};
        _storage->seek(20);
        _storage->readData(peek, sizeof(peek));
        _storage->seek(20);

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
        // Container: payload starts after the 4-byte prefix + 16-byte header.
        _frameBaseOffset = 20;
        _readFrameSizeHeader = true;
        _storage->seek(20);
    } else {
        // Raw JPEG: the 4-byte frame size is at offset 0.
        _frameBaseOffset = 0;
        _readFrameSizeHeader = true;
        _storage->seek(0);
    }
    } // end else (!isStaticNoImage)

    DLOG("[PLAY] setup OK: frames=%d", _totalFrames);

    // Volume BEFORE init(): at volume 0, init() leaves the amp off.
    _audio.setVolume(Settings::volume.load(), true);
    if (!_audio.isInitialized()) {
        _audio.init();
    }

    // The AUDC header sits right after the video data, at offset _currentDataSize.
    bool hasAudio = _audio.loadFromStorage(_storage, _currentDataSize, _currentAudioSize);
    if (hasAudio) {
        _audio.prefill(); // avoids a click at the start
    }

    _storage->seek(_frameBaseOffset);

    if (info.type == StorageItemType::IMAGE) {
        _state = PlaybackState::SHOWING;
        if (isStaticNoImage) {
            // Keep the cleared (black) screen.
        } else {
            decodeOneFrame(false);
        }

        // Caption: ASCII-folded, word-wrapped, drawn on top.
        char rawCaption[300] = "";
        if (_storage->getItemText(identifier, rawCaption, sizeof(rawCaption))) {
            char asciiCaption[300];
            asciiFoldVietnamese(rawCaption, asciiCaption, sizeof(asciiCaption));
            if (isStaticNoImage) {
                // No image: nearly the whole screen.
                _display->showWrappedText(asciiCaption, 8, 8, SCREEN_WIDTH - 16, SCREEN_HEIGHT - 16);
            } else {
                // With an image: a band in the bottom third.
                int32_t bandY = (SCREEN_HEIGHT * 2) / 3;
                _display->showWrappedText(asciiCaption, 8, bandY, SCREEN_WIDTH - 16, SCREEN_HEIGHT - bandY - 8);
            }
            // Measurement only: showWrappedText() puts 768B on the stack (MEMORY.md
            // §9.7). Refactor to two passes only if this drops < ~1024. ESP-IDF returns
            // BYTES here, not words.
            DLOG("[PLAY] stack hwm=%u", (unsigned)uxTaskGetStackHighWaterMark(nullptr));
        }
    } else {
        _state = PlaybackState::PLAYING;
        ScreenLogger::setOverlayEnabled(false); // the SPI bus is all for video
        uint32_t targetMs = (_fps > 0) ? (1000 / _fps) : FRAME_DURATION_MS;
        _nextFrameDeadline = millis() + targetMs;
        _lastFrameSkipped = false;
    }

    if (_playerMutex) xSemaphoreGiveRecursive(_playerMutex);
    return true;
}
void MediaPlayer::update() {
    if (_playerMutex) xSemaphoreTakeRecursive(_playerMutex, portMAX_DELAY);

    // Volume changed mid-playback: fillChunk() ramps to the new target.
    if (_state == PlaybackState::PLAYING || _state == PlaybackState::SHOWING) {
        _audio.setVolume(Settings::volume.load());
    }

    if (_state == PlaybackState::PLAYING) {
        // Top up the DMA before decoding
        _audio.tick();

        uint32_t targetMs = (_fps > 0) ? (1000 / _fps) : FRAME_DURATION_MS;

        // Past this frame's deadline => skip rendering it. Decoding two frames back
        // to back to catch up peaks the current while the amp draws (voltage sag,
        // crackle, backlight flicker); skipping is cheaper and keeps A/V in sync.
        bool skipRender = !_lastFrameSkipped &&
                          (int32_t)(_nextFrameDeadline - millis()) < 0;

        if (!decodeOneFrame(skipRender)) {
            _storage->seek(_frameBaseOffset);
            _currentFrame = 0;
            _lastFrameSkipped = false;
            _nextFrameDeadline = millis() + targetMs;
            if (_playerMutex) xSemaphoreGiveRecursive(_playerMutex);
            return;
        }
        _lastFrameSkipped = skipRender;

        // Refill the DMA consumed during decode + render
        _audio.tick();

        _currentFrame++;
        if (_totalFrames > 0 && _currentFrame >= _totalFrames) {
            _storage->seek(_frameBaseOffset);
            _currentFrame = 0;
            _lastFrameSkipped = false;
            // Restart audio when the video loops
            if (_audio.hasAudio()) {
                _audio.loadFromStorage(_storage, _currentDataSize, _currentAudioSize);
                _audio.prefill();
                _storage->seek(_frameBaseOffset);
            }
            _nextFrameDeadline = millis() + targetMs;
        }

        // Accumulated deadlines, not "sleep if time is left": I2S never waits, so a
        // slow frame must be made up by the next one or video drifts behind audio.
        _nextFrameDeadline += targetMs;
        uint32_t now = millis();
        int32_t remain = (int32_t)(_nextFrameDeadline - now);

        // More than 4 frames late: re-anchor to now instead of chasing forever.
        if (remain < -(int32_t)(targetMs * 4)) {
            _nextFrameDeadline = now + targetMs;
            remain = 0;
        }

        // Always leave an idle gap: never two decodes back to back.
        if (remain < (int32_t)FRAME_MIN_IDLE_MS) remain = (int32_t)FRAME_MIN_IDLE_MS;

        if (_playerMutex) xSemaphoreGiveRecursive(_playerMutex);
        vTaskDelay(pdMS_TO_TICKS((uint32_t)remain));
    } else if (_state == PlaybackState::SHOWING) {
        // A still image may carry audio: without this tick it would stay silent.
        // A 50ms period is within the DMA depth (96-192ms).
        if (_audio.hasAudio()) _audio.tick();
        if (_playerMutex) xSemaphoreGiveRecursive(_playerMutex);
        vTaskDelay(pdMS_TO_TICKS(50));
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
    _state = PlaybackState::IDLE;
    _alarmMusic = false;
    _audio.stop();
    // Return the 32KB buffer and the 17.9KB decoder to the heap for the TLS handshake
    if (_jpegBuffer != nullptr) {
        free(_jpegBuffer);
        _jpegBuffer = nullptr;
    }
    if (_jpeg != nullptr) {
        delete _jpeg;
        _jpeg = nullptr;
    }
    _currentSlot = -1;
    _currentId[0] = '\0';
    _currentFrame = 0;
    ScreenLogger::setOverlayEnabled(true);

    if (_playerMutex) xSemaphoreGiveRecursive(_playerMutex);
}

void MediaPlayer::testAudioBeep() {
    _audio.setVolume(Settings::volume.load(), true);
    _audio.testBeep();
}

void MediaPlayer::alarmBeep(uint8_t volume) {
    _audio.setVolume(volume, true);
    _audio.beep(400);
}

bool MediaPlayer::startAlarmMusic(const char* path, uint8_t volume) {
    if (_playerMutex) xSemaphoreTakeRecursive(_playerMutex, portMAX_DELAY);
    _audio.setVolume(volume, true);
    bool ok = _audio.isInitialized() || _audio.init();
    // Alarm music needs only I2S DMA + a 3KB buffer, no JPEG decoder.
    if (ok) ok = _audio.loadFromFile(SdStore::card(), path, true);
    if (ok) {
        _audio.prefill();
        _alarmMusic = true;
    } else {
        _audio.stop();
        DLOG("[ALM] khong mo duoc nhac -> bip");
    }
    if (_playerMutex) xSemaphoreGiveRecursive(_playerMutex);
    return ok;
}

void MediaPlayer::tickAlarmMusic(uint8_t volume) {
    if (!_alarmMusic) return;
    if (_playerMutex) xSemaphoreTakeRecursive(_playerMutex, portMAX_DELAY);
    _audio.setVolume(volume);
    _audio.tick();
    if (_playerMutex) xSemaphoreGiveRecursive(_playerMutex);
}

PlaybackState MediaPlayer::getState() const {
    return _state;
}

int8_t MediaPlayer::getCurrentSlot() const {
    return _currentSlot;
}

bool MediaPlayer::decodeOneFrame(bool skipRender) {
    if (_jpegBuffer == nullptr || _jpeg == nullptr || _storage == nullptr) return false;

    // Case 1: SLBX raw RGB565, pushed straight to the LCD
    if (_isSlbxRgb565) {
        uint32_t bytesPerLine = _slbxWidth * 2;
        uint32_t linesPerChunk = JPEG_BUFFER_SIZE / bytesPerLine;
        if (linesPerChunk == 0) linesPerChunk = 1;

        int x = (SCREEN_WIDTH > _slbxWidth) ? (SCREEN_WIDTH - _slbxWidth) / 2 : 0;
        int y = (SCREEN_HEIGHT > _slbxHeight) ? (SCREEN_HEIGHT - _slbxHeight) / 2 : 0;
        int currentY = y;

        uint32_t remainingLines = _slbxHeight;
        while (remainingLines > 0) {
            uint32_t linesToRead = (remainingLines > linesPerChunk) ? linesPerChunk : remainingLines;
            uint32_t bytesToRead = linesToRead * bytesPerLine;

            int readBytes = _storage->readData(_jpegBuffer, bytesToRead);
            if ((uint32_t)readBytes < bytesToRead) {
                DLOG("[PLAY] RGB short read");
                return false;
            }

            // Top up the DMA before locking the bus (see the JPEG branch).
            _audio.tick();

            if (!skipRender) {
                if (!_display->acquireSPI()) return false;
                _display->pushImage(x, currentY, _slbxWidth, linesToRead, (const uint16_t*)_jpegBuffer);
                _display->releaseSPI();
            }

            currentY += linesToRead;
            remainingLines -= linesToRead;
        }

        return true;
    }

    // Case 2: JPEG / MJPEG
    uint32_t jpegSize = 0;

    if (_readFrameSizeHeader) {
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

    // The cursor already sits at the next frame: skip only the decode + push.
    if (skipRender) return true;

    // Top up the DMA right before the longest stretch without ticks (it lasts only
    // 96ms at 16kHz). BEFORE acquireSPI(): tick() reads the card and the mutex is
    // not recursive.
    _audio.tick();

    if (!_display->acquireSPI()) return false;

    if (_jpeg->openRAM(_jpegBuffer, jpegSize, jpegDrawCallback)) {
        _jpeg->setPixelType(RGB565_LITTLE_ENDIAN);
        LGFX* tft = _display->getTFT();

        tft->startWrite(); // one SPI transaction for all MCU blocks
        int decodeRes = _jpeg->decode(0, 0, 0);
        tft->endWrite();

        _jpeg->close();
    } else {
        DLOG("[PLAY] ERR: openRAM");
    }

    _display->releaseSPI();
    return true;
}
