#include "UIController.h"
#include "DisplayDriver.h"
#include "ScreenLogger.h"
#include "config.h"

void UIController::init(uint8_t touchPin, DisplayDriver* display) {
    _touchPin = touchPin;
    _display  = display;
    pinMode(_touchPin, INPUT_PULLDOWN);
}

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

TouchEvent UIController::getTouchEvent() {
    bool currentState = digitalRead(_touchPin) == HIGH;
    uint32_t now = millis();
    TouchEvent result = TouchEvent::NONE;

    if (currentState != _lastTouchState) {
        _lastTouchState = currentState;
        _lastDebounceTime = now;
    }

    if (currentState) {
        if (_touchStartTime == 0) {
            _touchStartTime = now;
        } else if ((now - _touchStartTime) >= 30 && !_touchConfirmed) {
            _touchConfirmed = true;
        }

        if (_touchConfirmed && (now - _touchStartTime) >= 3000 && !_longPressEmitted) {
            _longPressEmitted = true;
            result = TouchEvent::LONG_PRESS;
            DLOG("[UI] LONG_PRESS");
        }
    } else {
        if (_touchConfirmed && !_longPressEmitted) {
            result = TouchEvent::SHORT_PRESS;
            DLOG("[UI] SHORT_PRESS");
        }
        _touchStartTime = 0;
        _touchConfirmed = false;
        _longPressEmitted = false;
    }

    return result;
}

void UIController::resetTouch() {
    _touchConfirmed = false;
    _longPressEmitted = false;
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
    DLOG("[UI] ERR: %s", message ? message : "(null)");
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
    vTaskDelay(pdMS_TO_TICKS(2000));
}
