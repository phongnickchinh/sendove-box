#include "DisplayDriver.h"
#include "config.h"

// ============================================================================
// DisplayDriver Implementation
// ============================================================================

// LEDC PWM config cho backlight
static constexpr uint32_t BLK_PWM_FREQ       = 5000; // 5kHz
static constexpr uint8_t  BLK_PWM_RESOLUTION = 8;    // 8-bit (0–255)

bool DisplayDriver::init(uint8_t backlightPin, SemaphoreHandle_t spiMutex) {
    _backlightPin = backlightPin;
    _spiMutex = spiMutex;

    // Cấu hình LEDC PWM cho backlight
    _backlightChannel = 0;
    ledcSetup(_backlightChannel, BLK_PWM_FREQ, BLK_PWM_RESOLUTION);
    ledcAttachPin(_backlightPin, _backlightChannel);
    ledcWrite(_backlightChannel, 0); // Bắt đầu với backlight tắt

    // Khởi tạo TFT
    if (acquireSPI()) {
        _tft.init();
        _tft.setRotation(0); // Portrait
        _tft.fillScreen(TFT_BLACK);
        releaseSPI();
    }

    Serial.println(F("[Display] TFT initialized"));
    return true;
}

void DisplayDriver::pushFrameBuffer(const uint16_t* buffer, uint16_t w, uint16_t h) {
    if (!acquireSPI()) return;

    _tft.pushImage(0, 0, w, h, buffer);

    releaseSPI();
}

void DisplayDriver::drawClockFace(uint8_t hour, uint8_t minute) {
    if (!acquireSPI()) return;

    _tft.fillScreen(TFT_BLACK);

    // Vẽ giờ:phút ở giữa màn hình, font lớn
    _tft.setTextColor(TFT_WHITE, TFT_BLACK);
    _tft.setTextDatum(MC_DATUM); // Middle Center

    char timeStr[6];
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d", hour, minute);

    // Sử dụng font lớn (font 7 = 7-segment, 48px)
    _tft.setTextFont(7);
    _tft.drawString(timeStr, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);

    releaseSPI();
}

void DisplayDriver::drawStatusBar(uint8_t batteryPercent, bool wifiConnected) {
    if (!acquireSPI()) return;

    // Thanh trạng thái ở top, cao 16px
    _tft.fillRect(0, 0, SCREEN_WIDTH, 16, TFT_BLACK);
    _tft.setTextFont(1);
    _tft.setTextDatum(TL_DATUM);

    // Icon Wi-Fi (trái)
    _tft.setTextColor(wifiConnected ? TFT_GREEN : TFT_RED, TFT_BLACK);
    _tft.drawString(wifiConnected ? "WiFi" : "NoWF", 2, 2);

    // Phần trăm pin (phải)
    uint16_t batColor = (batteryPercent > 20) ? TFT_GREEN : TFT_RED;
    _tft.setTextColor(batColor, TFT_BLACK);
    char batStr[8];
    snprintf(batStr, sizeof(batStr), "%3d%%", batteryPercent);
    _tft.setTextDatum(TR_DATUM);
    _tft.drawString(batStr, SCREEN_WIDTH - 2, 2);

    releaseSPI();
}

void DisplayDriver::showMessage(const char* message) {
    if (!acquireSPI()) return;

    _tft.fillScreen(TFT_BLACK);
    _tft.setTextColor(TFT_WHITE, TFT_BLACK);
    _tft.setTextDatum(MC_DATUM);
    _tft.setTextFont(2); // Font 2 = 16px
    _tft.drawString(message, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);

    releaseSPI();
}

void DisplayDriver::setBacklight(uint8_t percent) {
    // Map 0–100% → 0–255
    uint8_t duty = map(percent, 0, 100, 0, 255);
    ledcWrite(_backlightChannel, duty);
}

void DisplayDriver::turnOff() {
    setBacklight(0);

    if (acquireSPI()) {
        // Gửi lệnh sleep cho TFT controller (ST7735)
        _tft.writecommand(0x10); // SLPIN (Sleep In)
        releaseSPI();
    }

    Serial.println(F("[Display] TFT off"));
}

void DisplayDriver::turnOn() {
    if (acquireSPI()) {
        _tft.writecommand(0x11); // SLPOUT (Sleep Out)
        releaseSPI();
    }

    delay(120); // Datasheet: chờ 120ms sau SLPOUT
    Serial.println(F("[Display] TFT on"));
}

void DisplayDriver::clear() {
    if (acquireSPI()) {
        _tft.fillScreen(TFT_BLACK);
        releaseSPI();
    }
}

TFT_eSPI* DisplayDriver::getTFT() {
    return &_tft;
}

// --- SPI Mutex ---

bool DisplayDriver::acquireSPI() {
    if (_spiMutex == nullptr) return true;
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
