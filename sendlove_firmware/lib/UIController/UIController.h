#ifndef UI_CONTROLLER_H
#define UI_CONTROLLER_H

#include <Arduino.h>

// Forward declarations
class DisplayDriver;

// ============================================================================
// UIController — Touch Debounce + Display UI
// ============================================================================
// Serves Task_UIController: debounces the TTP223 touch signal and shows the boot
// screen. There are no LED functions (no LED pin is wired; PIN_LED is commented
// out in config.h). The clock and battery bar are drawn by LayoutEngine.
// ============================================================================

// VERY_LONG_PRESS: a TOUCH_OTA_HOLD_MS (6s) hold — the last step of the OTA touch
// sequence (main.cpp). The 3s LONG_PRESS still fires first within the same hold.
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

    /// How long the touch has been held (ms), 0 when not touching. Lets the render
    /// task draw the progress bar for the final 6s hold of the OTA sequence.
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
    // Written by Task_UIController, read by Task_MediaPlayer. One aligned 32-bit
    // word, so reads/writes are atomic on RV32; it is only used for drawing, and
    // being one tick stale is harmless.
    volatile uint32_t _touchHoldMs = 0;
};

#endif // UI_CONTROLLER_H
