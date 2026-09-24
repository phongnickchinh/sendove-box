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
    String font;    // "f_time"/"f_date" = phông VLW trong gói theme; "Font7"/"Font2" = có sẵn
    String format;  // clock_date: "WD, DD.MM" | "DD/MM/YYYY" | "WD DD.MM"
    String locale;  // clock_date: "vi" | "en"
};

/// Dynamic UI layout engine for rendering JSON-configured widgets
///
/// Nền, phông và bố cục KHÔNG còn biên dịch cứng (thiết kế 2026-09-24, MEMORY.md §28):
/// loadTheme() lấy từ ThemeStore (phân vùng flash `theme`, qua mmap). Không có theme thì
/// vẽ màn dự phòng: nền đen, giờ phông 7 đoạn, ngày ASCII không dấu, chữ trắng.
/// Chỉ Task_MediaPlayer gọi các hàm của lớp này (một chủ sở hữu, không cần mutex).
class LayoutEngine {
public:
    LayoutEngine() = default;

    /// Nạp theme đang có trong ThemeStore (hoặc dự phòng). Gọi lại sau mỗi lần cài theme.
    /// false = đang ở màn dự phòng.
    bool loadTheme();

    /// Render standby UI screen widgets
    void renderStandbyScreen(DisplayDriver* display, NetworkManager* network, bool fullRedraw = false);

    /// Man hinh bao thuc dang keu: gio lon o giua + dong goi y thao tac (ASCII)
    void renderAlarmScreen(DisplayDriver* display, const char* timeStr, const char* hint);

    /// Invalidate cached widget state to force full redraw of all widgets
    void invalidateCache();

    /// Draw a section of the static background image at (x, y, w, h)
    void drawBackgroundPatch(LGFX* canvas, int32_t x, int32_t y, int32_t w, int32_t h);

private:
    std::vector<WidgetConfig> _widgets;
    bool _fallback = true;

    // Asset theme trong flash (mmap). nullptr = không có -> nền đen / phông có sẵn.
    const uint16_t* _bg = nullptr;
    // Phông VLW đọc thẳng từ con trỏ flash. Wrapper phải sống cùng phông: VLWfont đọc
    // glyph qua nó MỖI LẦN VẼ, không chỉ lúc nạp.
    lgfx::PointerWrapper _pwTime, _pwDate;
    lgfx::VLWfont _vlwTime, _vlwDate;
    bool _hasVlwTime = false;
    bool _hasVlwDate = false;
    UBaseType_t _stackMin = 0xFFFFFFFF;  // mức stack còn lại thấp nhất đã in (xem render)

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
    /// Phông cho widget: VLW nếu gói có, không thì phông có sẵn theo loại widget.
    const lgfx::IFont* fontFor(const WidgetConfig& cfg) const;
    bool isVlw(const lgfx::IFont* f) const { return f == &_vlwTime || f == &_vlwDate; }
    /// Ngày theo format/locale. unicode = false -> tên thứ ASCII (phông có sẵn không có dấu).
    void formatDate(const WidgetConfig& cfg, bool unicode, char* out, size_t len) const;

    void drawTextWidget(LGFX* canvas, const WidgetConfig& cfg, const char* text, int32_t defaultW, int32_t defaultH, const lgfx::IFont* font = nullptr);
    void drawClockTime(LGFX* canvas, const WidgetConfig& cfg, NetworkManager* network, bool force);
    void drawClockDate(LGFX* canvas, const WidgetConfig& cfg, NetworkManager* network, bool force);
    void drawWifiIcon(LGFX* canvas, const WidgetConfig& cfg, NetworkManager* network, bool force);
    void drawBatteryIcon(LGFX* canvas, const WidgetConfig& cfg, bool force);
    void drawChipTemp(LGFX* canvas, const WidgetConfig& cfg, bool force);
};

#endif // LAYOUT_ENGINE_H
