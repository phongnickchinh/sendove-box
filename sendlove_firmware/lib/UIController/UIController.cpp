#include "UIController.h"
#include "DisplayDriver.h"
#include "config.h"

// ============================================================================
// UIController Implementation — Phase 1
// ============================================================================

void UIController::init(uint8_t touchPin, DisplayDriver* display) {
    _touchPin = touchPin;
    _display  = display;

    // Cấu hình chân touch: INPUT_PULLDOWN vì TTP223 active HIGH
    pinMode(_touchPin, INPUT_PULLDOWN);

    Serial.println(F("[UI] Initialized (Touch + Display)"));
}

// ============================================================================
// LED Control — Phase 2 (no-op stubs)
// ============================================================================

void UIController::startBreathingLED() {
    _ledState = LEDState::BREATHING;
    // Phase 2: ledcSetup + ledcAttachPin
}

void UIController::stopBreathingLED() {
    _ledState = LEDState::OFF;
}

void UIController::updateLED() {
    // Phase 2: LED breathing animation
}

void UIController::setLEDState(LEDState state) {
    _ledState = state;
}

// ============================================================================
// Touch Debounce
// ============================================================================

bool UIController::isTouched() {
    bool currentState = digitalRead(_touchPin) == HIGH;
    uint32_t now = millis();
    bool triggered = false;

    if (currentState) {
        if (_touchStartTime == 0) {
            _touchStartTime = now;
        } else if ((now - _touchStartTime) >= 30 && !_touchConfirmed) {
            // Chỉ công nhận là chạm thật khi giữ HIGH liên tục >= 30ms
            _touchConfirmed = true;
            triggered = true;
        }
    } else {
        _touchStartTime = 0;
        _touchConfirmed = false;
    }

    return triggered;
}

void UIController::resetTouch() {
    _touchConfirmed = false;
}

// ============================================================================
// Display UI
// ============================================================================

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
