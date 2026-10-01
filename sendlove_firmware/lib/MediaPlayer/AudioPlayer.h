#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include <Arduino.h>
#include <driver/i2s.h>
#include "IStorageProvider.h"
#include "config.h"

// ============================================================================
// AudioPlayer — plays raw 16-bit mono PCM over I2S DMA (MAX98357A)
// ============================================================================
// Non-blocking and tick-based: call tick() once per video frame to top up the
// DMA buffer. The I2S DMA plays continuously in hardware without blocking the CPU.
// ============================================================================

class AudioPlayer {
public:
    /// Install the I2S driver with the constants from config.h
    bool init();

    /// Stop I2S and uninstall the driver
    void stop();

    /// Short beep to test the speaker at boot
    void testBeep();

    /// A ~1.6kHz sine beep of durationMs; BLOCKS until it finishes (+200ms to drain
    /// the DMA). Only call while not playing (it shares I2S with tick()).
    void beep(uint32_t durationMs);

    /// Look for the AUDC header in storage starting at byte videoDataSize.
    /// appendedSize = audio bytes recorded in the slot table (incl. the AUDC header), 0 = unknown.
    /// Returns true if valid audio was found.
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

    /// Volume 0..100 (Settings::volumeGainQ15). Changing it mid-playback slides the
    /// gain over a few tens of ms (no pop). 100 = the unscaled path, no multiply.
    /// immediate = true: jump straight to the new level (start of a track, before
    /// any sample reaches the speaker).
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

    // PCM read buffer. The READ size (AUDIO_READ_CHUNK_SIZE) is separate from the
    // oversampling chunk (AUDIO_PCM_CHUNK_SIZE) because on an SD card every read
    // is an fread through VFS/FATFS — see config.h.
    uint8_t _chunk[AUDIO_READ_CHUNK_SIZE];
    // Mono -> stereo (x2), then each sample repeated AUDIO_OVERSAMPLE times (I2S
    // runs at the file rate x AUDIO_OVERSAMPLE, see config.h) so BCLK is high
    // enough for the MAX98357A.
    int16_t _stereo[AUDIO_PCM_CHUNK_SIZE / 2 * 2 * AUDIO_OVERSAMPLE];

    /// Read one PCM chunk from the source and write it to I2S (mono → stereo).
    /// Returns true if the DMA took the whole chunk (room left, keep filling).
    bool fillChunk();
};

#endif // AUDIO_PLAYER_H
