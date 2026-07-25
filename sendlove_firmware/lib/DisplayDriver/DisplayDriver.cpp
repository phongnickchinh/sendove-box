#include "DisplayDriver.h"
#include "config.h"

// ============================================================================
// DisplayDriver Implementation — LovyanGFX
// ============================================================================

bool DisplayDriver::init(SemaphoreHandle_t spiMutex) {
  _spiMutex = spiMutex;

  gpio_hold_dis((gpio_num_t)PIN_TFT_BLK);

  // Khởi tạo LGFX (SPI bus + Panel + Backlight tự động cấu hình)
  _tft.init();
  _tft.setRotation(0);
  _tft.setSwapBytes(true); // Cần cho pushImage RGB565
  _tft.fillScreen(TFT_BLACK);
  _tft.setBrightness(0); // Bắt đầu với backlight tắt

  Serial.println(
      F("[Display] LGFX initialized (ST7789 240x240, SPI Mode 3, bus_shared)"));
  return true;
}

void DisplayDriver::pushImage(int32_t x, int32_t y, int32_t w, int32_t h,
                              const uint16_t *pixels) {
  // Gọi trực tiếp — KHÔNG acquire mutex ở đây
  // Vì JPEGDEC callback gọi pushImage nhiều lần liên tục trong 1 frame,
  // caller (MediaPlayer) phải acquire mutex 1 lần trước khi decode toàn bộ
  // frame.
  _tft.pushImage(x, y, w, h, pixels);
}

void DisplayDriver::drawClockFace(uint8_t hour, uint8_t minute) {
  if (!acquireSPI())
    return;

  _tft.fillScreen(TFT_BLACK);

  // Vẽ giờ:phút ở giữa màn hình, font lớn
  _tft.setTextColor(TFT_WHITE, TFT_BLACK);
  _tft.setTextDatum(lgfx::middle_center);

  char timeStr[6];
  snprintf(timeStr, sizeof(timeStr), "%02d:%02d", hour, minute);

  // Font 7 = 7-segment, 48px height
  _tft.setTextFont(7);
  _tft.drawString(timeStr, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);

  releaseSPI();
}

void DisplayDriver::drawStatusBar(uint8_t batteryPercent, bool wifiConnected) {
  if (!acquireSPI())
    return;

  // Thanh trạng thái ở top, cao 16px
  _tft.fillRect(0, 0, SCREEN_WIDTH, 16, TFT_BLACK);
  _tft.setTextFont(1);
  _tft.setTextDatum(lgfx::top_left);

  // Icon Wi-Fi (trái)
  _tft.setTextColor(wifiConnected ? TFT_GREEN : TFT_RED, TFT_BLACK);
  _tft.drawString(wifiConnected ? "WiFi" : "NoWF", 2, 2);

  // Phần trăm pin (phải)
  uint16_t batColor = (batteryPercent > 20) ? TFT_GREEN : TFT_RED;
  _tft.setTextColor(batColor, TFT_BLACK);
  char batStr[8];
  snprintf(batStr, sizeof(batStr), "%3d%%", batteryPercent);
  _tft.setTextDatum(lgfx::top_right);
  _tft.drawString(batStr, SCREEN_WIDTH - 2, 2);

  releaseSPI();
}

void DisplayDriver::showMessage(const char *message) {
  if (!acquireSPI())
    return;

  _tft.fillScreen(TFT_BLACK);
  _tft.setTextColor(TFT_WHITE, TFT_BLACK);
  _tft.setTextDatum(lgfx::middle_center);
  _tft.setTextSize(2);
  _tft.drawString(message, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);
  _tft.setTextSize(1);

  releaseSPI();
}

#include "driver/gpio.h"

void DisplayDriver::setBacklight(uint8_t percent) {
  // LovyanGFX setBrightness: 0–255
  uint8_t brightness = map(percent, 0, 100, 0, 255);
  _tft.setBrightness(brightness);
}

void DisplayDriver::turnOff() {
  setBacklight(0);

  if (acquireSPI()) {
    _tft.sleep();
    releaseSPI();
  }

  // Ép chân Backlight (GPIO 3) về LOW và giữ trạng thái (hold) trong suốt quá
  // trình CPU ngủ
  pinMode(PIN_TFT_BLK, OUTPUT);
  digitalWrite(PIN_TFT_BLK, LOW);
  gpio_hold_en((gpio_num_t)PIN_TFT_BLK);
  gpio_deep_sleep_hold_en();

  _isSleeping = true;

  // [FIX-3] Bảo vệ Serial.print — USB CDC có thể bị ngắt ngay trước khi vào
  // sleep
  if (Serial) {
    Serial.println(F("[Display] TFT off (Backlight forced LOW & Held)"));
    Serial.flush();
  }
}

void DisplayDriver::turnOn(uint8_t cause) {
  // Nếu màn hình đã đang bật (không ở trạng thái sleeping), chỉ cần đảm bảo
  // backlight bật
  if (cause != ESP_SLEEP_WAKEUP_TIMER) {
    if (!_isSleeping) {
      setBacklight(BACKLIGHT_DAY_PERCENT);
      return;
    }

    // --- CHỈ THỰC HIỆN KHI THỰC SỰ THỨC DẬY TỪ SLEEP ---
    // 1. Giải phóng giữ chân Backlight (đã bị hold trong turnOff)
    gpio_hold_dis((gpio_num_t)PIN_TFT_BLK);

    // 2. Ép chân Backlight về OUTPUT HIGH ngay lập tức để đảm bảo sáng
    pinMode(PIN_TFT_BLK, OUTPUT);
    digitalWrite(PIN_TFT_BLK, HIGH);

    // [FIX-1+4+5] Full re-init LovyanGFX sau Light Sleep.
    _tft.init();
    _tft.setRotation(0);
    _tft.setSwapBytes(true);
    _tft.fillScreen(TFT_BLACK);
    _tft.setBrightness(255);

    _isSleeping = false;

    if (Serial) {
      Serial.println(F("[Display] TFT on (Full LGFX re-init after sleep)"));
    }
  }
}

void DisplayDriver::clear() {
  if (acquireSPI()) {
    _tft.fillScreen(TFT_BLACK);
    releaseSPI();
  }
}

LGFX *DisplayDriver::getTFT() { return &_tft; }

// --- SPI Mutex ---

bool DisplayDriver::acquireSPI() {
  if (_spiMutex == nullptr)
    return true;
  if (xSemaphoreTake(_spiMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
    return true;
  }
  Serial.println(F("[Display] ERROR: SPI mutex timeout"));
  return false;
}

void DisplayDriver::releaseSPI() {
  if (_spiMutex != nullptr) {
    xSemaphoreGive(_spiMutex);
  }
}
