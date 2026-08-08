#include "ScreenLogger.h"
#include "DisplayDriver.h"
#include <string.h>
#include <stdio.h>

// Static member definitions
DisplayDriver* ScreenLogger::_display = nullptr;
portMUX_TYPE   ScreenLogger::_mux     = portMUX_INITIALIZER_UNLOCKED;
char           ScreenLogger::_buf[SCREEN_LOG_LINES][SCREEN_LOG_COL_MAX + 1] = {};
uint8_t        ScreenLogger::_head    = 0;

void ScreenLogger::init(DisplayDriver* display) {
    _display = display;
}

void ScreenLogger::log(const char* fmt, ...) {
    char line[SCREEN_LOG_COL_MAX + 1];
    va_list args;
    va_start(args, fmt);
    vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);

    pushLine(line);
    render();
}

void ScreenLogger::pushLine(const char* line) {
    // Spinlock: không block, không switch context — an toàn trong mọi task
    portENTER_CRITICAL(&_mux);
    _head = (_head + 1) % SCREEN_LOG_LINES;
    strncpy(_buf[_head], line, SCREEN_LOG_COL_MAX);
    _buf[_head][SCREEN_LOG_COL_MAX] = '\0';
    portEXIT_CRITICAL(&_mux);
}

void ScreenLogger::render() {
    if (_display == nullptr) return;

    // Timeout ngắn 50ms: nếu SPI đang bận render video frame, bỏ qua lần này.
    // Buffer vẫn được cập nhật và sẽ hiện ở lần log kế tiếp.
    if (!_display->acquireSPI(50)) return;

    drawOverlay();
    _display->releaseSPI();
}

void ScreenLogger::drawOverlay() {
    auto* tft = _display->getTFT();

    // Nền tối bán-trong-suốt cho vùng log
    tft->fillRect(0, SCREEN_LOG_Y_START, 240, SCREEN_LOG_HEIGHT, 0x0841);

    tft->setTextSize(1);
    tft->setTextDatum(lgfx::top_left);

    for (int i = 0; i < SCREEN_LOG_LINES; i++) {
        // i=0: dòng cũ nhất, i=SCREEN_LOG_LINES-1: dòng mới nhất
        int     idx      = (_head + 1 + i) % SCREEN_LOG_LINES;
        int     y        = SCREEN_LOG_Y_START + 2 + i * 13;
        bool    isNewest = (i == SCREEN_LOG_LINES - 1);
        uint32_t color   = isNewest ? TFT_CYAN : 0x7BEF; // Cyan mới | Trắng mờ cũ
        tft->setTextColor(color, 0x0841);
        tft->drawString(_buf[idx], 2, y);
    }
}
