#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include <Arduino.h>

// ============================================================================
// PowerManager — Quản lý trạng thái nguồn và Sleep/Wake
// ============================================================================
// Chịu trách nhiệm:
// - Cấu hình Timer Wakeup (5 phút) và Touch Wakeup (GPIO Interrupt)
// - Đưa ESP32 vào Light-sleep
// - Xác định nguyên nhân thức dậy và phân luồng sự kiện
// ============================================================================

/// Nguyên nhân thức dậy
enum class WakeupCause : uint8_t {
    POWER_ON,       // Khởi động lần đầu hoặc reset
    TIMER,          // Hết 5 phút timer
    TOUCH,          // Cảm biến chạm TTP223
    UNKNOWN         // Không xác định
};

class PowerManager {
public:
    /// Khởi tạo module, cấu hình chân GPIO cho touch wakeup
    /// @param touchPin Chân GPIO nối với TTP223
    void init(gpio_num_t touchPin);

    /// Cấu hình timer wakeup
    /// @param sleepDurationUs Thời gian ngủ (microseconds), mặc định 5 phút
    void configureTimerWakeup(uint64_t sleepDurationUs);

    /// Cấu hình GPIO wakeup cho cảm biến chạm
    /// @param touchPin Chân GPIO
    void configureTouchWakeup(gpio_num_t touchPin);

    /// Đưa ESP32 vào Light-sleep. Hàm này block cho đến khi thức dậy.
    void enterLightSleep();

    /// Xác định nguyên nhân thức dậy sau khi thoát Light-sleep
    /// @return WakeupCause enum
    WakeupCause getWakeupCause();

private:
    gpio_num_t _touchPin;
};

#endif // POWER_MANAGER_H
