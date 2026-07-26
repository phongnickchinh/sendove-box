#include "LayoutEngine.h"
#include "ChakraPetch_SemiBold_48.h"
#include "ChakraPetch_SemiBold_16.h"
#include "CustomWifiIcons.h"
#include "StandbyBackground.h"

uint16_t LayoutEngine::hexToColor(const char* hex) {
    if (hex == nullptr || strlen(hex) < 7 || hex[0] != '#') return TFT_WHITE;
    long rgb = strtol(hex + 1, nullptr, 16);
    return lgfx::color565((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
}

bool LayoutEngine::loadConfig(const char* jsonString) {
    _widgets.clear();
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonString);
    if (error) return false;

    JsonArray widgets = doc["widgets"];
    for (JsonObject widget : widgets) {
        WidgetConfig cfg;
        const char* type = widget["type"];
        
        if (strcmp(type, "clock_time") == 0) cfg.type = WIDGET_CLOCK_TIME;
        else if (strcmp(type, "clock_date") == 0) cfg.type = WIDGET_CLOCK_DATE;
        else if (strcmp(type, "wifi_icon") == 0) cfg.type = WIDGET_WIFI_ICON;
        else if (strcmp(type, "battery_icon") == 0) cfg.type = WIDGET_BATTERY_ICON;
        else if (strcmp(type, "image") == 0) cfg.type = WIDGET_IMAGE;
        else continue;

        cfg.x = widget["x"] | 0;
        cfg.y = widget["y"] | 0;
        cfg.color = hexToColor(widget["color"] | "#FFFFFF");
        cfg.align = widget["align"] | "left";
        cfg.font = widget["font"] | "default";
        cfg.format = widget["format"] | "";
        cfg.src = widget["src"] | "";

        _widgets.push_back(cfg);
    }
    
    Serial.printf("[LayoutEngine] Loaded %d widgets\n", _widgets.size());
    return true;
}

void LayoutEngine::invalidateCache() {
    _lastTimeStr = "";
    _lastDateStr = "";
    _lastRssiBars = -1;
    _lastBatPercent = -1;
}

void LayoutEngine::drawBackgroundPatch(LGFX* canvas, int32_t x, int32_t y, int32_t w, int32_t h) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > BG_WIDTH) w = BG_WIDTH - x;
    if (y + h > BG_HEIGHT) h = BG_HEIGHT - y;
    if (w <= 0 || h <= 0) return;

    for (int32_t r = 0; r < h; r++) {
        int32_t curY = y + r;
        canvas->pushImage(x, curY, w, 1, &StandbyBackground[curY * BG_WIDTH + x]);
    }
}

void LayoutEngine::renderStandbyScreen(DisplayDriver* display, NetworkManager* network, bool fullRedraw) {
    if (!display || !network) return;

    LGFX* tft = display->getTFT();
    display->acquireSPI();

    if (fullRedraw) {
        tft->startWrite();
        tft->pushImage(0, 0, BG_WIDTH, BG_HEIGHT, StandbyBackground);
        tft->endWrite();
        invalidateCache();
    }

    for (const auto& cfg : _widgets) {
        switch (cfg.type) {
            case WIDGET_CLOCK_TIME:
                drawClockTime(tft, cfg, network, fullRedraw);
                break;
            case WIDGET_CLOCK_DATE:
                drawClockDate(tft, cfg, network, fullRedraw);
                break;
            case WIDGET_WIFI_ICON:
                drawWifiIcon(tft, cfg, network, fullRedraw);
                break;
            case WIDGET_BATTERY_ICON:
                drawBatteryIcon(tft, cfg, fullRedraw);
                break;
            case WIDGET_IMAGE:
                break;
        }
    }

    display->releaseSPI();
}

void LayoutEngine::drawClockTime(LGFX* canvas, const WidgetConfig& cfg, NetworkManager* network, bool force) {
    String timeStr = network->getTimeString();
    if (!force && timeStr == _lastTimeStr) return;

    // Bounding Box cho đồng hồ (48pt SemiBold, tâm cfg.x, cfg.y)
    int32_t boxW = 200;
    int32_t boxH = 60;
    int32_t boxX = cfg.x - (boxW / 2);
    int32_t boxY = cfg.y - (boxH / 2);

    canvas->startWrite();
    drawBackgroundPatch(canvas, boxX, boxY, boxW, boxH);

    if (cfg.font == "Orbitron_32") {
        canvas->setFont(&fonts::Orbitron_Light_32);
        canvas->setTextSize(1);
    } else if (cfg.font == "Font7") {
        canvas->setFont(&fonts::Font7);
        canvas->setTextSize(2);
    } else {
        // Mặc định hoặc "ChakraPetch_48" / "ChakraPetch"
        canvas->setFont(&ChakraPetch_SemiBold_48);
        canvas->setTextSize(1);
    }

    if (cfg.align == "center") canvas->setTextDatum(lgfx::middle_center);
    else if (cfg.align == "right") canvas->setTextDatum(lgfx::middle_right);
    else canvas->setTextDatum(lgfx::middle_left);

    canvas->setTextColor(cfg.color);
    canvas->drawString(timeStr.c_str(), cfg.x, cfg.y);

    canvas->setTextSize(1);
    canvas->setFont(nullptr);
    canvas->endWrite();

    _lastTimeStr = timeStr;
}

void LayoutEngine::drawClockDate(LGFX* canvas, const WidgetConfig& cfg, NetworkManager* network, bool force) {
    String dateStr = network->getDateString();
    if (!force && dateStr == _lastDateStr) return;

    // Bounding Box cho ngày tháng (16pt SemiBold, tâm cfg.x, cfg.y)
    int32_t boxW = 230;
    int32_t boxH = 30;
    int32_t boxX = cfg.x - (boxW / 2);
    int32_t boxY = cfg.y - (boxH / 2);

    canvas->startWrite();
    drawBackgroundPatch(canvas, boxX, boxY, boxW, boxH);

    if (cfg.font == "Roboto_14") {
        canvas->setFont(&fonts::Roboto_Thin_24);
    } else if (cfg.font == "FreeSans_12") {
        canvas->setFont(&fonts::FreeSansBold12pt7b);
    } else {
        // Mặc định hoặc "ChakraPetch_16"
        canvas->setFont(&ChakraPetch_SemiBold_16);
    }

    if (cfg.align == "center") canvas->setTextDatum(lgfx::middle_center);
    else if (cfg.align == "right") canvas->setTextDatum(lgfx::middle_right);
    else canvas->setTextDatum(lgfx::middle_left);

    canvas->setTextColor(cfg.color);
    canvas->drawString(dateStr.c_str(), cfg.x, cfg.y);
    canvas->setFont(nullptr);
    canvas->endWrite();

    _lastDateStr = dateStr;
}

void LayoutEngine::drawWifiIcon(LGFX* canvas, const WidgetConfig& cfg, NetworkManager* network, bool force) {
    int rssi = network->getWifiRSSI();
    int bars = 0;
    if (rssi > -60) bars = 4;
    else if (rssi > -70) bars = 3;
    else if (rssi > -80) bars = 2;
    else if (rssi > -100) bars = 1;

    if (!force && bars == _lastRssiBars) return;

    canvas->startWrite();
    drawBackgroundPatch(canvas, cfg.x, cfg.y, WIFI_ICON_WIDTH, WIFI_ICON_HEIGHT);
    canvas->drawBitmap(cfg.x, cfg.y, WIFI_ICONS[bars], WIFI_ICON_WIDTH, WIFI_ICON_HEIGHT, cfg.color);
    canvas->endWrite();

    _lastRssiBars = bars;
}

void LayoutEngine::drawBatteryIcon(LGFX* canvas, const WidgetConfig& cfg, bool force) {
    int percentage = 80;
    if (!force && percentage == _lastBatPercent) return;

    canvas->startWrite();
    drawBackgroundPatch(canvas, cfg.x, cfg.y, 28, 14);

    canvas->drawRect(cfg.x, cfg.y, 24, 12, cfg.color);
    canvas->fillRect(cfg.x + 24, cfg.y + 3, 2, 6, cfg.color);
    
    uint16_t color = (percentage <= 20) ? TFT_RED : (cfg.color == TFT_BLACK ? TFT_BLACK : TFT_GREEN);
    int fillWidth = (20 * percentage) / 100;
    canvas->fillRect(cfg.x + 2, cfg.y + 2, fillWidth, 8, color);

    canvas->endWrite();

    _lastBatPercent = percentage;
}
