#include "UIController.h"
#include "DisplayDriver.h"
#include "ScreenLogger.h"
#include "config.h"
#include "Settings.h"

void UIController::init(uint8_t touchPin, DisplayDriver* display) {
    _touchPin = touchPin;
    _display  = display;
    pinMode(_touchPin, INPUT_PULLDOWN);
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

        // Ngưỡng thứ hai trong CÙNG một lần giữ (6s, bước cuối của chuỗi OTA).
        // _longPressEmitted chặn mọi event sau mốc 3s cho tới khi nhả tay, nên phải
        // kiểm riêng cờ _veryLongEmitted để bắn đúng một lần.
        // KHÔNG đặt ngưỡng nào ≥ ~7s: TTP223 tự hiệu chuẩn sau 7-8s chạm liên tục
        // và báo là đã nhả (xem TOUCH_OTA_HOLD_MS trong config.h).
        if (_touchConfirmed && (now - _touchStartTime) >= TOUCH_OTA_HOLD_MS && !_veryLongEmitted) {
            _veryLongEmitted = true;
            result = TouchEvent::VERY_LONG_PRESS;
            DLOG("[UI] VERY_LONG_PRESS");
        }
        _touchHoldMs = _touchConfirmed ? (now - _touchStartTime) : 0;
    } else {
        if (_touchConfirmed && !_longPressEmitted) {
            result = TouchEvent::SHORT_PRESS;
            DLOG("[UI] SHORT_PRESS");
        }
        _touchStartTime = 0;
        _touchConfirmed = false;
        _longPressEmitted = false;
        _veryLongEmitted = false;
        _touchHoldMs = 0;
    }

    return result;
}

void UIController::showBootScreen() {
    if (_display == nullptr) return;
    _display->turnOn();
    _display->clear();
    _display->showMessage("Sendlove Box");
    _display->setBacklight(Settings::currentBacklight());
    vTaskDelay(pdMS_TO_TICKS(2000));
}
