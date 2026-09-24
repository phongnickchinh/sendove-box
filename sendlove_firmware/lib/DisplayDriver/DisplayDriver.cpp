#include "DisplayDriver.h"
#include "config.h"
#include "Settings.h"
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

void DisplayDriver::showWrappedText(const char *asciiText, int32_t x, int32_t y, int32_t w, int32_t h,
                                     uint16_t color) {
  if (asciiText == nullptr || asciiText[0] == '\0' || w <= 0 || h <= 0) return;
  if (!acquireSPI()) return;

  // Phông có sẵn của LovyanGFX (ChakraPetch biên dịch cứng đã gỡ 2026-09-24, §28).
  _tft.setFont(&fonts::FreeSansBold9pt7b);
  _tft.setTextColor(color);
  _tft.setTextDatum(lgfx::top_center);

  // yAdvance của FreeSansBold9pt7b = 22; dùng nguyên làm line height.
  const int32_t lineHeight = 22;
  int32_t maxLines = h / lineHeight;
  if (maxLines < 1) maxLines = 1;
  if (maxLines > 16) maxLines = 16;

  // Bản copy cục bộ vì strtok() sửa thẳng vào buffer.
  char buf[300];
  strncpy(buf, asciiText, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';

  char lines[16][48];
  int lineCount = 0;
  char currentLine[64] = "";

  char *word = strtok(buf, " ");
  while (word != nullptr && lineCount < maxLines) {
    char trial[64];
    if (currentLine[0] == '\0') {
      snprintf(trial, sizeof(trial), "%s", word);
    } else {
      snprintf(trial, sizeof(trial), "%s %s", currentLine, word);
    }
    if (_tft.textWidth(trial) <= w || currentLine[0] == '\0') {
      strncpy(currentLine, trial, sizeof(currentLine) - 1);
      currentLine[sizeof(currentLine) - 1] = '\0';
    } else {
      strncpy(lines[lineCount], currentLine, sizeof(lines[0]) - 1);
      lines[lineCount][sizeof(lines[0]) - 1] = '\0';
      lineCount++;
      strncpy(currentLine, word, sizeof(currentLine) - 1);
      currentLine[sizeof(currentLine) - 1] = '\0';
    }
    word = strtok(nullptr, " ");
  }
  if (lineCount < maxLines && currentLine[0] != '\0') {
    strncpy(lines[lineCount], currentLine, sizeof(lines[0]) - 1);
    lines[lineCount][sizeof(lines[0]) - 1] = '\0';
    lineCount++;
  }
  // Còn từ chưa xếp hết -> bị cắt do quá dài, đánh dấu bằng "..." ở dòng cuối.
  if (word != nullptr && lineCount > 0) {
    size_t len = strlen(lines[lineCount - 1]);
    if (len > sizeof(lines[0]) - 4) len = sizeof(lines[0]) - 4;
    lines[lineCount - 1][len] = '\0';
    strncat(lines[lineCount - 1], "...", sizeof(lines[0]) - len - 1);
  }

  int32_t startY = y + (h - lineCount * lineHeight) / 2;
  if (startY < y) startY = y;
  for (int i = 0; i < lineCount; i++) {
    _tft.drawString(lines[i], x + w / 2, startY + i * lineHeight);
  }

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
    setBacklight(Settings::currentBacklight());
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
    setBacklight(Settings::currentBacklight());
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
