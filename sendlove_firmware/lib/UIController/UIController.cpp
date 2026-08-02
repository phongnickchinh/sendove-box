#include "UIController.h"
#include "DisplayDriver.h"
#include "config.h"

void UIController::init(uint8_t touchPin, DisplayDriver* display) {
    _touchPin = touchPin;
    _display  = display;
    pinMode(_touchPin, INPUT_PULLDOWN);
    Serial.printf("[UIController] init: touchPin=%u display=%p\n", _touchPin, (void*)_display);
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
        Serial.printf("[UIController] raw touch state -> %s at %lu ms\n",
                      currentState ? "HIGH" : "LOW", (unsigned long)now);
        _lastTouchState = currentState;
        _lastDebounceTime = now;
    }

    if (currentState) {
        if (_touchStartTime == 0) {
            _touchStartTime = now;
            Serial.printf("[UIController] touch start at %lu ms\n", (unsigned long)now);
        } else if ((now - _touchStartTime) >= 30 && !_touchConfirmed) {
            _touchConfirmed = true;
            Serial.printf("[UIController] touch confirmed after %lu ms\n",
                          (unsigned long)(now - _touchStartTime));
        }

        if (_touchConfirmed && (now - _touchStartTime) >= 3000 && !_longPressEmitted) {
            _longPressEmitted = true;
            result = TouchEvent::LONG_PRESS;
            Serial.println("[UIController] LONG_PRESS detected!");
        }
    } else {
        if (_touchConfirmed && !_longPressEmitted) {
            result = TouchEvent::SHORT_PRESS;
            Serial.println("[UIController] SHORT_PRESS detected!");
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
    Serial.println(F("[UIController] showConnecting()"));
    _display->turnOn();
    _display->showMessage("Connecting...");
    _display->setBacklight(BACKLIGHT_NIGHT_PERCENT);
}

void UIController::showDownloading() {
    if (_display == nullptr) return;
    Serial.println(F("[UIController] showDownloading()"));
    _display->showMessage("Downloading...");
}

void UIController::showError(const char* message) {
    if (_display == nullptr) return;
    Serial.printf("[UIController] showError: %s\n", message ? message : "(null)");
    _display->turnOn();
    _display->showMessage(message);
    _display->setBacklight(BACKLIGHT_DAY_PERCENT);
    setLEDState(LEDState::BLINK_FAST);
}

void UIController::showBootScreen() {
    if (_display == nullptr) return;
    Serial.println(F("[UIController] showBootScreen()"));
    _display->turnOn();
    _display->clear();
    _display->showMessage("Sendlove Box");
    _display->setBacklight(BACKLIGHT_DAY_PERCENT);
    vTaskDelay(pdMS_TO_TICKS(2000));
}
