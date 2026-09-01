#include "AudioPlayer.h"
#include "ScreenLogger.h"

// ============================================================================
// AudioPlayer Implementation
// ============================================================================

bool AudioPlayer::init() {
    i2s_config_t cfg = {
        .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        // x AUDIO_OVERSAMPLE: BCLK ở đúng tốc độ file (8kHz) quá thấp cho
        // MAX98357A -> rè. Mỗi mẫu file được lặp lại trong fillChunk()/testBeep()
        // để cao độ không đổi. Xem giải thích đầy đủ ở config.h.
        .sample_rate          = _sampleRate * AUDIO_OVERSAMPLE,
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
    _sampleRate  = AUDIO_SAMPLE_RATE;
}

void AudioPlayer::testBeep() {
    if (!init()) return;

    // init() đã mở I2S ở _sampleRate * AUDIO_OVERSAMPLE (32kHz mặc định) nên
    // không cần set lại sample rate ở đây - cứ ghi thẳng ở tốc độ đó.
    uint32_t rate = _sampleRate * AUDIO_OVERSAMPLE;
    DLOG("[AUD] Playing boot beep test (%lu Hz)", (unsigned long)rate);
    int16_t beepFrame[2];
    // Phát sóng sin mượt mà thay vì sóng vuông để tránh tiếng rè (rẹt rẹt)
    const int16_t sine[20] = {0, 1236, 2351, 3236, 3804, 4000, 3804, 3236, 2351, 1236, 0, -1236, -2351, -3236, -3804, -4000, -3804, -3236, -2351, -1236};
    int samples = (rate * 150) / 1000;
    for (int i = 0; i < samples; i++) {
        int16_t sample = sine[i % 20];
        beepFrame[0] = sample;
        beepFrame[1] = sample;
        size_t written = 0;
        // Block until written
        i2s_write(I2S_NUM_0, beepFrame, sizeof(beepFrame), &written, portMAX_DELAY);
    }
    // Nếu stop() (i2s_driver_uninstall) ngay sau vòng ghi thì DMA còn dữ liệu
    // chưa phát hết bị xoá giữa chừng ⇒ nghe thành tiếng rẹt chứ không phải
    // tiếng bíp trọn vẹn. Chờ đủ thời lượng phát rồi mới gỡ driver.
    delay(200);
    stop();
}

bool AudioPlayer::loadFromStorage(IStorageProvider* storage, uint32_t videoDataSize, uint32_t appendedSize) {
    _hasAudio = false;
    _storage  = storage;

    // readAt() = đọc theo offset tuyệt đối. KHÔNG dùng seek()+readData() ở đây:
    //  1) readData() bị chặn ở dataSize (chỉ phần video) nên không bao giờ với tới
    //     được vùng audio nối phía sau — đó là lý do audio câm trên bản NOR;
    //  2) seek() dịch chính con trỏ tuần tự mà MediaPlayer::decodeOneFrame() đang
    //     dùng, nên frame kế tiếp đọc trúng PCM và báo BAD jpegSize.
    uint32_t audioStartOffset = videoDataSize;

    uint8_t header[AUDC_HEADER_SIZE] = {0};
    int readBytes = storage->readAt(audioStartOffset, header, AUDC_HEADER_SIZE);

    if (readBytes < (int)AUDC_HEADER_SIZE || memcmp(header, "AUDC", 4) != 0) {
        // Không có audio — backward-compatible, tiếp tục phát video im lặng
        DLOG("[AUD] no AUDC header @ %lu", (unsigned long)audioStartOffset);
        return false;
    }

    uint16_t sampleRate = 0;
    uint32_t pcmSize    = 0;
    memcpy(&sampleRate, header + 4, sizeof(sampleRate));
    memcpy(&pcmSize,    header + 6, sizeof(pcmSize));

    // Bảng slot đáng tin hơn header: khi server trả chunked (Content-Length = -1)
    // NetworkManager không biết trước độ dài nên đã ghi pcmSize = 0.
    if (appendedSize > AUDC_HEADER_SIZE) {
        uint32_t fromTable = appendedSize - AUDC_HEADER_SIZE;
        if (pcmSize == 0 || pcmSize > fromTable) pcmSize = fromTable;
    }

    if (sampleRate < 4000 || sampleRate > 48000) {
        DLOG("[AUD] rate la %u -> dung %lu", (unsigned)sampleRate, (unsigned long)AUDIO_SAMPLE_RATE);
        sampleRate = AUDIO_SAMPLE_RATE;
    }

    if (pcmSize == 0 || pcmSize > AUDIO_MAX_PCM_BYTES) {
        DLOG("[AUD] AUDC invalid size=%lu", (unsigned long)pcmSize);
        return false;
    }

    // Web gửi lên file WAV thật (backend chốt content-type audio/wav, và bản ghi âm
    // giọng nói còn phải phát được trên trình duyệt). 44 byte RIFF ở đầu không phải
    // âm thanh — phát thẳng thì nghe một tiếng "tạch" rồi lệch kênh cả bài.
    uint32_t pcmStart = videoDataSize + AUDC_HEADER_SIZE;
    uint8_t riff[WAV_HEADER_SIZE] = {0};
    if (pcmSize > WAV_HEADER_SIZE &&
        storage->readAt(pcmStart, riff, WAV_HEADER_SIZE) == (int)WAV_HEADER_SIZE &&
        memcmp(riff, "RIFF", 4) == 0 && memcmp(riff + 8, "WAVE", 4) == 0 &&
        memcmp(riff + 36, "data", 4) == 0) {

        uint32_t wavRate = 0, dataLen = 0;
        memcpy(&wavRate, riff + 24, 4);   // fmt: sample rate
        memcpy(&dataLen, riff + 40, 4);   // data: số byte PCM

        pcmStart += WAV_HEADER_SIZE;
        uint32_t avail = pcmSize - WAV_HEADER_SIZE;
        pcmSize = (dataLen > 0 && dataLen <= avail) ? dataLen : avail;
        if (wavRate >= 4000 && wavRate <= 48000) sampleRate = (uint16_t)wavRate;
        DLOG("[AUD] WAV: rate=%lu data=%lu", (unsigned long)wavRate, (unsigned long)pcmSize);
    }

    // Chốt sau khi đã trừ header: dataLen lẻ thì byte cuối cho samples = 0,
    // cursor đứng yên và mỗi tick còn lại đọc SPI 1 byte vô ích tới hết bài.
    pcmSize &= ~1u;   // PCM 16-bit

    // Tốc độ lấy mẫu do file quyết định, không phải hằng số biên dịch: web đổi
    // 8k <-> 16k thì hộp phát đúng cao độ mà không phải nạp lại firmware.
    if (_initialized && sampleRate != _sampleRate) {
wao        i2s_set_sample_rates(I2S_NUM_0, sampleRate * AUDIO_OVERSAMPLE);
    }
    _sampleRate     = sampleRate;
    _audioPcmOffset = pcmStart;
    _audioPcmSize   = pcmSize;
    _audioCursor    = 0;
    _hasAudio       = true;

    DLOG("[AUD] OK: %lu bytes, rate=%u", (unsigned long)pcmSize, sampleRate);
    return true;
}

void AudioPlayer::prefill() {
    if (!_hasAudio || !_initialized) return;
    // Xoa DMA truoc khi nap: prefill() con duoc goi lai moi khi video loop ve
    // dau. Neu khong xoa, tan du toi 512ms cua vong truoc van dang xep hang va
    // se phat chong len doan dau moi -> nghe nhu vap/giat tai diem loop.
    i2s_zero_dma_buffer(I2S_NUM_0);
    // Nap day DMA thay vi dung 2 chunk: 2 chunk chi la 200ms trong khi DMA sau
    // 512ms, khong du dem khi mot vai frame JPEG giai ma cham.
    while (_audioCursor < _audioPcmSize && fillChunk()) {}
}

void AudioPlayer::tick() {
    if (!_hasAudio || !_initialized) return;
    // Nap cho toi khi DMA tu choi. Cach cu nap dung 1 chunk co dinh moi frame
    // nen luong nap phu thuoc fps va sample rate: 1600 byte/frame chi du o
    // 8kHz. O 16kHz/15fps can 2133 byte/frame -> thieu 25% -> DMA can dan ->
    // re/giat. Vong lap nay tu dieu tiet theo toc do tieu thu that cua I2S.
    while (_audioCursor < _audioPcmSize && fillChunk()) {}
}

bool AudioPlayer::fillChunk() {
    uint32_t remaining = _audioPcmSize - _audioCursor;
    if (remaining == 0) return false;

    uint32_t toRead = (remaining < AUDIO_PCM_CHUNK_SIZE) ? remaining : AUDIO_PCM_CHUNK_SIZE;

    // readAt(): offset tuyệt đối, không đụng con trỏ tuần tự của MediaPlayer
    int bytesRead = _storage->readAt(_audioPcmOffset + _audioCursor, _chunk, toRead);
    if (bytesRead <= 0) return false;

    // Expand Mono → Stereo + Oversample: mỗi mẫu mono 16-bit lặp lại
    // AUDIO_OVERSAMPLE lần thành frame stereo (L+R) để I2S ở tốc độ
    // sampleRate * AUDIO_OVERSAMPLE giữ đúng cao độ gốc.
    const int16_t* pcm     = (const int16_t*)_chunk;
    int            samples = bytesRead / 2;
    for (int i = 0; i < samples; i++) {
        for (int r = 0; r < AUDIO_OVERSAMPLE; r++) {
            int idx = (i * AUDIO_OVERSAMPLE + r) * 2;
            _stereo[idx]     = pcm[i]; // Left
            _stereo[idx + 1] = pcm[i]; // Right
        }
    }

    // Một lần i2s_write cho cả chunk, timeout = 0 (non-blocking).
    // 'written' PHẢI được tôn trọng: nếu DMA từ chối một phần thì chỉ tính
    // phần đã chấp nhận, lần tick() kế sẽ nạp phần còn lại.
    size_t totalBytes = (size_t)samples * AUDIO_OVERSAMPLE * 4;
    size_t written    = 0;
    i2s_write(I2S_NUM_0, _stereo, totalBytes, &written, 0);

    // Quy đổi ngược: 1 mẫu mono gốc = AUDIO_OVERSAMPLE * 4 bytes stereo output
    // -> 1 byte mono gốc = AUDIO_OVERSAMPLE * 2 bytes stereo output
    written &= ~(size_t)3;
    _audioCursor += (uint32_t)(written / (AUDIO_OVERSAMPLE * 2));

    return written == totalBytes;
}
