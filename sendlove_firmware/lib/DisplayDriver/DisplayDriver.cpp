#include "DisplayDriver.h"
#include "config.h"
#include "driver/gpio.h"

bool DisplayDriver::init(SemaphoreHandle_t spiMutex) {
  _spiMutex = spiMutex;
  gpio_hold_dis((gpio_num_t)PIN_TFT_BLK);

  _tft.init();
  _tft.setRotation(0);
  _tft.setSwapBytes(true);
  _tft.fillScreen(TFT_BLACK);
  _tft.setBrightness(0);
  return true;
}

void DisplayDriver::pushImage(int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t *pixels) {
  _tft.pushImage(x, y, w, h, pixels);
}

void DisplayDriver::drawClockFace(uint8_t hour, uint8_t minute) {
  if (!acquireSPI()) return;

  _tft.fillScreen(TFT_BLACK);
  _tft.setTextColor(TFT_WHITE, TFT_BLACK);
  _tft.setTextDatum(lgfx::middle_center);

  char timeStr[6];
  snprintf(timeStr, sizeof(timeStr), "%02d:%02d", hour, minute);

  _tft.setTextFont(7);
  _tft.drawString(timeStr, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);
  releaseSPI();
}

void DisplayDriver::drawStatusBar(uint8_t batteryPercent, bool wifiConnected) {
  if (!acquireSPI()) return;

  _tft.fillRect(0, 0, SCREEN_WIDTH, 16, TFT_BLACK);
  _tft.setTextFont(1);
  _tft.setTextDatum(lgfx::top_left);

  _tft.setTextColor(wifiConnected ? TFT_GREEN : TFT_RED, TFT_BLACK);
  _tft.drawString(wifiConnected ? "WiFi" : "NoWF", 2, 2);

  uint16_t batColor = (batteryPercent > 20) ? TFT_GREEN : TFT_RED;
  _tft.setTextColor(batColor, TFT_BLACK);
  char batStr[8];
  snprintf(batStr, sizeof(batStr), "%3d%%", batteryPercent);
  _tft.setTextDatum(lgfx::top_right);
  _tft.drawString(batStr, SCREEN_WIDTH - 2, 2);

  releaseSPI();
}

void DisplayDriver::showMessage(const char *message) {
  if (!acquireSPI()) return;

  _tft.fillScreen(TFT_BLACK);
  _tft.setTextColor(TFT_WHITE, TFT_BLACK);
  _tft.setTextDatum(lgfx::middle_center);
  _tft.setTextSize(2);
  _tft.drawString(message, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);
  _tft.setTextSize(1);
  releaseSPI();
}

void DisplayDriver::setBacklight(uint8_t percent) {
  _tft.setBrightness(map(percent, 0, 100, 0, 255));
}

void DisplayDriver::turnOff() {
  setBacklight(0);

  if (acquireSPI()) {
    _tft.sleep();
    releaseSPI();
  }

  pinMode(PIN_TFT_BLK, OUTPUT);
  digitalWrite(PIN_TFT_BLK, LOW);
  gpio_hold_en((gpio_num_t)PIN_TFT_BLK);
  gpio_deep_sleep_hold_en();
  _isSleeping = true;
}

void DisplayDriver::turnOn(uint8_t cause) {
  if (cause != ESP_SLEEP_WAKEUP_TIMER) {
    if (!_isSleeping) {
      setBacklight(BACKLIGHT_DAY_PERCENT);
      return;
    }

    gpio_hold_dis((gpio_num_t)PIN_TFT_BLK);
    pinMode(PIN_TFT_BLK, OUTPUT);
    digitalWrite(PIN_TFT_BLK, HIGH);

    _tft.init();
    _tft.setRotation(0);
    _tft.setSwapBytes(true);
    _tft.fillScreen(TFT_BLACK);
    _tft.setBrightness(255);
    _isSleeping = false;
  }
}

void DisplayDriver::clear() {
  if (acquireSPI()) {
    _tft.fillScreen(TFT_BLACK);
    releaseSPI();
  }
}

LGFX *DisplayDriver::getTFT() { return &_tft; }

bool DisplayDriver::acquireSPI() {
  if (_spiMutex == nullptr) return true;
  return xSemaphoreTake(_spiMutex, pdMS_TO_TICKS(1000)) == pdTRUE;
}

void DisplayDriver::releaseSPI() {
  if (_spiMutex != nullptr) xSemaphoreGive(_spiMutex);
}
