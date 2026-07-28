#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include <Arduino.h>
#include "DisplayDriver.h"

/// System wakeup cause
enum class WakeupCause : uint8_t {
    POWER_ON,
    TIMER,
    TOUCH,
    UNKNOWN
};

/// Supported sleep modes
enum class SleepMode : uint8_t {
    LIGHT_SLEEP,
    DEEP_SLEEP
};

/// Power and sleep management driver
class PowerManager {
public:
    PowerManager() = default;

    /// Initialize PowerManager and configure touch wakeup GPIO
    void init(gpio_num_t touchPin);

    /// Initialize battery ADC monitoring pin
    void initBattery(uint8_t adcPin, float dividerRatio = 2.0f);

    /// Read battery voltage in Volts
    float getBatteryVoltage();

    /// Convert battery voltage to percentage (0-100%)
    uint8_t getBatteryPercentage();

    /// Check if device is charging
    bool isCharging() { return false; }

    /// Check if battery is low
    bool isLowBattery(uint8_t threshold = 10);

    /// Get recommended sleep mode based on current battery percentage
    SleepMode getRecommendedSleepMode(uint8_t lowBatteryThreshold = 30);

    /// Enter specified sleep mode
    void enterSleep(SleepMode mode, uint64_t sleepDurationUs, DisplayDriver* display = nullptr);

    /// Enter Light Sleep mode
    void enterLightSleep(uint64_t sleepDurationUs, DisplayDriver* display = nullptr);

    /// Enter Deep Sleep mode (reboots on wakeup)
    void enterDeepSleep(uint64_t sleepDurationUs, DisplayDriver* display = nullptr);

    /// Configure timer wakeup source
    void configureTimerWakeup(uint64_t sleepDurationUs);

    /// Configure touch GPIO wakeup source
    void configureTouchWakeup(gpio_num_t touchPin);

    /// Get cause of current system wakeup
    WakeupCause getWakeupCause();

private:
    gpio_num_t _touchPin = GPIO_NUM_10;
    uint8_t    _adcPin = 0;
    float      _dividerRatio = 2.0f;

    uint8_t voltageToPercent(float voltage);
};

#endif // POWER_MANAGER_H
