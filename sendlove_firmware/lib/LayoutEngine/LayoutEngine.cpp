#include "LayoutEngine.h"
#include "CustomBatteryIcons.h"
#include "CustomWifiIcons.h"
#include "ScreenLogger.h"
#include "SystemMonitor.h"
#include "ThemeStore.h"
#include <time.h>

// Background, fonts and layout come from the theme package (ThemeStore, MEMORY.md
// §28). Built-in fonts are used only for the fallback and alarm screens.

static constexpr int32_t BG_W = SCREEN_WIDTH;
static constexpr int32_t BG_H = SCREEN_HEIGHT;
static constexpr uint32_t BG_BYTES = BG_W * BG_H * 2;
static constexpr time_t LAYOUT_MIN_VALID_EPOCH = 1600000000;

// Weekday names MUST match the web's theme/layout.js: the web subsets the VLW font from exactly these strings.
static const char* const WD_VI[7] = {"Chủ nhật", "Thứ hai", "Thứ ba", "Thứ tư", "Thứ năm", "Thứ sáu", "Thứ bảy"};
static const char* const WD_VI_ASCII[7] = {"CN", "T2", "T3", "T4", "T5", "T6", "T7"};
static const char* const WD_EN[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

uint16_t LayoutEngine::hexToColor(const char *hex) {
  if (hex == nullptr || strlen(hex) < 7 || hex[0] != '#')
    return TFT_WHITE;
  long rgb = strtol(hex + 1, nullptr, 16);
  return lgfx::color565((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
}

void LayoutEngine::unloadFonts() {
  if (_hasVlwTime) _vlwTime.unloadFont();
  if (_hasVlwDate) _vlwDate.unloadFont();
  _hasVlwTime = _hasVlwDate = false;
}

bool LayoutEngine::parseWidgets(const char *json, size_t len) {
  _widgets.clear();
  JsonDocument doc;
  if (deserializeJson(doc, json, len)) return false;

  for (JsonObject widget : doc["widgets"].as<JsonArray>()) {
    WidgetConfig cfg;
    // A missing "type" would make strcmp(nullptr) crash the box -> skip the widget (mandatory case, MEMORY.md §28).
    const char *type = widget["type"] | "";
    if (strcmp(type, "clock_time") == 0) cfg.type = WIDGET_CLOCK_TIME;
    else if (strcmp(type, "clock_date") == 0) cfg.type = WIDGET_CLOCK_DATE;
    else if (strcmp(type, "wifi_icon") == 0) cfg.type = WIDGET_WIFI_ICON;
    else if (strcmp(type, "battery_icon") == 0) cfg.type = WIDGET_BATTERY_ICON;
    else if (strcmp(type, "chip_temp") == 0) cfg.type = WIDGET_CHIP_TEMP;
    else continue;

    int x = widget["x"] | -1, y = widget["y"] | -1, w = widget["w"] | 0, h = widget["h"] | 0;
    if (x < 0 || y < 0 || w < 0 || h < 0 || x + w > BG_W || y + h > BG_H) continue;  // off-screen
    cfg.x = x;
    cfg.y = y;
    cfg.w = w;
    cfg.h = h;
    cfg.color = hexToColor(widget["color"] | "#000000");
    cfg.align = widget["align"] | "left";
    cfg.font = widget["font"] | "";
    cfg.format = widget["format"] | "WD, DD.MM";
    cfg.locale = widget["locale"] | "vi";
    _widgets.push_back(cfg);
  }
  return !_widgets.empty();
}

void LayoutEngine::useFallbackWidgets() {
  _widgets.clear();
  WidgetConfig t;
  t.type = WIDGET_CLOCK_TIME;
  t.x = 0; t.y = 80; t.w = BG_W; t.h = 60;
  t.color = TFT_WHITE; t.align = "center"; t.font = "Font7";
  _widgets.push_back(t);
  WidgetConfig d;
  d.type = WIDGET_CLOCK_DATE;
  d.x = 0; d.y = 150; d.w = BG_W; d.h = 24;
  d.color = TFT_WHITE; d.align = "center"; d.font = "Font2";
  d.format = "WD, DD.MM"; d.locale = "vi";
  _widgets.push_back(d);
}

bool LayoutEngine::loadTheme() {
  unloadFonts();
  _bg = nullptr;
  _fallback = true;

  uint32_t len = 0;
  const uint8_t *layout = ThemeStore::asset("layout", &len);
  if (layout && parseWidgets((const char *)layout, len)) {
    uint32_t bgLen = 0;
    const uint8_t *bg = ThemeStore::asset("bg", &bgLen);
    if (bg && bgLen == BG_BYTES) _bg = (const uint16_t *)bg;  // RGB565 LE, 4-byte aligned

    uint32_t fl = 0;
    const uint8_t *ft = ThemeStore::asset("f_time", &fl);
    if (ft && fl > 24) {
      _pwTime.set(ft, fl);
      _hasVlwTime = _vlwTime.loadFont(&_pwTime);
    }
    const uint8_t *fd = ThemeStore::asset("f_date", &fl);
    if (fd && fl > 24) {
      _pwDate.set(fd, fl);
      _hasVlwDate = _vlwDate.loadFont(&_pwDate);
    }
    _fallback = false;
  } else {
    useFallbackWidgets();
  }
  invalidateCache();
  DLOG("[LAY] %s, %u widget, nen=%d, vlw=%d%d", _fallback ? "du phong" : ThemeStore::themeId(),
       (unsigned)_widgets.size(), _bg ? 1 : 0, _hasVlwTime ? 1 : 0, _hasVlwDate ? 1 : 0);
  return !_fallback;
}

const lgfx::IFont *LayoutEngine::fontFor(const WidgetConfig &cfg) const {
  if (cfg.font == "f_time" && _hasVlwTime) return &_vlwTime;
  if (cfg.font == "f_date" && _hasVlwDate) return &_vlwDate;
  if (cfg.type == WIDGET_CLOCK_TIME) return &fonts::Font7;  // 48px 7-segment: digits and ':' only
  return &fonts::Font2;                                      // 16px ASCII
}

void LayoutEngine::formatDate(const WidgetConfig &cfg, bool unicode, char *out, size_t len) const {
  time_t now = time(nullptr);
  if (now < LAYOUT_MIN_VALID_EPOCH) {
    // No time yet (power loss, no NTP): show that plainly instead of a wrong date.
    snprintf(out, len, "--.--");
    return;
  }
  struct tm t;
  localtime_r(&now, &t);
  const bool en = cfg.locale == "en";
  const char *wd = en ? WD_EN[t.tm_wday] : (unicode ? WD_VI[t.tm_wday] : WD_VI_ASCII[t.tm_wday]);

  if (cfg.format == "DD/MM/YYYY") {
    snprintf(out, len, "%02d/%02d/%04d", t.tm_mday, t.tm_mon + 1, t.tm_year + 1900);
  } else if (cfg.format == "WD DD.MM") {
    snprintf(out, len, "%s %02d.%02d", wd, t.tm_mday, t.tm_mon + 1);
  } else {  // "WD, DD.MM" (default)
    snprintf(out, len, "%s, %02d.%02d", wd, t.tm_mday, t.tm_mon + 1);
  }
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
  if (x + w > BG_W)
    w = BG_W - x;
  if (y + h > BG_H)
    h = BG_H - y;
  if (w <= 0 || h <= 0)
    return;

  if (!_bg) {
    canvas->fillRect(x, y, w, h, TFT_BLACK);
    return;
  }
  // Read straight from flash through the mmap — the same kind of memory as a PROGMEM array.
  for (int32_t r = 0; r < h; r++) {
    int32_t curY = y + r;
    canvas->pushImage(x, curY, w, 1, &_bg[curY * BG_W + x]);
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
    if (_bg) tft->pushImage(0, 0, BG_W, BG_H, _bg);
    else tft->fillScreen(TFT_BLACK);
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

  // VLW glyphs are alloca()ed on this task's stack: log whenever the remaining stack
  // reaches a new low, so the last number printed is the true floor.
  if (_hasVlwTime || _hasVlwDate) {
    UBaseType_t left = uxTaskGetStackHighWaterMark(nullptr);
    if (left < _stackMin) {
      _stackMin = left;
      DLOG("[LAY] stack con %u B", (unsigned)left);
    }
  }
}

void LayoutEngine::renderAlarmScreen(DisplayDriver *display, const char *timeStr,
                                     const char *hint) {
  if (!display) return;
  LGFX *tft = display->getTFT();
  if (!display->acquireSPI()) return;

  tft->startWrite();
  tft->fillScreen(TFT_BLACK);
  tft->setTextColor(TFT_WHITE);
  tft->setTextDatum(lgfx::middle_center);

  tft->setFont(&fonts::Font4);
  tft->drawString("BAO THUC", SCREEN_WIDTH / 2, 60);

  tft->setFont(&fonts::Font7);
  tft->drawString(timeStr ? timeStr : "--:--", SCREEN_WIDTH / 2, 118);

  tft->setFont(&fonts::Font2);
  tft->setTextColor(0xFD34);  // pale pink, matching the portal
  tft->drawString(hint ? hint : "", SCREEN_WIDTH / 2, 180);

  tft->setFont(nullptr);
  tft->endWrite();
  display->releaseSPI();

  // The next standby render must redraw everything; the widget cache can't be trusted.
  invalidateCache();
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
    // Text color only, NO background color (the background is an image). The VLW
    // alpha is 0/255, so no blending with read-back pixels is needed.
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

    drawTextWidget(canvas, cfg, timeStr, 132, 35, fontFor(cfg));

    strncpy(_lastTimeStr, timeStr, sizeof(_lastTimeStr) - 1);
    _lastTimeStr[sizeof(_lastTimeStr) - 1] = '\0';
}

void LayoutEngine::drawClockDate(LGFX* canvas, const WidgetConfig& cfg, NetworkManager* network, bool force) {
    (void)network;
    const lgfx::IFont* font = fontFor(cfg);
    char dateStr[48];
    formatDate(cfg, isVlw(font), dateStr, sizeof(dateStr));
    if (!force && strcmp(dateStr, _lastDateStr) == 0) return;

    drawTextWidget(canvas, cfg, dateStr, 140, 16, font);

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
  // Placeholder battery level (4/4 = 100%, 3/4 = 75%, 2/4 = 50%, 1/4 = 25%, 0/4 = 10%)
  int state = 3;
  if (!force && state == _lastBatPercent)
    return;

  int32_t boxX = cfg.x;
  int32_t boxY = cfg.y;
  int32_t boxW = (cfg.w > 0) ? cfg.w : BATTERY_ICON_WIDTH;
  int32_t boxH = (cfg.h > 0) ? cfg.h : BATTERY_ICON_HEIGHT;

  canvas->startWrite();
  drawBackgroundPatch(canvas, boxX, boxY, boxW, boxH);

  // Draw the 5-state battery icon (75x16 px)
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

  drawTextWidget(canvas, cfg, tempBuf, 100, 20, &fonts::Font2);

  _lastChipTemp = tempInt;
}
