#include "DisplayDriver.h"
#include "config.h"
#include "driver/gpio.h"
#include <esp_arduino_version.h>

#if defined(ESP_ARDUINO_VERSION) && ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
// ESP32 Core 3.0+
#define LEDC_SETUP() ledcAttach(PIN_TFT_BLK, 44100, 9)
#define LEDC_WRITE(val) ledcWrite(PIN_TFT_BLK, val)
#else
// ESP32 Core 2.x
#define LEDC_CHANNEL 0
#define LEDC_SETUP() do { ledcSetup(LEDC_CHANNEL, 44100, 9); ledcAttachPin(PIN_TFT_BLK, LEDC_CHANNEL); } while(0)
#define LEDC_WRITE(val) ledcWrite(LEDC_CHANNEL, val)
#endif
bool DisplayDriver::init(SemaphoreHandle_t spiMutex) {
  _spiMutex = spiMutex;
  gpio_hold_dis((gpio_num_t)PIN_TFT_BLK);

  _tft.init();
  _tft.setRotation(0);
  _tft.setSwapBytes(true);
  _tft.fillScreen(TFT_BLACK);
  LEDC_SETUP();
  setBacklight(0);
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
  uint32_t val = map(percent, 0, 100, 0, 511);
  LEDC_WRITE(val);
}

void DisplayDriver::turnOff() {
  setBacklight(0);
  delay(10); // Đợi 10ms để PWM áp dụng mức 0

  if (acquireSPI()) {
    _tft.fillScreen(TFT_BLACK);
    _tft.sleep();
    releaseSPI();
  }

  // Ép chân BLK ở mức LOW trong suốt light sleep để đèn nền tắt hẳn.
  pinMode(PIN_TFT_BLK, OUTPUT);
  digitalWrite(PIN_TFT_BLK, LOW);
  
  // Chỉ cần gpio_hold_en() là đủ để giữ trạng thái Pad qua light sleep.
  gpio_hold_en((gpio_num_t)PIN_TFT_BLK);
  _isSleeping = true;
}

void DisplayDriver::wakeupFlash() {
  // Nhả chốt hold phần cứng sau khi thức dậy
  gpio_hold_dis((gpio_num_t)PIN_TFT_BLK);
}

void DisplayDriver::turnOn() {
  if (!_isSleeping) {
    setBacklight(BACKLIGHT_DAY_PERCENT);
    return;
  }

  // Đảm bảo nhả chốt GPIO BLK 
  gpio_hold_dis((gpio_num_t)PIN_TFT_BLK);

  if (xSemaphoreTake(_spiMutex, pdMS_TO_TICKS(3000)) == pdTRUE) {
    // Không gọi _tft.init() vì có thể làm hỏng trạng thái SPI chung
    _tft.wakeup();
    _tft.setRotation(0);
    _tft.setSwapBytes(true); // Bắt buộc — tránh đảo màu RGB565 sau wakeup
    _tft.fillScreen(TFT_BLACK);
    xSemaphoreGive(_spiMutex);

    // Bật lại LEDC PWM hoàn toàn độc lập
    LEDC_SETUP();
    setBacklight(BACKLIGHT_DAY_PERCENT);
    _isSleeping = false;
  } else {
    Serial.println(F("[Display] ERROR: acquireSPI() timeout in turnOn()!"));
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

bool DisplayDriver::acquireSPI(uint32_t timeoutMs) {
  if (_spiMutex == nullptr) return true;
  return xSemaphoreTake(_spiMutex, pdMS_TO_TICKS(timeoutMs)) == pdTRUE;
}

void DisplayDriver::releaseSPI() {
  if (_spiMutex != nullptr) xSemaphoreGive(_spiMutex);
}
