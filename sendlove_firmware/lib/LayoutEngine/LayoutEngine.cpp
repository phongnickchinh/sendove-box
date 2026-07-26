#include "LayoutEngine.h"
#include "ChakraPetch_SemiBold_16.h"
#include "ChakraPetch_SemiBold_48.h"
#include "CustomBatteryIcons.h"
#include "CustomWifiIcons.h"
#include "StandbyBackground.h"
#include "SystemMonitor.h"

uint16_t LayoutEngine::hexToColor(const char *hex) {
  if (hex == nullptr || strlen(hex) < 7 || hex[0] != '#')
    return TFT_WHITE;
  long rgb = strtol(hex + 1, nullptr, 16);
  return lgfx::color565((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
}

bool LayoutEngine::loadConfig(const char *jsonString) {
  _widgets.clear();

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, jsonString);
  if (error)
    return false;

  JsonArray widgets = doc["widgets"];
  for (JsonObject widget : widgets) {
    WidgetConfig cfg;
    const char *type = widget["type"];

    if (strcmp(type, "clock_time") == 0)
      cfg.type = WIDGET_CLOCK_TIME;
    else if (strcmp(type, "clock_date") == 0)
      cfg.type = WIDGET_CLOCK_DATE;
    else if (strcmp(type, "wifi_icon") == 0)
      cfg.type = WIDGET_WIFI_ICON;
    else if (strcmp(type, "battery_icon") == 0)
      cfg.type = WIDGET_BATTERY_ICON;
    else if (strcmp(type, "chip_temp") == 0)
      cfg.type = WIDGET_CHIP_TEMP;
    else if (strcmp(type, "image") == 0)
      cfg.type = WIDGET_IMAGE;
    else
      continue;

    cfg.x = widget["x"] | 0;
    cfg.y = widget["y"] | 0;
    cfg.w = widget["w"] | 0;
    cfg.h = widget["h"] | 0;
    cfg.color = hexToColor(widget["color"] | "#000000");
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
  _lastTimeStr[0] = '\0';
  _lastDateStr[0] = '\0';
  _lastRssiBars = -1;
  _lastBatPercent = -1;
  _lastChipTemp = -999;
}

void LayoutEngine::drawBackgroundPatch(LGFX *canvas, int32_t x, int32_t y,
                                       int32_t w, int32_t h) {
  if (x < 0) {
    w += x;
    x = 0;
  }
  if (y < 0) {
    h += y;
    y = 0;
  }
  if (x + w > BG_WIDTH)
    w = BG_WIDTH - x;
  if (y + h > BG_HEIGHT)
    h = BG_HEIGHT - y;
  if (w <= 0 || h <= 0)
    return;

  for (int32_t r = 0; r < h; r++) {
    int32_t curY = y + r;
    canvas->pushImage(x, curY, w, 1, &StandbyBackground[curY * BG_WIDTH + x]);
  }
}

void LayoutEngine::renderStandbyScreen(DisplayDriver *display,
                                       NetworkManager *network,
                                       bool fullRedraw) {
  if (!display || !network)
    return;

  LGFX *tft = display->getTFT();
  display->acquireSPI();

  if (fullRedraw) {
    tft->startWrite();
    tft->pushImage(0, 0, BG_WIDTH, BG_HEIGHT, StandbyBackground);
    tft->endWrite();
    invalidateCache();
  }

  for (const auto &cfg : _widgets) {
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
    case WIDGET_CHIP_TEMP:
      drawChipTemp(tft, cfg, fullRedraw);
      break;
    case WIDGET_IMAGE:
      break;
    }
  }

  display->releaseSPI();
}

void LayoutEngine::drawTextWidget(LGFX* canvas, const WidgetConfig& cfg, const char* text, int32_t defaultW, int32_t defaultH, const lgfx::IFont* font) {
    int32_t boxX = cfg.x;
    int32_t boxY = cfg.y;
    int32_t boxW = (cfg.w > 0) ? cfg.w : defaultW;
    int32_t boxH = (cfg.h > 0) ? cfg.h : defaultH;

    canvas->startWrite();
    drawBackgroundPatch(canvas, boxX, boxY, boxW, boxH);

    if (font != nullptr) {
        canvas->setFont(font);
    }

    int32_t centerY = boxY + (boxH / 2);
    canvas->setTextColor(cfg.color);

    if (cfg.align == "center") {
        canvas->setTextDatum(lgfx::middle_center);
        canvas->drawString(text, boxX + (boxW / 2), centerY);
    } else if (cfg.align == "right") {
        canvas->setTextDatum(lgfx::middle_right);
        canvas->drawString(text, boxX + boxW, centerY);
    } else {
        canvas->setTextDatum(lgfx::middle_left);
        canvas->drawString(text, boxX, centerY);
    }

    canvas->setTextSize(1);
    canvas->setFont(nullptr);
    canvas->endWrite();
}

void LayoutEngine::drawClockTime(LGFX* canvas, const WidgetConfig& cfg, NetworkManager* network, bool force) {
    char timeStr[16];
    network->getTimeString(timeStr, sizeof(timeStr));
    if (!force && strcmp(timeStr, _lastTimeStr) == 0) return;

    const lgfx::IFont* selectedFont = &ChakraPetch_SemiBold_48;
    if (cfg.font == "Orbitron_32") {
        selectedFont = &fonts::Orbitron_Light_32;
    } else if (cfg.font == "Font7") {
        selectedFont = &fonts::Font7;
    }

    drawTextWidget(canvas, cfg, timeStr, 132, 35, selectedFont);

    strncpy(_lastTimeStr, timeStr, sizeof(_lastTimeStr) - 1);
    _lastTimeStr[sizeof(_lastTimeStr) - 1] = '\0';
}

void LayoutEngine::drawClockDate(LGFX* canvas, const WidgetConfig& cfg, NetworkManager* network, bool force) {
    char dateStr[32];
    network->getDateString(dateStr, sizeof(dateStr));
    if (!force && strcmp(dateStr, _lastDateStr) == 0) return;

    const lgfx::IFont* selectedFont = &ChakraPetch_SemiBold_16;
    if (cfg.font == "Roboto_14") {
        selectedFont = &fonts::Roboto_Thin_24;
    } else if (cfg.font == "FreeSans_12") {
        selectedFont = &fonts::FreeSansBold12pt7b;
    }

    drawTextWidget(canvas, cfg, dateStr, 140, 16, selectedFont);

    strncpy(_lastDateStr, dateStr, sizeof(_lastDateStr) - 1);
    _lastDateStr[sizeof(_lastDateStr) - 1] = '\0';
}

void LayoutEngine::drawWifiIcon(LGFX *canvas, const WidgetConfig &cfg,
                                NetworkManager *network, bool force) {
  int rssi = network->getWifiRSSI();
  int bars = 0;
  if (rssi > -60)
    bars = 4;
  else if (rssi > -70)
    bars = 3;
  else if (rssi > -80)
    bars = 2;
  else if (rssi > -100)
    bars = 1;

  if (!force && bars == _lastRssiBars)
    return;

  canvas->startWrite();
  drawBackgroundPatch(canvas, cfg.x, cfg.y, WIFI_ICON_WIDTH, WIFI_ICON_HEIGHT);
  canvas->drawBitmap(cfg.x, cfg.y, WIFI_ICONS[bars], WIFI_ICON_WIDTH,
                     WIFI_ICON_HEIGHT, cfg.color);
  canvas->endWrite();

  _lastRssiBars = bars;
}

void LayoutEngine::drawBatteryIcon(LGFX *canvas, const WidgetConfig &cfg,
                                   bool force) {
  // Giả lập nấc pin (4/4 = 100%, 3/4 = 75%, 2/4 = 50%, 1/4 = 25%, 0/4 = 10%)
  int state = 3;
  if (!force && state == _lastBatPercent)
    return;

  int32_t boxX = cfg.x;
  int32_t boxY = cfg.y;
  int32_t boxW = (cfg.w > 0) ? cfg.w : BATTERY_ICON_WIDTH;
  int32_t boxH = (cfg.h > 0) ? cfg.h : BATTERY_ICON_HEIGHT;

  canvas->startWrite();
  drawBackgroundPatch(canvas, boxX, boxY, boxW, boxH);

  // Vẽ icon pin 5 trạng thái (75x16 px)
  canvas->pushImage(boxX, boxY, BATTERY_ICON_WIDTH, BATTERY_ICON_HEIGHT,
                    BATTERY_ICONS[state], BATTERY_TRANSPARENT_COLOR);
  canvas->endWrite();

  _lastBatPercent = state;
}

void LayoutEngine::drawChipTemp(LGFX *canvas, const WidgetConfig &cfg,
                                bool force) {
  float tempC = SystemMonitor::getChipTemperature();
  int tempInt = (int)(tempC + 0.5f);
  if (!force && tempInt == _lastChipTemp)
    return;

  char tempBuf[16];
  snprintf(tempBuf, sizeof(tempBuf), "%d'C", tempInt);

  drawTextWidget(canvas, cfg, tempBuf, 100, 20, &ChakraPetch_SemiBold_16);

  _lastChipTemp = tempInt;
}
