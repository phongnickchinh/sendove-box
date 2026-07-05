#include "MediaPlayer.h"
#include "SDCardManager.h"
#include "DisplayDriver.h"
#include "config.h"

// ============================================================================
// MediaPlayer Implementation
// ============================================================================

#ifdef WOKWI_SIMULATION
// ============================================================================
// WOKWI SIMULATION MODE — Active Buzzer thay thế I2S Audio
// ============================================================================
// Trong giả lập Wokwi, I2S và MAX98357A không được hỗ trợ.
// Thay vào đó, dùng active buzzer để phát tín hiệu âm thanh đơn giản
// khi có tin nhắn mới.
// ============================================================================

bool MediaPlayer::init(SDCardManager* sdCard, DisplayDriver* display,
                        uint8_t bclkPin, uint8_t lrcPin, uint8_t doutPin) {
    _sdCard  = sdCard;
    _display = display;

    // Cấu hình buzzer pin (dùng chân DOUT cũ)
    _buzzerPin = PIN_BUZZER;
    pinMode(_buzzerPin, OUTPUT);
    digitalWrite(_buzzerPin, LOW); // Tắt ban đầu

    Serial.println(F("[MediaPlayer] WOKWI MODE: Buzzer initialized"));
    _state = PlaybackState::IDLE;
    return true;
}

bool MediaPlayer::playMessage(const char* videoPath, const char* audioPath) {
    if (_display == nullptr) {
        Serial.println(F("[MediaPlayer] ERROR: Display not initialized"));
        _state = PlaybackState::ERROR;
        return false;
    }

    Serial.println(F("[MediaPlayer] WOKWI MODE: Simulating playback..."));
    _state = PlaybackState::PLAYING;

    // Bật màn hình và hiển thị thông báo
    _display->turnOn();
    _display->setBacklight(BACKLIGHT_DAY_PERCENT);
    _display->showMessage("Playing msg...");

    // Bật buzzer kêu để mô phỏng phát âm thanh
    digitalWrite(_buzzerPin, HIGH);
    Serial.println(F("[MediaPlayer] WOKWI: Buzzer ON"));

    // Giữ buzzer kêu trong BUZZER_PLAY_DURATION_MS
    vTaskDelay(pdMS_TO_TICKS(BUZZER_PLAY_DURATION_MS));

    // Tắt buzzer
    digitalWrite(_buzzerPin, LOW);
    Serial.println(F("[MediaPlayer] WOKWI: Buzzer OFF"));

    _state = PlaybackState::IDLE;
    Serial.println(F("[MediaPlayer] WOKWI: Simulated playback complete"));
    return true;
}

void MediaPlayer::stop() {
    if (_state == PlaybackState::PLAYING) {
        digitalWrite(_buzzerPin, LOW);
    }
    _state = PlaybackState::IDLE;
}

PlaybackState MediaPlayer::getState() const {
    return _state;
}

#else
// ============================================================================
// HARDWARE MODE — I2S Audio (MAX98357A) + Video (SD Card)
// ============================================================================

// Buffer sizes cho double-buffering
// Frame buffer: 128 * 160 * 2 bytes (RGB565) = 40,960 bytes → quá lớn cho ESP32-C3
// → Dùng line-by-line hoặc block-by-block rendering
static constexpr size_t VIDEO_BLOCK_SIZE  = 2048;  // 2KB per block (chia nhỏ frame)
static constexpr size_t AUDIO_BLOCK_SIZE  = 1024;  // 1KB per block
static constexpr size_t FRAME_SIZE_BYTES  = SCREEN_WIDTH * SCREEN_HEIGHT * 2; // RGB565

bool MediaPlayer::init(SDCardManager* sdCard, DisplayDriver* display,
                        uint8_t bclkPin, uint8_t lrcPin, uint8_t doutPin) {
    _sdCard  = sdCard;
    _display = display;

    // I2S sẽ được khởi tạo khi bắt đầu phát (vì sample rate phụ thuộc file WAV)
    Serial.println(F("[MediaPlayer] Initialized (I2S deferred until playback)"));
    _state = PlaybackState::IDLE;
    return true;
}

bool MediaPlayer::playMessage(const char* videoPath, const char* audioPath) {
    if (_sdCard == nullptr || _display == nullptr) {
        Serial.println(F("[MediaPlayer] ERROR: SDCard or Display not initialized"));
        _state = PlaybackState::ERROR;
        return false;
    }

    // --- Bước 1: Mở và parse file WAV ---
    if (!_sdCard->openFileForRead(audioPath)) {
        Serial.printf("[MediaPlayer] ERROR: Cannot open audio: %s\n", audioPath);
        _state = PlaybackState::ERROR;
        return false;
    }

    // Đọc WAV header (44 bytes)
    uint8_t headerBuf[44];
    size_t headerRead = _sdCard->readBlock(headerBuf, sizeof(headerBuf));
    _sdCard->closeReadFile();

    if (headerRead < 44) {
        Serial.println(F("[MediaPlayer] ERROR: WAV header too short"));
        _state = PlaybackState::ERROR;
        return false;
    }

    WAVHeader wavHeader;
    if (!parseWAVHeader(headerBuf, &wavHeader)) {
        _state = PlaybackState::ERROR;
        return false;
    }

    // --- Bước 2: Khởi tạo I2S với sample rate từ WAV ---
    if (!initI2S(PIN_I2S_BCLK, PIN_I2S_LRC, PIN_I2S_DOUT,
                 wavHeader.sampleRate, wavHeader.bitsPerSample)) {
        _state = PlaybackState::ERROR;
        return false;
    }

    // --- Bước 3: Mở cả hai file ---
    // Mở lại audio file (skip header)
    if (!_sdCard->openFileForRead(audioPath)) {
        deinitI2S();
        _state = PlaybackState::ERROR;
        return false;
    }
    // Skip WAV header
    uint8_t skipBuf[44];
    _sdCard->readBlock(skipBuf, 44);

    // Tính số frames video
    int32_t videoSize = _sdCard->getFileSize(videoPath);
    if (videoSize <= 0) {
        Serial.printf("[MediaPlayer] ERROR: Cannot get video size: %s\n", videoPath);
        _sdCard->closeReadFile();
        deinitI2S();
        _state = PlaybackState::ERROR;
        return false;
    }

    uint32_t totalFrames = videoSize / FRAME_SIZE_BYTES;
    Serial.printf("[MediaPlayer] Playing: %u frames, audio %uHz %ubit\n",
                  totalFrames, wavHeader.sampleRate, wavHeader.bitsPerSample);

    // --- Bước 4: Phát đồng bộ ---
    _state = PlaybackState::PLAYING;
    _display->turnOn();
    _display->setBacklight(BACKLIGHT_DAY_PERCENT);

    // Allocate buffers
    uint8_t* videoBuf = (uint8_t*)malloc(VIDEO_BLOCK_SIZE);
    uint8_t* audioBuf = (uint8_t*)malloc(AUDIO_BLOCK_SIZE);

    if (!videoBuf || !audioBuf) {
        Serial.println(F("[MediaPlayer] ERROR: Buffer allocation failed"));
        free(videoBuf);
        free(audioBuf);
        _sdCard->closeReadFile();
        deinitI2S();
        _state = PlaybackState::ERROR;
        return false;
    }

    // Tính lượng audio bytes cần phát mỗi frame để đồng bộ
    // audio_bytes_per_frame = (sampleRate * channels * bytesPerSample) / FPS
    uint32_t audioBytesPerFrame = wavHeader.byteRate / TARGET_FPS;
    uint32_t audioDataRemaining = wavHeader.dataSize;

    // Phát từng frame
    for (uint32_t frame = 0; frame < totalFrames && _state == PlaybackState::PLAYING; frame++) {
        uint32_t frameStart = millis();

        // Mở video file, seek tới frame hiện tại
        // (Vì SD và Display chia sẻ SPI, ta đọc video trước rồi push display sau)
        // TODO: Tối ưu bằng cách mở file video riêng thay vì mở/đóng mỗi frame
        _sdCard->closeReadFile(); // Đóng audio file tạm thời

        if (_sdCard->openFileForRead(videoPath)) {
            // Skip đến frame hiện tại
            uint32_t skipBytes = frame * FRAME_SIZE_BYTES;
            while (skipBytes > 0) {
                size_t toSkip = min((size_t)skipBytes, VIDEO_BLOCK_SIZE);
                _sdCard->readBlock(videoBuf, toSkip);
                skipBytes -= toSkip;
            }

            // Đọc và push frame (block by block)
            size_t frameRemaining = FRAME_SIZE_BYTES;
            uint16_t y = 0;
            while (frameRemaining > 0) {
                size_t toRead = min(frameRemaining, VIDEO_BLOCK_SIZE);
                size_t read = _sdCard->readBlock(videoBuf, toRead);
                if (read == 0) break;

                uint16_t lines = read / (SCREEN_WIDTH * 2);
                _display->pushFrameBuffer((uint16_t*)videoBuf, SCREEN_WIDTH, lines);
                y += lines;
                frameRemaining -= read;
            }
            _sdCard->closeReadFile();
        }

        // Phát audio tương ứng
        if (audioDataRemaining > 0) {
            _sdCard->openFileForRead(audioPath);
            // Skip header + audio đã phát
            uint32_t audioOffset = 44 + (wavHeader.dataSize - audioDataRemaining);
            uint32_t skipAudio = audioOffset;
            while (skipAudio > 0) {
                size_t toSkip = min((size_t)skipAudio, AUDIO_BLOCK_SIZE);
                _sdCard->readBlock(audioBuf, toSkip);
                skipAudio -= toSkip;
            }

            uint32_t audioToPlay = min(audioBytesPerFrame, audioDataRemaining);
            while (audioToPlay > 0) {
                size_t toRead = min((size_t)audioToPlay, AUDIO_BLOCK_SIZE);
                size_t read = _sdCard->readBlock(audioBuf, toRead);
                if (read == 0) break;

                size_t bytesWritten = 0;
                i2s_write(_i2sPort, audioBuf, read, &bytesWritten, portMAX_DELAY);
                audioToPlay -= read;
                audioDataRemaining -= read;
            }
            _sdCard->closeReadFile();
        }

        // Frame timing: đảm bảo mỗi frame chiếm đúng FRAME_DURATION_MS
        uint32_t elapsed = millis() - frameStart;
        if (elapsed < FRAME_DURATION_MS) {
            vTaskDelay(pdMS_TO_TICKS(FRAME_DURATION_MS - elapsed));
        }
    }

    // --- Dọn dẹp ---
    free(videoBuf);
    free(audioBuf);
    deinitI2S();
    _state = PlaybackState::IDLE;

    Serial.println(F("[MediaPlayer] Playback complete"));
    return true;
}

void MediaPlayer::stop() {
    _state = PlaybackState::IDLE;
}

PlaybackState MediaPlayer::getState() const {
    return _state;
}

// ============================================================================
// I2S Driver
// ============================================================================

bool MediaPlayer::initI2S(uint8_t bclkPin, uint8_t lrcPin, uint8_t doutPin,
                           uint32_t sampleRate, uint8_t bitsPerSample) {
    i2s_config_t i2sConfig = {};
    i2sConfig.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
    i2sConfig.sample_rate = sampleRate;
    i2sConfig.bits_per_sample = (i2s_bits_per_sample_t)bitsPerSample;
    i2sConfig.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT; // Mono
    i2sConfig.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    i2sConfig.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    i2sConfig.dma_buf_count = 8;
    i2sConfig.dma_buf_len = 256;
    i2sConfig.use_apll = false;

    if (i2s_driver_install(_i2sPort, &i2sConfig, 0, nullptr) != ESP_OK) {
        Serial.println(F("[MediaPlayer] ERROR: I2S driver install failed"));
        return false;
    }

    i2s_pin_config_t pinConfig = {};
    pinConfig.bck_io_num = bclkPin;
    pinConfig.ws_io_num = lrcPin;
    pinConfig.data_out_num = doutPin;
    pinConfig.data_in_num = I2S_PIN_NO_CHANGE;

    if (i2s_set_pin(_i2sPort, &pinConfig) != ESP_OK) {
        Serial.println(F("[MediaPlayer] ERROR: I2S pin config failed"));
        i2s_driver_uninstall(_i2sPort);
        return false;
    }

    i2s_zero_dma_buffer(_i2sPort);
    Serial.printf("[MediaPlayer] I2S initialized: %uHz, %ubit\n",
                  sampleRate, bitsPerSample);
    return true;
}

void MediaPlayer::deinitI2S() {
    i2s_driver_uninstall(_i2sPort);
    Serial.println(F("[MediaPlayer] I2S driver uninstalled"));
}

bool MediaPlayer::parseWAVHeader(const uint8_t* data, WAVHeader* header) {
    memcpy(header, data, sizeof(WAVHeader));

    // Validate RIFF header
    if (memcmp(header->riff, "RIFF", 4) != 0 ||
        memcmp(header->wave, "WAVE", 4) != 0) {
        Serial.println(F("[MediaPlayer] ERROR: Invalid WAV header (not RIFF/WAVE)"));
        return false;
    }

    // Validate PCM format
    if (header->audioFormat != 1) {
        Serial.printf("[MediaPlayer] ERROR: Unsupported audio format: %d (expected PCM=1)\n",
                      header->audioFormat);
        return false;
    }

    Serial.printf("[MediaPlayer] WAV: %uHz, %uch, %ubit, %u bytes data\n",
                  header->sampleRate, header->numChannels,
                  header->bitsPerSample, header->dataSize);
    return true;
}

#endif // WOKWI_SIMULATION

