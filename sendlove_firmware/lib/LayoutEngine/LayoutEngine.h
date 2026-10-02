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
    String font;    // "f_time"/"f_date" = VLW fonts in the theme package; "Font7"/"Font2" = built-in
    String format;  // clock_date: "WD, DD.MM" | "DD/MM/YYYY" | "WD DD.MM"
    String locale;  // clock_date: "vi" | "en"
};

/// Renders the standby screen from the JSON theme in ThemeStore (MEMORY.md §28),
/// or a black fallback without one. Task_MediaPlayer only (single owner, no mutex).
class LayoutEngine {
public:
    LayoutEngine() = default;

    /// Load the theme in ThemeStore; call again after each install. false = fallback.
    bool loadTheme();

    /// Render standby UI screen widgets
    void renderStandbyScreen(DisplayDriver* display, NetworkManager* network, bool fullRedraw = false);

    /// The ringing-alarm screen: large time in the middle + a hint line (ASCII)
    void renderAlarmScreen(DisplayDriver* display, const char* timeStr, const char* hint);

    /// Invalidate cached widget state to force full redraw of all widgets
    void invalidateCache();

    /// Draw a section of the static background image at (x, y, w, h)
    void drawBackgroundPatch(LGFX* canvas, int32_t x, int32_t y, int32_t w, int32_t h);

private:
    std::vector<WidgetConfig> _widgets;
    bool _fallback = true;

    // Theme assets in flash (mmap). nullptr = absent -> black background / built-in fonts.
    const uint16_t* _bg = nullptr;
    // VLW fonts read from flash; the wrappers must outlive the fonts (read on EVERY draw).
    lgfx::PointerWrapper _pwTime, _pwDate;
    lgfx::VLWfont _vlwTime, _vlwDate;
    bool _hasVlwTime = false;
    bool _hasVlwDate = false;
    UBaseType_t _stackMin = 0xFFFFFFFF;  // lowest remaining stack printed so far (see render)

    // Cache variables for Dirty Flag optimization
    char _lastTimeStr[16] = "";
    char _lastDateStr[48] = "";
    int    _lastRssiBars = -1;
    int    _lastBatPercent = -1;
    int    _lastChipTemp = -999;

    /// Convert HEX color string (#FFFFFF) to RGB565 format
    uint16_t hexToColor(const char* hex);

    bool parseWidgets(const char* json, size_t len);
    void useFallbackWidgets();
    void unloadFonts();
    /// A widget's font: VLW if the package has it, otherwise the built-in font for its type.
    const lgfx::IFont* fontFor(const WidgetConfig& cfg) const;
    bool isVlw(const lgfx::IFont* f) const { return f == &_vlwTime || f == &_vlwDate; }
    /// The date per format/locale. unicode = false -> ASCII weekday names (built-in fonts have no diacritics).
    void formatDate(const WidgetConfig& cfg, bool unicode, char* out, size_t len) const;

    void drawTextWidget(LGFX* canvas, const WidgetConfig& cfg, const char* text, int32_t defaultW, int32_t defaultH, const lgfx::IFont* font = nullptr);
    void drawClockTime(LGFX* canvas, const WidgetConfig& cfg, NetworkManager* network, bool force);
    void drawClockDate(LGFX* canvas, const WidgetConfig& cfg, NetworkManager* network, bool force);
    void drawWifiIcon(LGFX* canvas, const WidgetConfig& cfg, NetworkManager* network, bool force);
    void drawBatteryIcon(LGFX* canvas, const WidgetConfig& cfg, bool force);
    void drawChipTemp(LGFX* canvas, const WidgetConfig& cfg, bool force);
};

#endif // LAYOUT_ENGINE_H
