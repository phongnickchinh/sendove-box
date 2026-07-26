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
    WIDGET_IMAGE,
    WIDGET_CHIP_TEMP
};

struct WidgetConfig {
    WidgetType type;
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
    uint16_t color;
    String align;
    String font;
    String format;
    String src;
};

/// Dynamic UI layout engine for rendering JSON-configured widgets
class LayoutEngine {
public:
    LayoutEngine() = default;

    /// Parse JSON configuration string into widget list
    bool loadConfig(const char* jsonString);

    /// Render standby UI screen widgets
    void renderStandbyScreen(DisplayDriver* display, NetworkManager* network, bool fullRedraw = false);

    /// Invalidate cached widget state to force full redraw of all widgets
    void invalidateCache();

    /// Draw a section of the static background image at (x, y, w, h)
    void drawBackgroundPatch(LGFX* canvas, int32_t x, int32_t y, int32_t w, int32_t h);

private:
    std::vector<WidgetConfig> _widgets;
    uint16_t _bgColor = TFT_BLACK;

    // Cache variables for Dirty Flag optimization
    char _lastTimeStr[16] = "";
    char _lastDateStr[32] = "";
    int    _lastRssiBars = -1;
    int    _lastBatPercent = -1;
    int    _lastChipTemp = -999;
    
    /// Convert HEX color string (#FFFFFF) to RGB565 format
    uint16_t hexToColor(const char* hex);

    void drawClockTime(LGFX* canvas, const WidgetConfig& cfg, NetworkManager* network, bool force);
    void drawClockDate(LGFX* canvas, const WidgetConfig& cfg, NetworkManager* network, bool force);
    void drawWifiIcon(LGFX* canvas, const WidgetConfig& cfg, NetworkManager* network, bool force);
    void drawBatteryIcon(LGFX* canvas, const WidgetConfig& cfg, bool force);
    void drawChipTemp(LGFX* canvas, const WidgetConfig& cfg, bool force);
};

#endif // LAYOUT_ENGINE_H
