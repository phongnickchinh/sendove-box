#include "AudioPlayer.h"
#include "SDCardManager.h"
#include "ScreenLogger.h"
#include "Settings.h"

// ============================================================================
// AudioPlayer Implementation
// ============================================================================

bool AudioPlayer::init() {
    if (_initialized) return true;

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
    setAmp(!_muted);
    DLOG("[AUD] I2S init OK");
    return true;
}

void AudioPlayer::setAmp(bool on) {
    if (PIN_AMP_SD < 0) return;
    pinMode((uint8_t)PIN_AMP_SD, OUTPUT);
    digitalWrite((uint8_t)PIN_AMP_SD, on ? HIGH : LOW);
}

void AudioPlayer::setVolume(uint8_t vol, bool immediate) {
    _gainTarget = Settings::volumeGainQ15(vol);
    if (immediate) _gain = _gainTarget;
    bool mute = (vol == 0);
    if (mute != _muted) {
        _muted = mute;
        if (_initialized) setAmp(!mute);
    }
}

// TRẢ LẠI 24KB RAM, không chỉ xoá bộ đệm. i2s_driver_install() cấp
// AUDIO_DMA_BUF_COUNT × AUDIO_DMA_BUF_LEN × 2 kênh × 2 byte = 24KB RAM nội bộ
// (đúng loại mbedTLS cần). Trước 2026-09-18 hàm này chỉ gọi i2s_zero_dma_buffer()
// — ghi số 0 vào bộ đệm chứ không trả về heap — nên 24KB nằm chết từ tiếng bíp
// lúc boot tới khi rút điện, trong khi hộp đứng ở màn hình chờ gần như suốt ngày.
void AudioPlayer::stop() {
    if (_initialized) {
        i2s_zero_dma_buffer(I2S_NUM_0);
        i2s_driver_uninstall(I2S_NUM_0);
        _initialized = false;
        setAmp(false);
    }
    _hasAudio    = false;
    _storage     = nullptr;
    if (_card) {
        _card->closeAtFile();  // file nhạc báo thức giữ handle đọc ngẫu nhiên
        _card = nullptr;
    }
    _loop        = false;
    _audioCursor = 0;
    _sampleRate  = AUDIO_SAMPLE_RATE;
}

void AudioPlayer::testBeep() {
    DLOG("[AUD] Playing boot beep test (%lu Hz)", (unsigned long)(_sampleRate * AUDIO_OVERSAMPLE));
    beep(150);
    stop();  // trả 24KB DMA lại ngay; lần phát sau tự init lại
}

void AudioPlayer::beep(uint32_t durationMs) {
    if (!init()) return;

    uint32_t rate = _sampleRate * AUDIO_OVERSAMPLE;
    // Bíp báo thức chạy sau khi đã phát tin: loadFromStorage() có thể đã đổi tốc
    // độ phần cứng sang 16kHz x4 mà stop() chỉ trả _sampleRate về mặc định. Không
    // set lại thì tiếng bíp nhanh gấp đôi và cao gấp đôi. Lúc boot thì vô hại.
    i2s_set_sample_rates(I2S_NUM_0, rate);
    int16_t beepFrame[2];
    // Phát sóng sin mượt mà thay vì sóng vuông để tránh tiếng rè (rẹt rẹt)
    const int16_t sine[20] = {0, 1236, 2351, 3236, 3804, 4000, 3804, 3236, 2351, 1236, 0, -1236, -2351, -3236, -3804, -4000, -3804, -3236, -2351, -1236};
    int samples = (rate * durationMs) / 1000;
    _gain = _gainTarget;  // bíp ngắn, không cần trượt
    const bool unity = (_gain == Settings::GAIN_UNITY);
    for (int i = 0; i < samples; i++) {
        int16_t sample = unity ? sine[i % 20] : (int16_t)(((int32_t)sine[i % 20] * _gain) >> 15);
        beepFrame[0] = sample;
        beepFrame[1] = sample;
        size_t written = 0;
        // Block until written
        i2s_write(I2S_NUM_0, beepFrame, sizeof(beepFrame), &written, portMAX_DELAY);
    }
    // Gỡ driver ngay sau vòng ghi thì DMA còn dữ liệu chưa phát hết bị xoá giữa
    // chừng ⇒ nghe thành tiếng rẹt. Chờ xả xong rồi mới cho phép gỡ.
    // KHÔNG tự gọi stop() ở đây: báo thức bíp mỗi giây, gỡ rồi cài lại driver 60
    // lần một hồi chuông là tự tay làm vụn heap. Bên gọi chịu trách nhiệm stop().
    delay(200);
}

int AudioPlayer::readSrc(uint32_t offset, uint8_t* buf, uint32_t len) {
    if (_card) return _card->readAtFile(offset, buf, len);
    return _storage ? _storage->readAt(offset, buf, len) : 0;
}

bool AudioPlayer::loadFromStorage(IStorageProvider* storage, uint32_t videoDataSize, uint32_t appendedSize) {
    _hasAudio = false;
    _storage  = storage;
    _card     = nullptr;
    _loop     = false;
    return parseAudc(videoDataSize, appendedSize, AUDIO_MAX_PCM_BYTES);
}

bool AudioPlayer::loadFromFile(SDCardManager* card, const char* path, bool loop) {
    _hasAudio = false;
    _storage  = nullptr;
    _card     = nullptr;
    _loop     = loop;
    if (!card || !card->openAtFile(path)) return false;
    _card = card;
    // File nhạc = AUDC(10) + WAV(44) + PCM, không có phần video phía trước -> offset 0.
    if (!parseAudc(0, card->atFileSize(), ALARM_MUSIC_MAX_BYTES)) {
        card->closeAtFile();
        _card = nullptr;
        return false;
    }
    return true;
}

bool AudioPlayer::parseAudc(uint32_t audioStartOffset, uint32_t appendedSize, uint32_t maxPcm) {
    // readAt() = đọc theo offset tuyệt đối. KHÔNG dùng seek()+readData() ở đây:
    //  1) readData() bị chặn ở dataSize (chỉ phần video) nên không bao giờ với tới
    //     được vùng audio nối phía sau — đó là lý do audio câm trên bản NOR;
    //  2) seek() dịch chính con trỏ tuần tự mà MediaPlayer::decodeOneFrame() đang
    //     dùng, nên frame kế tiếp đọc trúng PCM và báo BAD jpegSize.
    const uint32_t videoDataSize = audioStartOffset;

    uint8_t header[AUDC_HEADER_SIZE] = {0};
    int readBytes = readSrc(audioStartOffset, header, AUDC_HEADER_SIZE);

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

    if (pcmSize == 0 || pcmSize > maxPcm) {
        DLOG("[AUD] AUDC invalid size=%lu", (unsigned long)pcmSize);
        return false;
    }

    // Web gửi lên file WAV thật (backend chốt content-type audio/wav, và bản ghi âm
    // giọng nói còn phải phát được trên trình duyệt). 44 byte RIFF ở đầu không phải
    // âm thanh — phát thẳng thì nghe một tiếng "tạch" rồi lệch kênh cả bài.
    uint32_t pcmStart = videoDataSize + AUDC_HEADER_SIZE;
    uint8_t riff[WAV_HEADER_SIZE] = {0};
    if (pcmSize > WAV_HEADER_SIZE &&
        readSrc(pcmStart, riff, WAV_HEADER_SIZE) == (int)WAV_HEADER_SIZE &&
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
        i2s_set_sample_rates(I2S_NUM_0, sampleRate * AUDIO_OVERSAMPLE);
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
    for (;;) {
        if (_audioCursor >= _audioPcmSize) {
            if (!_loop) break;
            _audioCursor = 0;  // nhạc báo thức: hết bài quay lại đầu (web đã fade 2 đầu)
        }
        if (!fillChunk()) break;
    }
}

bool AudioPlayer::fillChunk() {
    uint32_t remaining = _audioPcmSize - _audioCursor;
    if (remaining == 0) return false;

    // MỘT lời gọi đọc cho cả AUDIO_READ_CHUNK_SIZE byte. Trên thẻ SD mỗi lượt đọc
    // là một lần lấy spiMutex + NOP hack + fread xuyên VFS/FATFS, nên số LƯỢT mới
    // là thứ đắt, không phải số byte. Cỡ giãn mẫu vẫn giữ AUDIO_PCM_CHUNK_SIZE để
    // _stereo không phình (xem config.h).
    uint32_t toRead = (remaining < AUDIO_READ_CHUNK_SIZE) ? remaining : AUDIO_READ_CHUNK_SIZE;
    toRead &= ~1u;
    if (toRead == 0) return false;

    // readAt(): offset tuyệt đối, không đụng con trỏ tuần tự của MediaPlayer
    int bytesRead = readSrc(_audioPcmOffset + _audioCursor, _chunk, toRead);
    if (bytesRead <= 0) return false;
    bytesRead &= ~1;

    const int16_t* pcm         = (const int16_t*)_chunk;
    const int      totalSamples = bytesRead / 2;
    // Mỗi lượt giãn tối đa bấy nhiêu mẫu thì vừa đúng sức chứa _stereo.
    const int      maxPerPass   = (int)(AUDIO_PCM_CHUNK_SIZE / 2);

    // Âm lượng: hệ số 32768 cả hai đầu = đường cũ nguyên vẹn, không nhân gì (vùng từng
    // sinh "rẹt rẹt" ở §14/§24, giữ mức mặc định 100 bit-identical với bản trước).
    // Khác 32768 thì mỗi lượt trượt _gain tới đích tối đa GAIN_STEP, nội suy trong lượt.
    // Lượt = 128 mẫu (8ms @16kHz) -> từ câm lên tối đa mất ~4 lượt, không nghe "bụp".
    static constexpr int32_t GAIN_STEP = 8192;
    const bool unity = (_gain == Settings::GAIN_UNITY && _gainTarget == Settings::GAIN_UNITY);

    for (int base = 0; base < totalSamples; base += maxPerPass) {
        int passSamples = totalSamples - base;
        if (passSamples > maxPerPass) passSamples = maxPerPass;

        int32_t gStart = _gain;
        int32_t gEnd = gStart;
        if (!unity && gStart != _gainTarget) {
            int32_t d = _gainTarget - gStart;
            if (d > GAIN_STEP) d = GAIN_STEP;
            if (d < -GAIN_STEP) d = -GAIN_STEP;
            gEnd = gStart + d;
        }

        // Expand Mono → Stereo + Linear Interpolation Oversample (x AUDIO_OVERSAMPLE):
        // Thay vì lặp mẫu thô (Zero-Order Hold) tạo sóng bậc thang vuông vức gây chói gắt,
        // nội suy tuyến tính nối mượt giữa mẫu hiện tại và mẫu tiếp theo:
        // S[i] -> S[i+1], chia đều khoảng cách làm AUDIO_OVERSAMPLE nấc liên tục.
        // Mẫu kế được nhìn XUYÊN ranh giới lượt trong cùng buffer đọc, nên chỉ mẫu
        // cuối cùng của cả buffer mới phải tự lặp lại chính nó.
        for (int i = 0; i < passSamples; i++) {
            int16_t currSample = pcm[base + i];
            int16_t nextSample = (base + i + 1 < totalSamples) ? pcm[base + i + 1] : currSample;
            if (!unity) {
                // |mẫu × g| >> 15 với g ≤ 32768 không bao giờ vượt int16 -> không cần kẹp.
                int32_t g = gStart + ((gEnd - gStart) * i) / passSamples;
                currSample = (int16_t)(((int32_t)currSample * g) >> 15);
                nextSample = (int16_t)(((int32_t)nextSample * g) >> 15);
            }
            int32_t diff       = (int32_t)nextSample - (int32_t)currSample;

            for (int r = 0; r < AUDIO_OVERSAMPLE; r++) {
                int16_t interpolated = (int16_t)(currSample + ((diff * r) / (int32_t)AUDIO_OVERSAMPLE));
                int idx = (i * AUDIO_OVERSAMPLE + r) * 2;
                _stereo[idx]     = interpolated; // Left
                _stereo[idx + 1] = interpolated; // Right
            }
        }

        // timeout = 0 (non-blocking). 'written' PHẢI được tôn trọng: nếu DMA từ
        // chối một phần thì chỉ tính phần đã chấp nhận rồi DỪNG cả vòng — phần
        // còn lại của buffer sẽ được đọc lại ở tick() sau. Đọc lặp một lượt rẻ
        // hơn nhiều so với việc phải giữ thêm state của buffer dở dang.
        size_t passBytes = (size_t)passSamples * AUDIO_OVERSAMPLE * 4;
        size_t written   = 0;
        i2s_write(I2S_NUM_0, _stereo, passBytes, &written, 0);
        // Chốt cả khi DMA chỉ nhận một phần: lượt đọc lại sau đi tiếp từ gEnd, lệch hệ
        // số một lượt chỉ xảy ra đúng lúc đang đổi âm lượng, không nghe được.
        _gain = gEnd;

        // Quy đổi ngược: 1 mẫu mono gốc = AUDIO_OVERSAMPLE * 4 bytes stereo output
        // -> 1 byte mono gốc = AUDIO_OVERSAMPLE * 2 bytes stereo output
        written &= ~(size_t)3;
        _audioCursor += (uint32_t)(written / (AUDIO_OVERSAMPLE * 2));

        if (written < passBytes) return false; // DMA đã đầy
    }

    return true;
}
