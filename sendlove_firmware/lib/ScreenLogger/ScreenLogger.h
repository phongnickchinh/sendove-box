#pragma once
#include <Arduino.h>
#include <stdarg.h>

// Forward declare để tránh circular include với DisplayDriver
class DisplayDriver;

// Layout của overlay log trên màn hình 240x240
static constexpr uint8_t  SCREEN_LOG_LINES   = 5;
static constexpr uint8_t  SCREEN_LOG_COL_MAX = 38;
static constexpr uint16_t SCREEN_LOG_Y_START = 170;
static constexpr uint16_t SCREEN_LOG_HEIGHT  = 70;

/// Singleton ghi log trực tiếp lên màn hình TFT.
/// Thread-safe: ring buffer bảo vệ bằng spinlock, render cố gắng lấy SPI mutex 50ms.
class ScreenLogger {
public:
    /// Gọi một lần sau display.init() trong main.cpp
    static void init(DisplayDriver* display);

    /// Ghi một dòng log (printf-style). Thread-safe.
    static void log(const char* fmt, ...);

    /// Bật / tắt vẽ overlay lên màn hình TFT (tắt khi phát video để giải phóng SPI và tránh giật hình)
    static void setOverlayEnabled(bool enabled) { _overlayEnabled = enabled; }
    static bool isOverlayEnabled() { return _overlayEnabled; }

    /// Force render lại toàn bộ overlay (không ghi dòng mới).
    static void render();

private:
    static DisplayDriver* _display;
    static portMUX_TYPE   _mux;
    static char           _buf[SCREEN_LOG_LINES][SCREEN_LOG_COL_MAX + 1];
    static uint8_t        _head; // Index của dòng MỚI NHẤT trong ring buffer
    static volatile bool  _overlayEnabled;

    static void pushLine(const char* line);
    static void drawOverlay();
};

/// Macro tiện dụng — dùng thay cho Serial.printf/println trong mọi file
#define DLOG(fmt, ...) ScreenLogger::log(fmt, ##__VA_ARGS__)
