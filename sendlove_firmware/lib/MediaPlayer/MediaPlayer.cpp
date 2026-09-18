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

// ============================================================================
// ASCII-fold tiếng Việt (tạm thời, chờ phase font Unicode thật) — bỏ dấu để
// hiển thị được bằng font ChakraPetch_* hiện chỉ có glyph ASCII 32-126.
// ============================================================================

// Khối Vietnamese Unicode U+1EA0-1EF9 (và 4 cặp Latin Extended-A Ă/Đ/Ơ/Ư) đều
// xen kẽ chẵn=hoa/lẻ=thường trong từng khối liên tục -> chỉ cần base letter.
struct AsciiFoldAltRange { uint16_t start; uint16_t end; char base; };
static const AsciiFoldAltRange ASCII_FOLD_ALT_RANGES[] = {
    {0x1EA0, 0x1EB7, 'A'}, {0x1EB8, 0x1EC7, 'E'}, {0x1EC8, 0x1ECB, 'I'},
    {0x1ECC, 0x1EE3, 'O'}, {0x1EE4, 0x1EF1, 'U'}, {0x1EF2, 0x1EF9, 'Y'},
    {0x0102, 0x0103, 'A'}, {0x0110, 0x0111, 'D'}, {0x01A0, 0x01A1, 'O'}, {0x01AF, 0x01B0, 'U'},
};

// Latin-1 Supplement: mỗi khối cùng 1 case (hoa/thường tách khối riêng), map
// thẳng ra 1 ký tự cố định, không cần tính chẵn/lẻ.
struct AsciiFoldFlatRange { uint16_t start; uint16_t end; char out; };
static const AsciiFoldFlatRange ASCII_FOLD_FLAT_RANGES[] = {
    {0x00C0, 0x00C3, 'A'}, {0x00E0, 0x00E3, 'a'},
    {0x00C8, 0x00CA, 'E'}, {0x00E8, 0x00EA, 'e'},
    {0x00CC, 0x00CD, 'I'}, {0x00EC, 0x00ED, 'i'},
    {0x00D2, 0x00D5, 'O'}, {0x00F2, 0x00F5, 'o'},
    {0x00D9, 0x00DA, 'U'}, {0x00F9, 0x00FA, 'u'},
    {0x00DD, 0x00DD, 'Y'}, {0x00FD, 0x00FD, 'y'},
};

// Decode 1 ký tự UTF-8 (1-3 byte, đủ cho toàn bộ range tiếng Việt) thành codepoint.
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
    // Chuỗi UTF-8 lỗi hoặc 4-byte (ngoài phạm vi tiếng Việt) -> bỏ qua an toàn.
    *outBytesConsumed = 1;
    return 0xFFFF;
}

// Trả về 0 nếu không map được (ký tự bị bỏ qua khi ghép chuỗi kết quả).
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

/// Bỏ dấu tiếng Việt (UTF-8 -> ASCII gần đúng nhất). Ký tự không map được bị
/// bỏ qua hoàn toàn (không chèn '?' rác vào caption).
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
    // _jpegBuffer (32KB) được cấp phát on-demand trong playItem() và giải phóng
    // ngay trong stop() để trả lại toàn bộ heap cho MbedTLS SSL Handshake lúc Standby.
    // I2S cũng vậy: KHÔNG init ở đây nữa (2026-09-18). playItem() tự init khi cần,
    // beep() cũng thế; init sẵn từ boot nghĩa là giữ 24KB DMA suốt đời máy.
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
            // Lần 1 thất bại: nhường CPU 100ms cho IDLE task dọn dẹp task vừa xóa (WakeSync 12KB)
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

    // Tin nhắn tĩnh KHÔNG có ảnh thật (chỉ audio/text): NetworkManager ghi slot
    // rỗng với dataSize=4 sentinel (xem checkAndDownloadNewMessages()). Bỏ qua
    // hoàn toàn bước dò header SLBX + decodeOneFrame() bên dưới — nếu cứ chạy sẽ
    // đọc trúng vùng NAND chưa từng ghi (0xFF) và báo lỗi "Bad jpegSize" +
    // delay(2000) chặn màn hình vô ích.
    bool isStaticNoImage = (info.type == StorageItemType::IMAGE && _currentDataSize <= 4);

    _display->turnOn();
    _display->clear();
    _display->setBacklight(BACKLIGHT_DAY_PERCENT);

    _isSlbxRgb565 = false;

    if (isStaticNoImage) {
        _frameBaseOffset = 0;
        _readFrameSizeHeader = true;
    } else {
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
    } // end else (!isStaticNoImage) — đóng nhánh dò header SLBX/SLOT/VJPG/VIMG

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
        if (isStaticNoImage) {
            // Đã _display->clear() ở trên -> giữ nguyên màn đen (bản NAND).
            // Bản SD card sẽ có background mặc định riêng ở phase sau.
        } else {
            decodeOneFrame(false);
        }

        // Caption text (nếu có) — bỏ dấu tiếng Việt tạm thời rồi word-wrap vẽ đè.
        char rawCaption[300] = "";
        if (_storage->getItemText(identifier, rawCaption, sizeof(rawCaption))) {
            char asciiCaption[300];
            asciiFoldVietnamese(rawCaption, asciiCaption, sizeof(asciiCaption));
            if (isStaticNoImage) {
                // Không có ảnh: caption chiếm gần trọn màn hình.
                _display->showWrappedText(asciiCaption, 8, 8, SCREEN_WIDTH - 16, SCREEN_HEIGHT - 16);
            } else {
                // Có ảnh: dải chữ ở 1/3 dưới màn hình, không đè lên phần trên.
                int32_t bandY = (SCREEN_HEIGHT * 2) / 3;
                _display->showWrappedText(asciiCaption, 8, bandY, SCREEN_WIDTH - 16, SCREEN_HEIGHT - bandY - 8);
            }
            // ĐO, chưa sửa. showWrappedText() cấp char lines[16][48] = 768B trên stack,
            // trong khi TASK_STACK_MEDIA_PLAYER từng bị hạ 8192->6144 mà CHƯA HỀ ĐO
            // (MEMORY.md §9.7). Đây là đường dùng stack sâu nhất của task này, nên đo
            // ngay tại đây. Chỉ refactor sang 2 lượt nếu con số này < ~1024.
            // Trên ESP-IDF hàm trả về BYTES (không phải words như FreeRTOS gốc) —
            // đọc thẳng, đừng nhân 4.
            DLOG("[PLAY] stack hwm=%u", (unsigned)uxTaskGetStackHighWaterMark(nullptr));
        }
    } else {
        _state = PlaybackState::PLAYING;
        ScreenLogger::setOverlayEnabled(false); // Tắt render log overlay lên LCD để giải phóng 100% SPI cho video
        uint32_t targetMs = (_fps > 0) ? (1000 / _fps) : FRAME_DURATION_MS;
        _nextFrameDeadline = millis() + targetMs;
        _lastFrameSkipped = false;
    }

    if (_playerMutex) xSemaphoreGiveRecursive(_playerMutex);
    return true;
}
void MediaPlayer::update() {
    if (_playerMutex) xSemaphoreTakeRecursive(_playerMutex, portMAX_DELAY);

    if (_state == PlaybackState::PLAYING) {
        // 1. Tick audio trước decode (nạp đầy DMA 192ms)
        _audio.tick();

        uint32_t targetMs = (_fps > 0) ? (1000 / _fps) : FRAME_DURATION_MS;

        // Đã trễ mốc của chính frame này => bỏ render nó.
        // Cách cũ là giải mã dồn hai frame dính liền nhau không nghỉ: đọc NAND +
        // JPEGDEC + đẩy nguyên frame qua SPI ở 100% CPU, tạo đỉnh dòng chồng đúng
        // lúc ampli đang kéo dòng -> sụt áp -> rè tiếng + nhấp nháy đèn nền.
        // Bỏ frame thì RẺ hơn giải mã, nên khi trễ máy tiêu thụ ÍT đi chứ không
        // nhiều lên, mà nhịp hình vẫn bám đúng mốc thời gian của audio.
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

        // 2. Tick audio ngay sau decode & render để bù lượng DMA vừa tiêu thụ
        _audio.tick();

        _currentFrame++;
        if (_totalFrames > 0 && _currentFrame >= _totalFrames) {
            _storage->seek(_frameBaseOffset);
            _currentFrame = 0;
            _lastFrameSkipped = false;
            // Restart audio khi video loop về đầu
            if (_audio.hasAudio()) {
                _audio.loadFromStorage(_storage, _currentDataSize, _currentAudioSize);
                _audio.prefill();
                _storage->seek(_frameBaseOffset);
            }
            _nextFrameDeadline = millis() + targetMs;
        }

        // Pacer cộng dồn mốc thay vì "ngủ nếu còn dư": I2S chạy bằng clock phần
        // cứng và không bao giờ chờ, nên với cách cũ mỗi frame chậm đẩy video
        // tụt lại vĩnh viễn so với audio. Cộng dồn thì một frame chậm được bù
        // ngay ở frame sau, miễn là trung bình còn dưới ngân sách.
        _nextFrameDeadline += targetMs;
        uint32_t now = millis();
        int32_t remain = (int32_t)(_nextFrameDeadline - now);

        // Trễ quá 4 frame nghĩa là phần cứng không theo kịp thật sự; bám lại mốc
        // hiện tại để khỏi chạy đuổi vô hạn (chỉ ăn CPU mà không đuổi kịp).
        if (remain < -(int32_t)(targetMs * 4)) {
            _nextFrameDeadline = now + targetMs;
            remain = 0;
        }

        // Luôn chừa lại một khoảng nghỉ: kể cả khi bỏ frame vẫn chưa bù đủ,
        // không bao giờ được chạy hai lần decode dính liền nhau.
        if (remain < (int32_t)FRAME_MIN_IDLE_MS) remain = (int32_t)FRAME_MIN_IDLE_MS;

        if (_playerMutex) xSemaphoreGiveRecursive(_playerMutex);
        vTaskDelay(pdMS_TO_TICKS((uint32_t)remain));
    } else if (_state == PlaybackState::SHOWING) {
        // Ảnh tĩnh / tin nhắn tĩnh không có frame nào để decode, nhưng vẫn có
        // thể có audio (voice/nhạc nền) đi kèm. Trước đây nhánh này không bao
        // giờ tick audio (chỉ PLAYING mới tick) -> combo ảnh+voice bị câm tiếng
        // dù NetworkManager đã tải đúng. Tick định kỳ ở đây đủ nhanh so với độ
        // sâu DMA (~192ms) để không bao giờ bị underrun.
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
    _audio.stop();
    // Giải phóng _jpegBuffer để hoàn trả 32KB cho heap lúc Standby / TLS Handshake
    if (_jpegBuffer != nullptr) {
        free(_jpegBuffer);
        _jpegBuffer = nullptr;
    }
    _currentSlot = -1;
    _currentId[0] = '\0';
    _currentFrame = 0;
    ScreenLogger::setOverlayEnabled(true);

    if (_playerMutex) xSemaphoreGiveRecursive(_playerMutex);
}

void MediaPlayer::testAudioBeep() {
    _audio.testBeep();
}

void MediaPlayer::alarmBeep() {
    _audio.beep(400);
}

PlaybackState MediaPlayer::getState() const {
    return _state;
}

int8_t MediaPlayer::getCurrentSlot() const {
    return _currentSlot;
}

bool MediaPlayer::decodeOneFrame(bool skipRender) {
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

    // Đã đọc xong dữ liệu nên con trỏ file đã đúng vị trí frame kế; thoát sớm
    // để bỏ đúng phần đắt nhất (JPEGDEC + đẩy full frame qua SPI).
    if (skipRender) return true;

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
