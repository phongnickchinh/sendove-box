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
// Chống nhiễu tín hiệu chạm TTP223 + màn hình chào lúc boot.
//
// Các hàm LED (breathing/solid/blink) đã bị xoá 2026-09-18: bo mạch chưa nối chân
// LED nào (PIN_LED vẫn đang comment trong config.h), thân hàm rỗng, và updateLED()
// được gọi mỗi 10ms trong vòng lặp chính chỉ để không làm gì. Đồng hồ và thanh pin
// giờ do LayoutEngine vẽ.
// ============================================================================

// VERY_LONG_PRESS: giữ TOUCH_OTA_HOLD_MS (6s) — bước cuối của chuỗi chạm OTA
// (main.cpp). LONG_PRESS 3s vẫn bắn trước trong cùng một lần giữ.
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

    /// Đang giữ bao lâu (ms), 0 khi không chạm. Để task render vẽ thanh tiến trình
    /// cho cú giữ 6s cuối của chuỗi OTA.
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
    // Ghi ở Task_UIController, đọc ở Task_MediaPlayer. Một từ 32-bit căn lề nên
    // đọc/ghi là nguyên tử trên RV32; chỉ dùng để vẽ, lệch một nhịp là vô hại.
    volatile uint32_t _touchHoldMs = 0;
};

#endif // UI_CONTROLLER_H
