#ifndef UI_CONTROLLER_H
#define UI_CONTROLLER_H

#include <Arduino.h>

// Forward declarations
class DisplayDriver;

// ============================================================================
// UIController — Touch Debounce + Display UI
// ============================================================================
// Phục vụ Task_UI_Controller trong kiến trúc FreeRTOS.
//
// Phase 1:
// - Chống nhiễu tín hiệu chạm TTP223 (Debounce)
// - Hiển thị thông báo UI (boot, error, connecting)
//
// Phase 2 (chưa triển khai):
// - Hiệu ứng LED nhịp thở (Breathing PWM) — khi có PIN_LED
// - Mặt đồng hồ — khi có TimeManager
// - Thanh trạng thái pin — khi có BatteryMonitor
// ============================================================================

/// Trạng thái hiển thị LED (giữ enum cho Phase 2)
enum class LEDState : uint8_t {
    OFF,
    BREATHING,
    SOLID,
    BLINK_FAST
};

/// UI controller and touch debounce manager
class UIController {
public:
    /// Initialize touch sensor pin and display reference
    void init(uint8_t touchPin, DisplayDriver* display);

    void startBreathingLED();
    void stopBreathingLED();
    void updateLED();
    void setLEDState(LEDState state);

    /// Read debounced touch sensor state
    bool isTouched();

    /// Reset touch confirmation state
    void resetTouch();

    /// Show Wi-Fi connecting screen
    void showConnecting();

    /// Show data downloading screen
    void showDownloading();

    /// Show error message on display
    void showError(const char* message);

    /// Show startup boot logo screen
    void showBootScreen();

private:
    uint8_t _touchPin = 0;
    DisplayDriver* _display = nullptr;
    LEDState _ledState = LEDState::OFF;

    bool     _lastTouchState   = false;
    bool     _touchConfirmed   = false;
    uint32_t _lastDebounceTime = 0;
    uint32_t _touchStartTime   = 0;
};

#endif // UI_CONTROLLER_H
