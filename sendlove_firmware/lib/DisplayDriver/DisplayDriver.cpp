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

  // A LovyanGFX built-in font (no compiled-in custom fonts, see MEMORY.md §28).
  _tft.setFont(&fonts::FreeSansBold9pt7b);
  _tft.setTextColor(color);
  _tft.setTextDatum(lgfx::top_center);

  // FreeSansBold9pt7b's yAdvance = 22; used as the line height.
  const int32_t lineHeight = 22;
  int32_t maxLines = h / lineHeight;
  if (maxLines < 1) maxLines = 1;
  if (maxLines > 16) maxLines = 16;

  // A local copy, because strtok() modifies its buffer.
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
  // Words left over -> the text was cut for length; mark it with "..." on the last line.
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
  delay(10); // wait 10ms for the PWM to settle at 0

  if (acquireSPI()) {
    _tft.fillScreen(TFT_BLACK);
    _tft.sleep();
    releaseSPI();
  }

  // Hold BLK LOW throughout light sleep so the backlight stays fully off.
  pinMode(PIN_TFT_BLK, OUTPUT);
  digitalWrite(PIN_TFT_BLK, LOW);
  
  // gpio_hold_en() alone keeps the pad state through light sleep.
  gpio_hold_en((gpio_num_t)PIN_TFT_BLK);
  _isSleeping = true;
}

void DisplayDriver::wakeupFlash() {
  // Release the hardware hold after waking
  gpio_hold_dis((gpio_num_t)PIN_TFT_BLK);
}

void DisplayDriver::turnOn() {
  if (!_isSleeping) {
    setBacklight(Settings::currentBacklight());
    return;
  }

  // Make sure the BLK GPIO hold is released
  gpio_hold_dis((gpio_num_t)PIN_TFT_BLK);

  if (xSemaphoreTake(_spiMutex, pdMS_TO_TICKS(3000)) == pdTRUE) {
    // Don't call _tft.init(): it could corrupt the shared SPI state
    _tft.wakeup();
    _tft.setRotation(0);
    _tft.setSwapBytes(true); // required — prevents swapped RGB565 colors after wake-up
    _tft.fillScreen(TFT_BLACK);
    xSemaphoreGive(_spiMutex);

    // Re-enable the LEDC PWM independently
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
