#include "MediaPlayer.h"

#include <new>  // std::nothrow for the heap-allocated JPEGDEC

#include "DisplayDriver.h"
#include "SystemMonitor.h"
#include "config.h"
#include "ScreenLogger.h"
#include "Settings.h"
#include "SdStore.h"

// ============================================================================
// MediaPlayer Implementation — VJPG/VIMG via IStorageProvider
// ============================================================================

static DisplayDriver* s_display = nullptr;

static void dumpHexBytes(const char* tag, const uint8_t* data, size_t len) {
    if (tag == nullptr || data == nullptr || len == 0) return;
}

// ============================================================================
// Vietnamese ASCII folding (temporary, until a real Unicode font) — strips
// diacritics so text renders with FreeSansBold9pt7b (ASCII 32-126 glyphs only).
// ============================================================================

// The Vietnamese block U+1EA0-1EF9 (and the 4 Latin Extended-A pairs Ă/Đ/Ơ/Ư)
// alternates even=upper/odd=lower within each run -> only the base letter is needed.
struct AsciiFoldAltRange { uint16_t start; uint16_t end; char base; };
static const AsciiFoldAltRange ASCII_FOLD_ALT_RANGES[] = {
    {0x1EA0, 0x1EB7, 'A'}, {0x1EB8, 0x1EC7, 'E'}, {0x1EC8, 0x1ECB, 'I'},
    {0x1ECC, 0x1EE3, 'O'}, {0x1EE4, 0x1EF1, 'U'}, {0x1EF2, 0x1EF9, 'Y'},
    {0x0102, 0x0103, 'A'}, {0x0110, 0x0111, 'D'}, {0x01A0, 0x01A1, 'O'}, {0x01AF, 0x01B0, 'U'},
};

// Latin-1 Supplement: each run is a single case (upper/lower are separate runs),
// so it maps straight to one fixed character, no even/odd logic.
struct AsciiFoldFlatRange { uint16_t start; uint16_t end; char out; };
static const AsciiFoldFlatRange ASCII_FOLD_FLAT_RANGES[] = {
    {0x00C0, 0x00C3, 'A'}, {0x00E0, 0x00E3, 'a'},
    {0x00C8, 0x00CA, 'E'}, {0x00E8, 0x00EA, 'e'},
    {0x00CC, 0x00CD, 'I'}, {0x00EC, 0x00ED, 'i'},
    {0x00D2, 0x00D5, 'O'}, {0x00F2, 0x00F5, 'o'},
    {0x00D9, 0x00DA, 'U'}, {0x00F9, 0x00FA, 'u'},
    {0x00DD, 0x00DD, 'Y'}, {0x00FD, 0x00FD, 'y'},
};

// Decodes one UTF-8 character (1-3 bytes, enough for all of Vietnamese) to a codepoint.
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
    // Malformed or 4-byte UTF-8 (outside Vietnamese) -> skip safely.
    *outBytesConsumed = 1;
    return 0xFFFF;
}

// Returns 0 when unmappable (the character is dropped from the output).
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

/// Strips Vietnamese diacritics (UTF-8 -> closest ASCII). Unmappable characters
/// are dropped entirely (no stray '?' in the caption).
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
    // _jpegBuffer (32KB) is allocated on demand in playItem() and freed in stop()
    // to give the heap back to the MbedTLS handshake during standby.
    // Same for I2S: NOT initialised here. playItem() and beep() init it when
    // needed; initialising at boot would hold 24KB of DMA for the device's lifetime.
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
            // First attempt failed: yield 100ms so the IDLE task can reclaim the just-deleted task (WakeSync, 12KB)
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

    // The decoder (17.9KB) shares _jpegBuffer's lifetime — see MediaPlayer.h
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

    // A still message with NO real image (audio/text only): NetworkManager writes
    // an empty slot with the dataSize=4 sentinel (see checkAndDownloadNewMessages()).
    // Skip the SLBX header probe and decodeOneFrame() below entirely — running them
    // would read never-written NAND (0xFF), report "Bad jpegSize" and block the
    // screen with delay(2000) for nothing.
    bool isStaticNoImage = (info.type == StorageItemType::IMAGE && _currentDataSize <= 4);

    _display->turnOn();
    _display->clear();
    _display->setBacklight(Settings::currentBacklight());

    _isSlbxRgb565 = false;

    if (isStaticNoImage) {
        _frameBaseOffset = 0;
        _readFrameSizeHeader = true;
    } else {
    // Detect the container from the header at offset 4 (SLBX / SLOT / VJPG / VIMG)
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

        // Tell JPEG from raw RGB565 by peeking 7 bytes at offset 20
        // JPEG:   [4-byte size][FF D8 FF ...] -> bytes[4..6] == JPEG magic
        // RGB565: raw pixel data, no JPEG magic
        uint8_t peek[7] = {0};
        _storage->seek(20);
        _storage->readData(peek, sizeof(peek));
        _storage->seek(20); // rewind to the start of the payload

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
        // Pre-encoded container: skip the 4-byte prefix + 16-byte container header -> offset 20
        _frameBaseOffset = 20;
        _readFrameSizeHeader = true;
        _storage->seek(20);
    } else {
        // Raw JPEG: seek to offset 0 to read the 4-byte frame size header
        _frameBaseOffset = 0;
        _readFrameSizeHeader = true;
        _storage->seek(0);
    }
    } // end else (!isStaticNoImage)

    DLOG("[PLAY] setup OK: frames=%d", _totalFrames);

    // Init I2S if not already up.
    // Volume is set BEFORE init(): at volume 0, init() leaves the amp off.
    _audio.setVolume(Settings::volume.load(), true);
    if (!_audio.isInitialized()) {
        _audio.init();
    }

    // Audio (AUDC header) sits right after the video data, at offset
    // _currentDataSize (the video size, from SlotEntry.dataSize) from the slot start.
    bool hasAudio = _audio.loadFromStorage(_storage, _currentDataSize, _currentAudioSize);
    if (hasAudio) {
        _audio.prefill(); // fill the DMA first to avoid a click at the start
    }

    // Seek back to the start of the video to begin playback
    _storage->seek(_frameBaseOffset);

    if (info.type == StorageItemType::IMAGE) {
        _state = PlaybackState::SHOWING;
        if (isStaticNoImage) {
            // Already _display->clear()ed above -> keep the black screen (NAND build).
            // The SD card build will get its own default background in a later phase.
        } else {
            decodeOneFrame(false);
        }

        // Caption (if any) — strip Vietnamese diacritics for now, then draw it word-wrapped on top.
        char rawCaption[300] = "";
        if (_storage->getItemText(identifier, rawCaption, sizeof(rawCaption))) {
            char asciiCaption[300];
            asciiFoldVietnamese(rawCaption, asciiCaption, sizeof(asciiCaption));
            if (isStaticNoImage) {
                // No image: the caption takes almost the whole screen.
                _display->showWrappedText(asciiCaption, 8, 8, SCREEN_WIDTH - 16, SCREEN_HEIGHT - 16);
            } else {
                // With an image: a text band in the bottom third, leaving the top clear.
                int32_t bandY = (SCREEN_HEIGHT * 2) / 3;
                _display->showWrappedText(asciiCaption, 8, bandY, SCREEN_WIDTH - 16, SCREEN_HEIGHT - bandY - 8);
            }
            // MEASURE only, not fixed yet. showWrappedText() puts char lines[16][48] =
            // 768B on the stack and TASK_STACK_MEDIA_PLAYER was never measured (see
            // MEMORY.md §9.7). This is the task's deepest stack path, so measure
            // here. Refactor to two passes only if this drops < ~1024.
            // On ESP-IDF the call returns BYTES (not words as in stock FreeRTOS) —
            // read it as is, don't multiply by 4.
            DLOG("[PLAY] stack hwm=%u", (unsigned)uxTaskGetStackHighWaterMark(nullptr));
        }
    } else {
        _state = PlaybackState::PLAYING;
        ScreenLogger::setOverlayEnabled(false); // no log overlay on the LCD: the SPI bus is all for video
        uint32_t targetMs = (_fps > 0) ? (1000 / _fps) : FRAME_DURATION_MS;
        _nextFrameDeadline = millis() + targetMs;
        _lastFrameSkipped = false;
    }

    if (_playerMutex) xSemaphoreGiveRecursive(_playerMutex);
    return true;
}
void MediaPlayer::update() {
    if (_playerMutex) xSemaphoreTakeRecursive(_playerMutex, portMAX_DELAY);

    // Volume changed from the web mid-playback: only the target moves, fillChunk() ramps to it.
    if (_state == PlaybackState::PLAYING || _state == PlaybackState::SHOWING) {
        _audio.setVolume(Settings::volume.load());
    }

    if (_state == PlaybackState::PLAYING) {
        // 1. Tick audio before decoding (tops up the 192ms of DMA)
        _audio.tick();

        uint32_t targetMs = (_fps > 0) ? (1000 / _fps) : FRAME_DURATION_MS;

        // Already past this frame's own deadline => skip rendering it.
        // Catching up by decoding two frames back to back (NAND read + JPEGDEC +
        // a full frame over SPI at 100% CPU) creates a current peak right while the
        // amp is drawing -> voltage sag -> crackling audio + backlight flicker.
        // Skipping a frame is CHEAPER than decoding it, so when late the device
        // draws LESS, not more, and the picture still tracks the audio timeline.
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

        // 2. Tick audio right after decode & render to refill the DMA just consumed
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

        // The pacer accumulates deadlines instead of "sleep if time is left": I2S
        // runs off a hardware clock and never waits, so with per-frame sleeping
        // every slow frame pushes video permanently behind audio. Accumulating
        // lets the next frame make up for a slow one, as long as the average
        // stays within budget.
        _nextFrameDeadline += targetMs;
        uint32_t now = millis();
        int32_t remain = (int32_t)(_nextFrameDeadline - now);

        // More than 4 frames late means the hardware really can't keep up; re-anchor
        // to now instead of chasing forever (burning CPU without catching up).
        if (remain < -(int32_t)(targetMs * 4)) {
            _nextFrameDeadline = now + targetMs;
            remain = 0;
        }

        // Always leave an idle gap: even when skipping frames hasn't caught up,
        // never run two decodes back to back.
        if (remain < (int32_t)FRAME_MIN_IDLE_MS) remain = (int32_t)FRAME_MIN_IDLE_MS;

        if (_playerMutex) xSemaphoreGiveRecursive(_playerMutex);
        vTaskDelay(pdMS_TO_TICKS((uint32_t)remain));
    } else if (_state == PlaybackState::SHOWING) {
        // A still image / still message has no frames to decode but may carry
        // audio (voice / background music). Without a tick in this branch an
        // image+voice message stays silent even though NetworkManager downloaded
        // it correctly. Ticking at this period is fast enough for the DMA depth
        // (~192ms) to never underrun.
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
    // Free _jpegBuffer to return 32KB to the heap for standby / the TLS handshake
    if (_jpegBuffer != nullptr) {
        free(_jpegBuffer);
        _jpegBuffer = nullptr;
    }
    // ...and the decoder's 17.9KB, for the same reason (see MediaPlayer.h)
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
    // Alarm music only needs the I2S DMA (~24KB) + a 3KB buffer; no JPEG decoder is allocated.
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

    // Case 1: SLBX raw RGB565 (pixels pushed straight to the LCD, no JPEGDEC)
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

            // Read storage first (the LCD hasn't locked SPI yet)
            int readBytes = _storage->readData(_jpegBuffer, bytesToRead);
            if ((uint32_t)readBytes < bytesToRead) {
                DLOG("[PLAY] RGB short read");
                return false;
            }

            // Same reason as the JPEG branch below: top up the DMA before locking the bus.
            _audio.tick();

            // Hold the SPI mutex only while pushing to the LCD
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

    // Case 2: JPEG / MJPEG (decoded by JPEGDEC)
    uint32_t jpegSize = 0;

    if (_readFrameSizeHeader) {
        // 1. Read the JPEG frame size (4-byte header)
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

    // 2. Read the whole JPEG into the RAM buffer in a single call
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

    // The data is read, so the file cursor already sits at the next frame; return
    // early to skip exactly the expensive part (JPEGDEC + a full frame over SPI).
    if (skipRender) return true;

    // Top up the DMA RIGHT BEFORE the longest blind stretch. update() only ticks
    // at both ends of a frame, so in between the DMA gets no bytes for the whole
    // (SD read + decode + screen push). The DMA only lasts 192ms for 8kHz files
    // and 96ms for 16kHz (12 × 512 frames, hardware runs x AUDIO_OVERSAMPLE) ->
    // running dry is an audible crackle.
    // HERE rather than after acquireSPI(): tick() reads the card, so it must stay
    // outside the display transaction — the mutex is not recursive.
    _audio.tick();

    // 3. Lock the SPI bus and decode straight to the screen
    if (!_display->acquireSPI()) return false;

    if (_jpeg->openRAM(_jpegBuffer, jpegSize, jpegDrawCallback)) {
        _jpeg->setPixelType(RGB565_LITTLE_ENDIAN);
        LGFX* tft = _display->getTFT();

        tft->startWrite(); // one SPI transaction with the ST7789 for all MCU blocks
        int decodeRes = _jpeg->decode(0, 0, 0);
        tft->endWrite();

        _jpeg->close();
    } else {
        DLOG("[PLAY] ERR: openRAM");
    }

    _display->releaseSPI();
    return true;
}
