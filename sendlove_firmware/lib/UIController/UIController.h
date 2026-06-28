#ifndef UI_CONTROLLER_H
#define UI_CONTROLLER_H

#include <Arduino.h>

// Forward declarations
class DisplayDriver;
class BatteryMonitor;
class TimeManager;

// ============================================================================
// UIController — LED Breathing, Debounce, Clock Face, Status Display
// ============================================================================
// Phục vụ Task 4: Task_UI_Controller
//
// Chức năng:
// - Hiệu ứng LED nhịp thở (Breathing PWM)
// - Chống nhiễu tín hiệu chạm TTP223 (Debounce)
// - Hiển thị mặt đồng hồ khi chạm mà không có tin nhắn
// - Hiển thị icon trạng thái
// ============================================================================

/// Trạng thái hiển thị LED
enum class LEDState : uint8_t {
    OFF,            // Tắt hoàn toàn
    BREATHING,      // Nhịp thở (có tin nhắn mới)
    SOLID,          // Sáng liên tục
    BLINK_FAST      // Nhấp nháy nhanh (lỗi)
};

class UIController {
public:
    /// Khởi tạo module
    /// @param ledPin Chân GPIO cho LED indicator
    /// @param touchPin Chân GPIO cho cảm biến TTP223
    /// @param display Con trỏ tới DisplayDriver
    /// @param battery Con trỏ tới BatteryMonitor
    /// @param timeMgr Con trỏ tới TimeManager
    void init(uint8_t ledPin, uint8_t touchPin,
              DisplayDriver* display, BatteryMonitor* battery,
              TimeManager* timeMgr);

    // --- LED Control ---

    /// Bật hiệu ứng LED nhịp thở (gọi update() liên tục trong task loop)
    void startBreathingLED();

    /// Tắt LED
    void stopBreathingLED();

    /// Cập nhật hiệu ứng LED (gọi mỗi ~10ms trong task loop)
    void updateLED();

    /// Đặt trạng thái LED
    void setLEDState(LEDState state);

    // --- Touch Debounce ---

    /// Đọc trạng thái chạm đã debounce
    /// @return true nếu có sự kiện chạm hợp lệ (tín hiệu ổn định > DEBOUNCE_MS)
    bool isTouched();

    /// Reset trạng thái touch (gọi sau khi đã xử lý sự kiện chạm)
    void resetTouch();

    // --- Display UI ---

    /// Hiển thị mặt đồng hồ trong khoảng thời gian nhất định
    /// @param durationMs Thời gian hiển thị (ms), mặc định 5 giây
    void showClock(uint32_t durationMs = 5000);

    /// Hiển thị trạng thái "đang kết nối Wi-Fi"
    void showConnecting();

    /// Hiển thị trạng thái "đang tải dữ liệu"
    void showDownloading();

    /// Hiển thị lỗi
    void showError(const char* message);

    /// Hiển thị trang chào khi khởi động
    void showBootScreen();

private:
    uint8_t _ledPin   = 0;
    uint8_t _touchPin = 0;
    uint8_t _ledChannel = 1; // LEDC channel (khác với DisplayDriver dùng channel 0)

    DisplayDriver*  _display = nullptr;
    BatteryMonitor* _battery = nullptr;
    TimeManager*    _timeMgr = nullptr;

    LEDState _ledState = LEDState::OFF;

    // Breathing LED state
    uint8_t  _breathBrightness = 0;
    int8_t   _breathDirection  = 1; // 1 = sáng dần, -1 = tối dần
    uint32_t _lastBreathUpdate = 0;

    // Touch debounce state
    bool     _lastTouchState   = false;
    bool     _touchConfirmed   = false;
    uint32_t _lastDebounceTime = 0;
};

#endif // UI_CONTROLLER_H
