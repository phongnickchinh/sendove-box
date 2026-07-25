#include "LayoutEngine.h"

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
    
    return true;
}

void LayoutEngine::renderStandbyScreen(DisplayDriver* display, NetworkManager* network, bool fullRedraw) {
    if (!display || !network) return;

    LGFX* tft = display->getTFT();
    display->acquireSPI();

    if (fullRedraw) tft->fillScreen(_bgColor);

    for (const auto& cfg : _widgets) {
        switch (cfg.type) {
            case WIDGET_CLOCK_TIME:
                drawClockTime(tft, cfg, network);
                break;
            case WIDGET_CLOCK_DATE:
                drawClockDate(tft, cfg, network);
                break;
            case WIDGET_WIFI_ICON:
                drawWifiIcon(tft, cfg, network);
                break;
            case WIDGET_BATTERY_ICON:
                drawBatteryIcon(tft, cfg);
                break;
            case WIDGET_IMAGE:
                break;
        }
    }

    display->releaseSPI();
}

void LayoutEngine::drawClockTime(LGFX* canvas, const WidgetConfig& cfg, NetworkManager* network) {
    if (cfg.font == "Orbitron_32") {
        canvas->setFont(&fonts::Orbitron_Light_32);
    } else {
        canvas->setFont(&fonts::Font7);
        canvas->setTextSize(2);
    }

    if (cfg.align == "center") canvas->setTextDatum(lgfx::middle_center);
    else if (cfg.align == "right") canvas->setTextDatum(lgfx::middle_right);
    else canvas->setTextDatum(lgfx::middle_left);

    canvas->setTextColor(cfg.color, _bgColor);
    String timeStr = network->getTimeString();
    canvas->drawString(timeStr.c_str(), cfg.x, cfg.y);
    
    canvas->setTextSize(1);
    canvas->setFont(nullptr);
}

void LayoutEngine::drawClockDate(LGFX* canvas, const WidgetConfig& cfg, NetworkManager* network) {
    if (cfg.font == "Roboto_14") {
        canvas->setFont(&fonts::Roboto_Thin_24);
    } else {
        canvas->setFont(&fonts::FreeSansBold12pt7b);
    }

    if (cfg.align == "center") canvas->setTextDatum(lgfx::middle_center);
    else if (cfg.align == "right") canvas->setTextDatum(lgfx::middle_right);
    else canvas->setTextDatum(lgfx::middle_left);

    canvas->setTextColor(cfg.color, _bgColor);
    String dateStr = network->getDateString();
    canvas->drawString(dateStr.c_str(), cfg.x, cfg.y);
    canvas->setFont(nullptr);
}

void LayoutEngine::drawWifiIcon(LGFX* canvas, const WidgetConfig& cfg, NetworkManager* network) {
    int rssi = network->getWifiRSSI();
    int bars = (rssi > -60) ? 4 : (rssi > -70) ? 3 : (rssi > -80) ? 2 : (rssi > -100) ? 1 : 0;

    for (int i = 0; i < 4; i++) {
        uint16_t color = (i < bars) ? cfg.color : TFT_DARKGREY;
        canvas->fillRect(cfg.x + (i * 5), cfg.y + (12 - (i * 3)), 3, (i * 3) + 3, color);
    }
}

void LayoutEngine::drawBatteryIcon(LGFX* canvas, const WidgetConfig& cfg) {
    int percentage = 80;

    canvas->drawRect(cfg.x, cfg.y, 24, 12, cfg.color);
    canvas->fillRect(cfg.x + 24, cfg.y + 3, 2, 6, cfg.color);
    
    uint16_t color = (percentage <= 20) ? TFT_RED : TFT_GREEN;
    int fillWidth = (20 * percentage) / 100;
    canvas->fillRect(cfg.x + 2, cfg.y + 2, fillWidth, 8, color);
    canvas->fillRect(cfg.x + 2 + fillWidth, cfg.y + 2, 20 - fillWidth, 8, _bgColor);
}
