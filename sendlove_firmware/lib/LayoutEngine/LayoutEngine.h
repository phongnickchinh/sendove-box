#ifndef LAYOUT_ENGINE_H
#define LAYOUT_ENGINE_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "DisplayDriver.h"
#include "NetworkManager.h"
#include <vector>

enum WidgetType {
    WIDGET_CLOCK_TIME,
    WIDGET_CLOCK_DATE,
    WIDGET_WIFI_ICON,
    WIDGET_BATTERY_ICON,
    WIDGET_IMAGE
};

struct WidgetConfig {
    WidgetType type;
    int16_t x;
    int16_t y;
    uint16_t color;
    String align;
    String font;
    String format;
    String src;
};

class LayoutEngine {
public:
    LayoutEngine() = default;

    // Load cấu hình JSON từ chuỗi (hoặc sau này từ file)
    bool loadConfig(const char* jsonString);

    // Render toàn bộ màn hình chờ
    void renderStandbyScreen(DisplayDriver* display, NetworkManager* network, bool fullRedraw = false);

private:
    std::vector<WidgetConfig> _widgets;
    uint16_t _bgColor = TFT_BLACK;
    
    // Hàm chuyển mã màu HEX (#FFFFFF) sang màu RGB565
    uint16_t hexToColor(const char* hex);

    // Vẽ từng widget con
    void drawClockTime(LGFX* canvas, const WidgetConfig& cfg, NetworkManager* network);
    void drawClockDate(LGFX* canvas, const WidgetConfig& cfg, NetworkManager* network);
    void drawWifiIcon(LGFX* canvas, const WidgetConfig& cfg, NetworkManager* network);
    void drawBatteryIcon(LGFX* canvas, const WidgetConfig& cfg);
};

#endif // LAYOUT_ENGINE_H
