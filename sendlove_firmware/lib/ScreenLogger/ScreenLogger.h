#pragma once
#include <Arduino.h>
#include <stdarg.h>

// Forward declaration to avoid a circular include with DisplayDriver
class DisplayDriver;

// Layout of the log overlay on the 240x240 screen
static constexpr uint8_t  SCREEN_LOG_LINES   = 5;
static constexpr uint8_t  SCREEN_LOG_COL_MAX = 38;
static constexpr uint16_t SCREEN_LOG_Y_START = 170;
static constexpr uint16_t SCREEN_LOG_HEIGHT  = 70;

/// Singleton that logs straight to the TFT screen.
/// Thread-safe: the ring buffer is guarded by a spinlock; render tries the SPI mutex for 50ms.
class ScreenLogger {
public:
    /// Call once after display.init() in main.cpp
    static void init(DisplayDriver* display);

    /// Log one line (printf-style). Thread-safe.
    static void log(const char* fmt, ...);

    /// Enable / disable drawing the overlay (off during video playback to free SPI and avoid stutter)
    static void setOverlayEnabled(bool enabled) { _overlayEnabled = enabled; }
    static bool isOverlayEnabled() { return _overlayEnabled; }

    /// Force a redraw of the whole overlay (without adding a line).
    static void render();

private:
    static DisplayDriver* _display;
    static portMUX_TYPE   _mux;
    static char           _buf[SCREEN_LOG_LINES][SCREEN_LOG_COL_MAX + 1];
    static uint8_t        _head; // index of the NEWEST line in the ring buffer
    static volatile bool  _overlayEnabled;

    static void pushLine(const char* line);
    static void drawOverlay();
};

/// Convenience macro — use instead of Serial.printf/println everywhere
#define DLOG(fmt, ...) ScreenLogger::log(fmt, ##__VA_ARGS__)
