#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include <Arduino.h>
#include <driver/i2s.h>
#include "IStorageProvider.h"
#include "config.h"

// AudioPlayer — 16-bit mono PCM over I2S DMA (MAX98357A). Non-blocking: call
// tick() regularly to top up the DMA buffer.

class AudioPlayer {
public:
    /// Install the I2S driver with the constants from config.h
    bool init();

    /// Stop I2S and uninstall the driver
    void stop();

    /// Short beep to test the speaker at boot
    void testBeep();

    /// A ~1.6kHz beep; BLOCKS for durationMs + 200ms. Only call while not playing.
    void beep(uint32_t durationMs);

    /// Look for the AUDC header at byte videoDataSize. appendedSize = audio bytes
    /// per the slot table (0 = unknown). true = valid audio found.
    bool loadFromStorage(IStorageProvider* storage, uint32_t videoDataSize, uint32_t appendedSize = 0);

    /// Alarm music: a standalone file on the card (AUDC + WAV + PCM); holds
    /// SDCardManager's random-read handle until stop(). loop = restart at the end.
    bool loadFromFile(class SDCardManager* card, const char* path, bool loop);

    /// Clear the DMA and fill it before playing (also called at each loop restart).
    void prefill();

    /// Call every frame to refill the I2S DMA buffer. Non-blocking.
    void tick();

    bool hasAudio()     const { return _hasAudio; }
    bool isInitialized() const { return _initialized; }

    /// Volume 0..100; 100 = the unscaled path. Mid-playback the gain slides (no
    /// pop); immediate = jump to the level (start of a track).
    void setVolume(uint8_t vol, bool immediate = false);

private:
    // 10 bytes: "AUDC" (4) + sampleRate uint16 (2) + pcmSize uint32 (4)
    static constexpr size_t AUDC_HEADER_SIZE = 10;
    // Standard 44-byte RIFF/WAVE header produced by mediaEncoder.js
    static constexpr size_t WAV_HEADER_SIZE = 44;

    IStorageProvider* _storage      = nullptr;
    class SDCardManager* _card      = nullptr;  // file source (alarm music), used instead of _storage
    bool              _loop         = false;
    bool              _hasAudio     = false;
    bool              _initialized  = false;

    uint32_t _audioPcmOffset = 0; // absolute offset in the slot: after the video + the 10-byte AUDC header
    uint32_t _audioPcmSize   = 0; // total PCM bytes
    uint32_t _audioCursor    = 0; // bytes already fed to the DMA
    uint32_t _sampleRate     = AUDIO_SAMPLE_RATE; // read from the AUDC header, not hardcoded

    // Q15 volume gain. 32768 = the unscaled path (the multiply is skipped).
    int32_t _gain       = 32768;  // in use
    int32_t _gainTarget = 32768;  // target; fillChunk() slides _gain toward it
    bool    _muted      = false;  // volume 0: shut the amp down (PIN_AMP_SD) if wired

    /// Switch the amp through SD_MODE. No-op if PIN_AMP_SD < 0.
    void setAmp(bool on);

    /// Read at an absolute offset from the current source (message slot or music file).
    int readSrc(uint32_t offset, uint8_t* buf, uint32_t len);
    /// Parse the AUDC header at audioStartOffset (+ WAV if present) and set _audioPcmOffset/_Size.
    bool parseAudc(uint32_t audioStartOffset, uint32_t appendedSize, uint32_t maxPcm);

    // PCM read buffer (read size vs oversampling chunk: see config.h).
    uint8_t _chunk[AUDIO_READ_CHUNK_SIZE];
    // Mono -> stereo, oversampled x AUDIO_OVERSAMPLE (see config.h).
    int16_t _stereo[AUDIO_PCM_CHUNK_SIZE / 2 * 2 * AUDIO_OVERSAMPLE];

    /// Read one PCM chunk and write it to I2S. true = the DMA took it all (keep filling).
    bool fillChunk();
};

#endif // AUDIO_PLAYER_H
