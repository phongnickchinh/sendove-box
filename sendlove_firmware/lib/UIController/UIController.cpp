#include "UIController.h"
#include "DisplayDriver.h"
#include "BatteryMonitor.h"
#include "TimeManager.h"
#include "config.h"
#include <WiFi.h>

// ============================================================================
// UIController Implementation
// ============================================================================

// LED Breathing config
static constexpr uint32_t BREATH_STEP_MS    = 15;   // Tốc độ thay đổi brightness
static constexpr uint8_t  BREATH_MAX        = 200;  // Brightness tối đa (0-255)
static constexpr uint8_t  BREATH_MIN        = 5;    // Brightness tối thiểu
static constexpr uint32_t LED_PWM_FREQ      = 5000; // 5kHz
static constexpr uint8_t  LED_PWM_RESOLUTION = 8;   // 8-bit

void UIController::init(uint8_t ledPin, uint8_t touchPin,
                         DisplayDriver* display, BatteryMonitor* battery,
                         TimeManager* timeMgr) {
    _ledPin   = ledPin;
    _touchPin = touchPin;
    _display  = display;
    _battery  = battery;
    _timeMgr  = timeMgr;

    // Cấu hình LED PWM (channel 1, khác với backlight ở channel 0)
    _ledChannel = 1;
    ledcSetup(_ledChannel, LED_PWM_FREQ, LED_PWM_RESOLUTION);
    ledcAttachPin(_ledPin, _ledChannel);
    ledcWrite(_ledChannel, 0); // Tắt ban đầu

    // Cấu hình chân touch
    pinMode(_touchPin, INPUT);

    Serial.println(F("[UI] Initialized"));
}

// ============================================================================
// LED Control
// ============================================================================

void UIController::startBreathingLED() {
    _ledState = LEDState::BREATHING;
    _breathBrightness = BREATH_MIN;
    _breathDirection = 1;
    _lastBreathUpdate = millis();
}

void UIController::stopBreathingLED() {
    _ledState = LEDState::OFF;
    ledcWrite(_ledChannel, 0);
}

void UIController::updateLED() {
    uint32_t now = millis();

    switch (_ledState) {
        case LEDState::BREATHING:
            if (now - _lastBreathUpdate >= BREATH_STEP_MS) {
                _lastBreathUpdate = now;

                _breathBrightness += _breathDirection * 2;

                if (_breathBrightness >= BREATH_MAX) {
                    _breathBrightness = BREATH_MAX;
                    _breathDirection = -1;
                } else if (_breathBrightness <= BREATH_MIN) {
                    _breathBrightness = BREATH_MIN;
                    _breathDirection = 1;
                }

                ledcWrite(_ledChannel, _breathBrightness);
            }
            break;

        case LEDState::SOLID:
            ledcWrite(_ledChannel, BREATH_MAX);
            break;

        case LEDState::BLINK_FAST:
            // Nhấp nháy 5Hz
            ledcWrite(_ledChannel, ((now / 100) % 2 == 0) ? BREATH_MAX : 0);
            break;

        case LEDState::OFF:
        default:
            break;
    }
}

void UIController::setLEDState(LEDState state) {
    _ledState = state;
    if (state == LEDState::OFF) {
        ledcWrite(_ledChannel, 0);
    }
}

// ============================================================================
// Touch Debounce
// ============================================================================

bool UIController::isTouched() {
    bool currentState = digitalRead(_touchPin) == HIGH;
    uint32_t now = millis();

    // Nếu trạng thái thay đổi, reset timer debounce
    if (currentState != _lastTouchState) {
        _lastDebounceTime = now;
        _lastTouchState = currentState;
    }

    // Nếu tín hiệu ổn định hơn TOUCH_DEBOUNCE_MS
    if ((now - _lastDebounceTime) > TOUCH_DEBOUNCE_MS) {
        if (currentState && !_touchConfirmed) {
            _touchConfirmed = true;
            return true; // Sự kiện chạm hợp lệ
        }

        if (!currentState) {
            _touchConfirmed = false; // Đã thả tay
        }
    }

    return false;
}

void UIController::resetTouch() {
    _touchConfirmed = false;
}

// ============================================================================
// Display UI
// ============================================================================

void UIController::showClock(uint32_t durationMs) {
    if (_display == nullptr || _timeMgr == nullptr) return;

    struct tm timeInfo;
    if (!_timeMgr->getCurrentTime(&timeInfo)) {
        _display->showMessage("No Time");
        _display->setBacklight(BACKLIGHT_DAY_PERCENT);
        vTaskDelay(pdMS_TO_TICKS(durationMs));
        _display->turnOff();
        return;
    }

    _display->turnOn();
    _display->drawClockFace(timeInfo.tm_hour, timeInfo.tm_min);

    // Vẽ thanh trạng thái nếu có BatteryMonitor
    if (_battery != nullptr) {
        _display->drawStatusBar(_battery->getPercentage(), WiFi.status() == WL_CONNECTED);
    }

    _display->setBacklight(BACKLIGHT_DAY_PERCENT);

    // Hiển thị trong durationMs
    vTaskDelay(pdMS_TO_TICKS(durationMs));

    _display->turnOff();
}

void UIController::showConnecting() {
    if (_display == nullptr) return;
    _display->turnOn();
    _display->showMessage("Connecting...");
    _display->setBacklight(BACKLIGHT_NIGHT_PERCENT);
}

void UIController::showDownloading() {
    if (_display == nullptr) return;
    _display->showMessage("Downloading...");
}

void UIController::showError(const char* message) {
    if (_display == nullptr) return;
    _display->turnOn();
    _display->showMessage(message);
    _display->setBacklight(BACKLIGHT_DAY_PERCENT);
    setLEDState(LEDState::BLINK_FAST);
}

void UIController::showBootScreen() {
    if (_display == nullptr) return;
    _display->turnOn();
    _display->clear();
    _display->showMessage("Sendlove Box");
    _display->setBacklight(BACKLIGHT_DAY_PERCENT);
    vTaskDelay(pdMS_TO_TICKS(2000)); // Hiển thị 2 giây
}
