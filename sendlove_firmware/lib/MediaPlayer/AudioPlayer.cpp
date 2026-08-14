#include "AudioPlayer.h"
#include "ScreenLogger.h"

// ============================================================================
// AudioPlayer Implementation
// ============================================================================

bool AudioPlayer::init() {
    i2s_config_t cfg = {
        .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate          = AUDIO_SAMPLE_RATE,
        .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format       = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count        = AUDIO_DMA_BUF_COUNT,
        .dma_buf_len          = AUDIO_DMA_BUF_LEN,
        .use_apll             = false,
        .tx_desc_auto_clear   = true
    };
    i2s_pin_config_t pins = {
        .bck_io_num   = PIN_I2S_BCLK,
        .ws_io_num    = PIN_I2S_LRC,
        .data_out_num = PIN_I2S_DOUT,
        .data_in_num  = I2S_PIN_NO_CHANGE
    };

    esp_err_t err1 = i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL);
    esp_err_t err2 = i2s_set_pin(I2S_NUM_0, &pins);

    if (err1 != ESP_OK || err2 != ESP_OK) {
        DLOG("[AUD] I2S init fail: %d %d", (int)err1, (int)err2);
        return false;
    }
    _initialized = true;
    DLOG("[AUD] I2S init OK");
    return true;
}

void AudioPlayer::stop() {
    if (_initialized) {
        i2s_driver_uninstall(I2S_NUM_0);
        _initialized = false;
    }
    _hasAudio    = false;
    _storage     = nullptr;
    _audioCursor = 0;
}

void AudioPlayer::testBeep() {
    if (!init()) return;
    
    DLOG("[AUD] Playing boot beep test");
    int16_t beepFrame[2];
    // Phát sóng sin 400Hz mượt mà thay vì sóng vuông để tránh tiếng rè (rẹt rẹt)
    const int16_t sine[20] = {0, 1236, 2351, 3236, 3804, 4000, 3804, 3236, 2351, 1236, 0, -1236, -2351, -3236, -3804, -4000, -3804, -3236, -2351, -1236};
    int samples = (AUDIO_SAMPLE_RATE * 150) / 1000;
    for (int i = 0; i < samples; i++) {
        int16_t sample = sine[i % 20];
        beepFrame[0] = sample;
        beepFrame[1] = sample;
        size_t written = 0;
        // Block until written
        i2s_write(I2S_NUM_0, beepFrame, sizeof(beepFrame), &written, portMAX_DELAY);
    }
    stop();
}

bool AudioPlayer::loadFromStorage(IStorageProvider* storage, uint32_t videoDataSize) {
    _hasAudio = false;
    _storage  = storage;

    // Seek đến ngay sau phần video để tìm "AUDC" header
    uint32_t audioStartOffset = videoDataSize;
    DLOG("[AUD] seeking to offset %lu for AUDC", (unsigned long)audioStartOffset);
    storage->seek(audioStartOffset);

    uint8_t header[AUDC_HEADER_SIZE] = {0};
    int readBytes = storage->readData(header, AUDC_HEADER_SIZE);
    DLOG("[AUD] read %d bytes, magic=%c%c%c%c", readBytes,
         header[0], header[1], header[2], header[3]);

    if (readBytes < (int)AUDC_HEADER_SIZE || memcmp(header, "AUDC", 4) != 0) {
        // Không có audio — backward-compatible, tiếp tục phát video im lặng
        DLOG("[AUD] no AUDC header found");
        return false;
    }

    uint16_t sampleRate = 0;
    uint32_t pcmSize    = 0;
    memcpy(&sampleRate, header + 4, sizeof(sampleRate));
    memcpy(&pcmSize,    header + 6, sizeof(pcmSize));

    if (pcmSize == 0 || pcmSize > 800000) {
        DLOG("[AUD] AUDC invalid size=%lu", (unsigned long)pcmSize);
        return false;
    }

    _audioPcmOffset = videoDataSize + AUDC_HEADER_SIZE;
    _audioPcmSize   = pcmSize;
    _audioCursor    = 0;
    _hasAudio       = true;

    DLOG("[AUD] OK: %lu bytes, rate=%u", (unsigned long)pcmSize, sampleRate);
    return true;
}

void AudioPlayer::prefill() {
    if (!_hasAudio || !_initialized) return;
    // Nạp trước 2 DMA buffer để tránh khoảng lặng đầu bài
    fillChunk();
    fillChunk();
}

void AudioPlayer::tick() {
    if (!_hasAudio || !_initialized) return;
    if (_audioCursor >= _audioPcmSize) return; // Hết audio
    fillChunk();
}

void AudioPlayer::fillChunk() {
    uint32_t remaining = _audioPcmSize - _audioCursor;
    if (remaining == 0) return;

    uint32_t toRead = (remaining < AUDIO_PCM_CHUNK_SIZE) ? remaining : AUDIO_PCM_CHUNK_SIZE;

    // Seek chính xác về vị trí đang phát trong PCM stream
    _storage->seek(_audioPcmOffset + _audioCursor);
    int bytesRead = _storage->readData(_chunk, toRead);
    if (bytesRead <= 0) return;

    // Expand Mono → Stereo: mỗi sample 16-bit Mono thành frame L+R 32-bit
    // Ghi trực tiếp vào I2S từng frame, tránh cấp phát buffer stereo
    const int16_t* pcm     = (const int16_t*)_chunk;
    int            samples = bytesRead / 2;
    int16_t        stereoFrame[2];

    for (int i = 0; i < samples; i++) {
        stereoFrame[0] = pcm[i]; // Left
        stereoFrame[1] = pcm[i]; // Right
        size_t written = 0;
        // timeout = 0: non-blocking — không block CPU nếu DMA buffer đầy
        i2s_write(I2S_NUM_0, stereoFrame, sizeof(stereoFrame), &written, 0);
    }

    _audioCursor += (uint32_t)bytesRead;
}
