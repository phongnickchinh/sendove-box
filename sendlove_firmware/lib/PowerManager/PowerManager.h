#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include <Arduino.h>
#include "DisplayDriver.h"

/// Nguyên nhân thức dậy
enum class WakeupCause : uint8_t {
    POWER_ON,       // Khởi động lần đầu hoặc reset
    TIMER,          // Hết timer wakeup (5ph / 15ph)
    TOUCH,          // Cảm biến chạm TTP223
    UNKNOWN         // Không xác định
};

/// Chế độ ngủ hỗ trợ
enum class SleepMode : uint8_t {
    LIGHT_SLEEP,    // Tạm dừng CPU, giữ RAM, thức dậy tức thì (1ms)
    DEEP_SLEEP      // Tắt CPU/RAM/Wi-Fi (5µA), reboot khi thức dậy
};

class PowerManager {
public:
    PowerManager() = default;

    /// Khởi tạo module PowerManager, cấu hình chân GPIO cho touch wakeup
    void init(gpio_num_t touchPin);

    /// Khởi tạo chân ADC đọc dung lượng Pin
    void initBattery(uint8_t adcPin, float dividerRatio = 2.0f);

    /// Đọc điện áp pin (Volt)
    float getBatteryVoltage();

    /// Quy đổi điện áp → phần trăm pin (0-100%)
    uint8_t getBatteryPercentage();

    /// Kiểm tra pin yếu
    bool isLowBattery(uint8_t threshold = 10);

    /// Tự động gợi ý chế độ ngủ tối ưu dựa trên % pin hiện tại
    /// @param lowBatteryThreshold Ngưỡng chuyển sang Deep Sleep (mặc định 30%)
    SleepMode getRecommendedSleepMode(uint8_t lowBatteryThreshold = 30);

    /// Đưa ESP32 vào chế độ ngủ tương ứng (LIGHT_SLEEP hoặc DEEP_SLEEP)
    /// Tự động tắt màn hình TFT trước khi ngủ và bật lại sau khi thức dậy (với Light-sleep)
    /// @param mode Chế độ ngủ
    /// @param sleepDurationUs Thời gian ngủ (microseconds)
    /// @param display Pointer đến DisplayDriver để tắt/bật màn hình (tùy chọn)
    void enterSleep(SleepMode mode, uint64_t sleepDurationUs, DisplayDriver* display = nullptr);

    /// Đưa ESP32 vào Light-sleep
    void enterLightSleep(uint64_t sleepDurationUs, DisplayDriver* display = nullptr);

    /// Đưa ESP32 vào Deep-sleep (reboot khi thức dậy)
    void enterDeepSleep(uint64_t sleepDurationUs, DisplayDriver* display = nullptr);

    /// Cấu hình timer wakeup
    void configureTimerWakeup(uint64_t sleepDurationUs);

    /// Cấu hình GPIO wakeup cho cảm biến chạm
    void configureTouchWakeup(gpio_num_t touchPin);

    /// Xác định nguyên nhân thức dậy
    WakeupCause getWakeupCause();

private:
    gpio_num_t _touchPin = GPIO_NUM_10;
    uint8_t    _adcPin = 0;
    float      _dividerRatio = 2.0f;

    uint8_t voltageToPercent(float voltage);
};

#endif // POWER_MANAGER_H
