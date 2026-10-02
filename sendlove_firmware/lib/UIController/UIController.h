#ifndef UI_CONTROLLER_H
#define UI_CONTROLLER_H

#include <Arduino.h>

// Forward declarations
class DisplayDriver;

// UIController — debounces the TTP223 touch signal and shows the boot screen.

// VERY_LONG_PRESS: a TOUCH_OTA_HOLD_MS hold (OTA sequence). LONG_PRESS still
// fires first within the same hold.
enum class TouchEvent { NONE, SHORT_PRESS, LONG_PRESS, VERY_LONG_PRESS };

/// UI controller and touch debounce manager
class UIController {
public:
    /// Initialize touch sensor pin and display reference
    void init(uint8_t touchPin, DisplayDriver* display);

    /// Read debounced touch sensor state
    TouchEvent getTouchEvent();

    /// Show startup boot logo screen
    void showBootScreen();

    /// How long the touch has been held (ms), 0 when not touching.
    uint32_t getTouchHoldMs() const { return _touchHoldMs; }

private:
    uint8_t _touchPin = 0;
    DisplayDriver* _display = nullptr;

    bool     _lastTouchState   = false;
    bool     _touchConfirmed   = false;
    uint32_t _lastDebounceTime = 0;
    uint32_t _touchStartTime   = 0;
    bool     _longPressEmitted = false;
    bool     _veryLongEmitted  = false;
    // Written by Task_UIController, read by Task_MediaPlayer (one aligned word:
    // atomic on RV32).
    volatile uint32_t _touchHoldMs = 0;
};

#endif // UI_CONTROLLER_H
