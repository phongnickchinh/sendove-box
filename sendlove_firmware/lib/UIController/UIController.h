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
    OFF,            // Tắt hoàn toàn
    BREATHING,      // Nhịp thở (có tin nhắn mới)
    SOLID,          // Sáng liên tục
    BLINK_FAST      // Nhấp nháy nhanh (lỗi)
};

class UIController {
public:
    /// Khởi tạo module (Phase 1: chỉ touch + display)
    /// @param touchPin Chân GPIO cho cảm biến TTP223
    /// @param display Con trỏ tới DisplayDriver
    void init(uint8_t touchPin, DisplayDriver* display);

    // --- LED Control (Phase 2 — no-op hiện tại) ---
    void startBreathingLED();
    void stopBreathingLED();
    void updateLED();
    void setLEDState(LEDState state);

    // --- Touch Debounce ---

    /// Đọc trạng thái chạm đã debounce
    /// @return true nếu có sự kiện chạm hợp lệ (rising edge, ổn định > DEBOUNCE_MS)
    bool isTouched();

    /// Reset trạng thái touch (gọi sau khi đã xử lý sự kiện chạm)
    void resetTouch();

    // --- Display UI ---

    /// Hiển thị trạng thái "đang kết nối Wi-Fi"
    void showConnecting();

    /// Hiển thị trạng thái "đang tải dữ liệu"
    void showDownloading();

    /// Hiển thị lỗi
    void showError(const char* message);

    /// Hiển thị trang chào khi khởi động
    void showBootScreen();

private:
    uint8_t _touchPin = 0;

    DisplayDriver*  _display = nullptr;

    LEDState _ledState = LEDState::OFF;

    // Touch debounce state
    bool     _lastTouchState   = false;
    bool     _touchConfirmed   = false;
    uint32_t _lastDebounceTime = 0;
};

#endif // UI_CONTROLLER_H
