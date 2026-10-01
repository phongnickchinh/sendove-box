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
        // x AUDIO_OVERSAMPLE: BCLK at the file rate (8kHz) is too low for the
        // MAX98357A -> crackle. Each file sample is repeated in
        // fillChunk()/testBeep() to keep the pitch. Full explanation in config.h.
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

// FREES 24KB of RAM, not just the buffer contents. i2s_driver_install() allocates
// AUDIO_DMA_BUF_COUNT × AUDIO_DMA_BUF_LEN × 2 channels × 2 bytes = 24KB of internal
// RAM (the very kind mbedTLS needs). Only calling i2s_zero_dma_buffer() would zero
// the buffers without returning them to the heap, leaving 24KB dead from the boot
// beep until power-off while the box sits on standby nearly all day.
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
        _card->closeAtFile();  // an alarm music file holds the random-read handle
        _card = nullptr;
    }
    _loop        = false;
    _audioCursor = 0;
    _sampleRate  = AUDIO_SAMPLE_RATE;
}

void AudioPlayer::testBeep() {
    DLOG("[AUD] Playing boot beep test (%lu Hz)", (unsigned long)(_sampleRate * AUDIO_OVERSAMPLE));
    beep(150);
    stop();  // frees the 24KB of DMA right away; the next playback re-inits
}

void AudioPlayer::beep(uint32_t durationMs) {
    if (!init()) return;

    uint32_t rate = _sampleRate * AUDIO_OVERSAMPLE;
    // The alarm beep may run after a message was played: loadFromStorage() may have
    // switched the hardware rate to 16kHz x4 while stop() only resets _sampleRate to
    // the default. Without setting it again the beep plays twice as fast and twice
    // as high. Harmless at boot.
    i2s_set_sample_rates(I2S_NUM_0, rate);
    int16_t beepFrame[2];
    // A smooth sine instead of a square wave, to avoid crackle.
    // Amplitude 32000 = nearly full scale (peak -0.2 dBFS, RMS -3.2 dBFS): product
    // decision — the beep should be "really loud". To make it quieter, scale the
    // whole table by one factor.
    const int16_t sine[20] = {0, 9889, 18809, 25889, 30434, 32000, 30434, 25889, 18809, 9889, 0, -9889, -18809, -25889, -30434, -32000, -30434, -25889, -18809, -9889};
    int samples = (rate * durationMs) / 1000;
    _gain = _gainTarget;  // a short beep needs no gain slide
    const bool unity = (_gain == Settings::GAIN_UNITY);
    for (int i = 0; i < samples; i++) {
        int16_t sample = unity ? sine[i % 20] : (int16_t)(((int32_t)sine[i % 20] * _gain) >> 15);
        beepFrame[0] = sample;
        beepFrame[1] = sample;
        size_t written = 0;
        // Block until written
        i2s_write(I2S_NUM_0, beepFrame, sizeof(beepFrame), &written, portMAX_DELAY);
    }
    // Uninstalling the driver right after the write loop would cut off data still
    // in the DMA ⇒ an audible crack. Wait for it to drain first.
    // Do NOT call stop() here: the alarm beeps every second, and reinstalling the
    // driver 60 times per ring would fragment the heap. The caller owns stop().
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
    // A music file = AUDC(10) + WAV(44) + PCM with no video in front -> offset 0.
    if (!parseAudc(0, card->atFileSize(), ALARM_MUSIC_MAX_BYTES)) {
        card->closeAtFile();
        _card = nullptr;
        return false;
    }
    return true;
}

bool AudioPlayer::parseAudc(uint32_t audioStartOffset, uint32_t appendedSize, uint32_t maxPcm) {
    // readAt() = read at an absolute offset. Do NOT use seek()+readData() here:
    //  1) readData() is capped at dataSize (the video part), so it can never
    //     reach the audio region appended after it — which would mean silent audio;
    //  2) seek() moves the very sequential cursor MediaPlayer::decodeOneFrame()
    //     uses, so the next frame would read PCM and report BAD jpegSize.
    const uint32_t videoDataSize = audioStartOffset;

    uint8_t header[AUDC_HEADER_SIZE] = {0};
    int readBytes = readSrc(audioStartOffset, header, AUDC_HEADER_SIZE);

    if (readBytes < (int)AUDC_HEADER_SIZE || memcmp(header, "AUDC", 4) != 0) {
        // No audio — backward-compatible: keep playing the video silently
        DLOG("[AUD] no AUDC header @ %lu", (unsigned long)audioStartOffset);
        return false;
    }

    uint16_t sampleRate = 0;
    uint32_t pcmSize    = 0;
    memcpy(&sampleRate, header + 4, sizeof(sampleRate));
    memcpy(&pcmSize,    header + 6, sizeof(pcmSize));

    // The slot table is more reliable than the header: with a chunked response
    // (Content-Length = -1) NetworkManager doesn't know the length up front and
    // writes pcmSize = 0.
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

    // The web uploads a real WAV file (the backend pins content-type audio/wav, and
    // voice recordings must also play in the browser). The 44 RIFF bytes at the
    // start aren't audio — played raw they give a click and shift the channels for
    // the whole clip.
    uint32_t pcmStart = videoDataSize + AUDC_HEADER_SIZE;
    uint8_t riff[WAV_HEADER_SIZE] = {0};
    if (pcmSize > WAV_HEADER_SIZE &&
        readSrc(pcmStart, riff, WAV_HEADER_SIZE) == (int)WAV_HEADER_SIZE &&
        memcmp(riff, "RIFF", 4) == 0 && memcmp(riff + 8, "WAVE", 4) == 0 &&
        memcmp(riff + 36, "data", 4) == 0) {

        uint32_t wavRate = 0, dataLen = 0;
        memcpy(&wavRate, riff + 24, 4);   // fmt: sample rate
        memcpy(&dataLen, riff + 40, 4);   // data: PCM byte count

        pcmStart += WAV_HEADER_SIZE;
        uint32_t avail = pcmSize - WAV_HEADER_SIZE;
        pcmSize = (dataLen > 0 && dataLen <= avail) ? dataLen : avail;
        if (wavRate >= 4000 && wavRate <= 48000) sampleRate = (uint16_t)wavRate;
        DLOG("[AUD] WAV: rate=%lu data=%lu", (unsigned long)wavRate, (unsigned long)pcmSize);
    }

    // Finalize after subtracting the header: with an odd dataLen the last byte
    // yields samples = 0, the cursor stalls and every remaining tick reads one
    // useless byte over SPI until the end.
    pcmSize &= ~1u;   // PCM 16-bit

    // The sample rate comes from the file, not a compile-time constant: the web can
    // switch 8k <-> 16k and the box plays at the right pitch without reflashing.
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
    // Clear the DMA before filling: prefill() also runs each time the video loops
    // back to the start. Without clearing, up to 512ms left from the previous lap
    // is still queued and plays over the new start -> a stumble at the loop point.
    i2s_zero_dma_buffer(I2S_NUM_0);
    // Fill the DMA completely instead of exactly 2 chunks: 2 chunks are only 200ms
    // while the DMA is 512ms deep — not enough cushion when a few JPEG frames decode slowly.
    while (_audioCursor < _audioPcmSize && fillChunk()) {}
}

void AudioPlayer::tick() {
    if (!_hasAudio || !_initialized) return;
    // Fill until the DMA refuses. One fixed chunk per frame would tie the feed
    // rate to fps and sample rate: 1600 bytes/frame is only enough at 8kHz; at
    // 16kHz/15fps it takes 2133 bytes/frame -> 25% short -> the DMA drains ->
    // crackle/stutter. This loop self-regulates to I2S's real consumption rate.
    for (;;) {
        if (_audioCursor >= _audioPcmSize) {
            if (!_loop) break;
            _audioCursor = 0;  // alarm music: loop back to the start (the web fades both ends)
        }
        if (!fillChunk()) break;
    }
}

bool AudioPlayer::fillChunk() {
    uint32_t remaining = _audioPcmSize - _audioCursor;
    if (remaining == 0) return false;

    // ONE read call for the whole AUDIO_READ_CHUNK_SIZE. On an SD card each read is
    // a spiMutex take + the NOP hack + an fread through VFS/FATFS, so the NUMBER of
    // reads is what costs, not the byte count. The oversampling chunk stays
    // AUDIO_PCM_CHUNK_SIZE so _stereo doesn't grow (see config.h).
    uint32_t toRead = (remaining < AUDIO_READ_CHUNK_SIZE) ? remaining : AUDIO_READ_CHUNK_SIZE;
    toRead &= ~1u;
    if (toRead == 0) return false;

    // readAt(): absolute offset; doesn't touch MediaPlayer's sequential cursor
    int bytesRead = readSrc(_audioPcmOffset + _audioCursor, _chunk, toRead);
    if (bytesRead <= 0) return false;
    bytesRead &= ~1;

    const int16_t* pcm         = (const int16_t*)_chunk;
    const int      totalSamples = bytesRead / 2;
    // Expanding at most this many samples per pass exactly fits _stereo.
    const int      maxPerPass   = (int)(AUDIO_PCM_CHUNK_SIZE / 2);

    // Volume: a gain of 32768 at both ends = the original path, no multiply (this
    // area produced crackle before — MEMORY.md §14/§24 — so the default volume 100
    // stays bit-identical). Otherwise each pass slides _gain toward the target by
    // at most GAIN_STEP, interpolated within the pass. A pass = 128 samples (8ms
    // @16kHz) -> from silence to full takes ~4 passes, with no audible pop.
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

        // Expand mono → stereo + linear-interpolation oversample (x AUDIO_OVERSAMPLE):
        // instead of repeating raw samples (zero-order hold), whose staircase wave
        // sounds harsh, interpolate linearly between the current and the next
        // sample: S[i] -> S[i+1] split into AUDIO_OVERSAMPLE even steps.
        // The next sample is looked up ACROSS pass boundaries within the same read
        // buffer, so only the very last sample of the buffer repeats itself.
        for (int i = 0; i < passSamples; i++) {
            int16_t currSample = pcm[base + i];
            int16_t nextSample = (base + i + 1 < totalSamples) ? pcm[base + i + 1] : currSample;
            if (!unity) {
                // |sample × g| >> 15 with g ≤ 32768 never exceeds int16 -> no clamp needed.
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

        // timeout = 0 (non-blocking). 'written' MUST be honored: if the DMA takes
        // only part, count just what was accepted and STOP the loop — the rest of
        // the buffer is read again on the next tick(). Re-reading one pass is far
        // cheaper than keeping state for a half-consumed buffer.
        size_t passBytes = (size_t)passSamples * AUDIO_OVERSAMPLE * 4;
        size_t written   = 0;
        i2s_write(I2S_NUM_0, _stereo, passBytes, &written, 0);
        // Commit even when the DMA took only part: the re-read continues from gEnd; a
        // one-pass gain mismatch only happens while the volume is changing and is inaudible.
        _gain = gEnd;

        // Convert back: 1 source mono sample = AUDIO_OVERSAMPLE * 4 bytes of stereo
        // output -> 1 source mono byte = AUDIO_OVERSAMPLE * 2 bytes of stereo output
        written &= ~(size_t)3;
        _audioCursor += (uint32_t)(written / (AUDIO_OVERSAMPLE * 2));

        if (written < passBytes) return false; // the DMA is full
    }

    return true;
}
