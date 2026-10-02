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

// MediaPlayer — plays VJPG video (looping) / VIMG still images from
// IStorageProvider, decoded with JPEGDEC straight to DisplayDriver.

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

    /// Play alarm music (looping) from the card. false = the caller falls back to
    /// the beep, the single fallback path (mandatory case, MEMORY.md §28).
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

    // Minimum idle time between two decodes (back-to-back frames sag the supply).
    static constexpr uint32_t FRAME_MIN_IDLE_MS = 2;

    IStorageProvider* _storage = nullptr;
    DisplayDriver*    _display = nullptr;
    PlaybackState     _state   = PlaybackState::IDLE;
    SemaphoreHandle_t _playerMutex = nullptr;

    // 17.9KB, needed only while decoding: heap-allocated together with _jpegBuffer
    // instead of sitting in BSS, so standby keeps that RAM for TLS (MEMORY.md §21).
    // openRAM() zeroes the state itself, so an uninitialised heap object is fine.
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

    /// Decode and render one frame. skipRender = consume the frame's bytes but skip
    /// the decode + push.
    bool decodeOneFrame(bool skipRender);

    /// Callback function for JPEGDEC pixel output
    static int jpegDrawCallback(JPEGDRAW* pDraw);

    AudioPlayer _audio;
};

#endif // MEDIA_PLAYER_H
