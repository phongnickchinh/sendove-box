#include "ScreenLogger.h"
#include "DisplayDriver.h"
#include "SdLog.h"
#include <string.h>
#include <stdio.h>

// Static member definitions
DisplayDriver* ScreenLogger::_display = nullptr;
portMUX_TYPE   ScreenLogger::_mux     = portMUX_INITIALIZER_UNLOCKED;
char           ScreenLogger::_buf[SCREEN_LOG_LINES][SCREEN_LOG_COL_MAX + 1] = {};
uint8_t        ScreenLogger::_head    = 0;
volatile bool  ScreenLogger::_overlayEnabled = true;

void ScreenLogger::init(DisplayDriver* display) {
    _display = display;
    _overlayEnabled = true;
}

void ScreenLogger::log(const char* fmt, ...) {
    char line[SCREEN_LOG_COL_MAX + 1];
    va_list args;
    va_start(args, fmt);
    vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);

    pushLine(line);
    SdLog::add(line);  // RAM copy only; the card write happens in SdLog::flush() (see SdLog.h)
    render();
}

void ScreenLogger::pushLine(const char* line) {
    // Spinlock: no blocking, no context switch — safe from any task
    portENTER_CRITICAL(&_mux);
    _head = (_head + 1) % SCREEN_LOG_LINES;
    strncpy(_buf[_head], line, SCREEN_LOG_COL_MAX);
    _buf[_head][SCREEN_LOG_COL_MAX] = '\0';
    portEXIT_CRITICAL(&_mux);
}

void ScreenLogger::render() {
    if (_display == nullptr || !_overlayEnabled) return;

    // Short 50ms timeout: if SPI is busy rendering a video frame, skip this draw.
    // The buffer is still updated and shows on the next log call.
    if (!_display->acquireSPI(50)) return;

    drawOverlay();
    _display->releaseSPI();
}

void ScreenLogger::drawOverlay() {
    auto* tft = _display->getTFT();

    // Dark background for the log area
    tft->fillRect(0, SCREEN_LOG_Y_START, 240, SCREEN_LOG_HEIGHT, 0x0841);

    tft->setTextSize(1);
    tft->setTextDatum(lgfx::top_left);

    for (int i = 0; i < SCREEN_LOG_LINES; i++) {
        // i=0: oldest line, i=SCREEN_LOG_LINES-1: newest line
        int     idx      = (_head + 1 + i) % SCREEN_LOG_LINES;
        int     y        = SCREEN_LOG_Y_START + 2 + i * 13;
        bool    isNewest = (i == SCREEN_LOG_LINES - 1);
        uint32_t color   = isNewest ? TFT_CYAN : 0x7BEF; // newest: cyan | older: dim white
        tft->setTextColor(color, 0x0841);
        tft->drawString(_buf[idx], 2, y);
    }
}
