#ifndef MEDIA_PLAYER_H
#define MEDIA_PLAYER_H

#include <Arduino.h>
#include <JPEGDEC.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "IStorageProvider.h"
#include "AudioPlayer.h"
#include "config.h"

class DisplayDriver;

// ============================================================================
// MediaPlayer — plays VJPG video / VIMG images from IStorageProvider (NAND / SD)
// ============================================================================
// Serves Task_MediaPlayer:
// - reads JPEG frames from IStorageProvider
// - decodes with JPEGDEC → the callback pushes pixels to DisplayDriver
// - two modes: VJPG (video, looping) and VIMG (still image)
// ============================================================================

/// Playback state
enum class PlaybackState : uint8_t {
    IDLE,
    PLAYING,
    SHOWING,
    ERROR
};

/// Video (VJPG) and Image (VIMG) player from Storage Provider
class MediaPlayer {
public:
    ~MediaPlayer();

    /// Initialize MediaPlayer instance
    bool init(IStorageProvider* storage, DisplayDriver* display);

    /// Start playing media from item ID
    bool playItem(const char* identifier);

    /// Update playback loop frame timing
    void update();

    /// Stop current playback
    void stop();

    /// Test I2S speaker beep
    void testAudioBeep();

    /// One alarm beep (blocks ~0.6s) at volume 0..100. Call while the player is IDLE.
    void alarmBeep(uint8_t volume);

    /// Play alarm music (looping) from a file on the card. false = it couldn't be
    /// opened/read -> the caller falls back to the beep (the single fallback path; mandatory case, MEMORY.md §28).
    bool startAlarmMusic(const char* path, uint8_t volume);
    /// Call every loop while ringing: updates the volume (ramp) + refills the DMA. Non-blocking.
    void tickAlarmMusic(uint8_t volume);
    bool isAlarmMusicPlaying() const { return _alarmMusic; }


    /// Get current playback state
    PlaybackState getState() const;

    /// Get current active slot index (-1 if IDLE)
    int8_t getCurrentSlot() const;

private:
    static constexpr size_t JPEG_BUFFER_SIZE = 32 * 1024;

    // Minimum idle time between two decodes. Prevents back-to-back frames — that
    // is when the current draw spikes and the supply sags.
    static constexpr uint32_t FRAME_MIN_IDLE_MS = 2;

    IStorageProvider* _storage = nullptr;
    DisplayDriver*    _display = nullptr;
    PlaybackState     _state   = PlaybackState::IDLE;
    SemaphoreHandle_t _playerMutex = nullptr;

    // 17,884 bytes — the largest piece of the whole appCtx (24,508 bytes of static
    // RAM), yet only alive inside _jpeg->decode(). As a direct member it would hold
    // 17.9KB of BSS for the box's whole uptime — exactly the RAM mbedTLS needs as a
    // ~16KB contiguous block while the box sits on standby doing a TLS handshake
    // (MEMORY.md §21). It is allocated/freed together with _jpegBuffer: peak RAM
    // during playback is unchanged, and the idle state gets the memory back.
    // Safe: openRAM() starts with memset(&_jpeg, 0, sizeof(JPEGIMAGE)), so a
    // heap object with garbage is fine — it doesn't rely on zeroed BSS.
    JPEGDEC* _jpeg          = nullptr;
    uint8_t* _jpegBuffer    = nullptr;
    int8_t   _currentSlot   = -1;
    char     _currentId[32] = "";
    uint16_t _fps          = 10;
    uint16_t _totalFrames  = 0;
    uint16_t _currentFrame = 0;
    uint32_t _nextFrameDeadline = 0;   // millis() deadline of the next frame; accumulated so it doesn't drift
    uint32_t _currentDataSize = 0;
    uint32_t _currentAudioSize = 0;   // audio bytes appended after the video (from SlotEntry.audioSize)
    uint32_t _frameBaseOffset = 0;
    bool     _readFrameSizeHeader = true;
    bool     _lastFrameSkipped = false;  // never skip two frames in a row

    bool     _alarmMusic    = false;  // playing alarm music (no video)

    bool     _isSlbxRgb565  = false;
    uint16_t _slbxWidth     = 128;
    uint16_t _slbxHeight    = 160;

    /// Decode and render single JPEG frame.
    /// skipRender = true: still consume the frame's bytes to keep the file position,
    /// but skip the expensive part: decoding the JPEG and pushing the frame over SPI.
    bool decodeOneFrame(bool skipRender);

    /// Callback function for JPEGDEC pixel output
    static int jpegDrawCallback(JPEGDRAW* pDraw);

    AudioPlayer _audio;
};

#endif // MEDIA_PLAYER_H
