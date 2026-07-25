#include "PowerManager.h"
#include <esp_sleep.h>
#include "driver/gpio.h"

void PowerManager::init(gpio_num_t touchPin) {
    _touchPin = touchPin;
    configureTouchWakeup(_touchPin);
}

void PowerManager::initBattery(uint8_t adcPin, float dividerRatio) {
    _adcPin = adcPin;
    _dividerRatio = dividerRatio;
    pinMode(_adcPin, INPUT);
    analogReadResolution(12);
}

float PowerManager::getBatteryVoltage() {
    // TODO: Remove _adcPin == 0 check and USB fallback when battery circuit is attached.
    if (_adcPin == 0) return 4.0f;

    uint32_t rawSum = 0;
    for (int i = 0; i < 10; i++) {
        rawSum += analogRead(_adcPin);
        delay(2);
    }
    float rawAvg = (float)rawSum / 10.0f;
    float pinVoltage = (rawAvg / 4095.0f) * 3.3f;
    float batteryVoltage = pinVoltage * _dividerRatio;

    // TODO [MOCK/TESTING USB POWER]: Floating ADC fallback
    if (batteryVoltage < 1.0f) return 4.0f;

    return batteryVoltage;
}

uint8_t PowerManager::getBatteryPercentage() {
    return voltageToPercent(getBatteryVoltage());
}

bool PowerManager::isLowBattery(uint8_t threshold) {
    return (getBatteryPercentage() < threshold);
}

SleepMode PowerManager::getRecommendedSleepMode(uint8_t lowBatteryThreshold) {
    return isLowBattery(lowBatteryThreshold) ? SleepMode::DEEP_SLEEP : SleepMode::LIGHT_SLEEP;
}

void PowerManager::enterSleep(SleepMode mode, uint64_t sleepDurationUs, DisplayDriver* display) {
    if (mode == SleepMode::DEEP_SLEEP) {
        enterDeepSleep(sleepDurationUs, display);
    } else {
        enterLightSleep(sleepDurationUs, display);
    }
}

void PowerManager::enterLightSleep(uint64_t sleepDurationUs, DisplayDriver* display) {
    uint32_t waitStart = millis();
    while (digitalRead(_touchPin) == HIGH && (millis() - waitStart < 2000)) delay(10);

    if (display != nullptr) display->turnOff();

    configureTimerWakeup(sleepDurationUs);
    configureTouchWakeup(_touchPin);

    esp_light_sleep_start();

    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    if (display != nullptr && cause != ESP_SLEEP_WAKEUP_TIMER) display->turnOn();

    delay(200);
}

void PowerManager::enterDeepSleep(uint64_t sleepDurationUs, DisplayDriver* display) {
    if (display != nullptr) display->turnOff();

    esp_sleep_enable_timer_wakeup(sleepDurationUs);

    // TODO [HARDWARE LIMITATION - ESP32-C3]: Deep sleep touch wakeup requires RTC GPIO 0-5
    if (_touchPin <= GPIO_NUM_5) {
        esp_deep_sleep_enable_gpio_wakeup((1ULL << _touchPin), ESP_GPIO_WAKEUP_GPIO_HIGH);
    }

    esp_deep_sleep_start();
}

uint8_t PowerManager::voltageToPercent(float voltage) {
    if (voltage >= 4.2f) return 100;
    if (voltage <= 3.0f) return 0;
    float percent = ((voltage - 3.0f) / (4.2f - 3.0f)) * 100.0f;
    return (uint8_t)constrain(percent, 0.0f, 100.0f);
}

void PowerManager::configureTimerWakeup(uint64_t sleepDurationUs) {
    esp_sleep_enable_timer_wakeup(sleepDurationUs);
}

void PowerManager::configureTouchWakeup(gpio_num_t touchPin) {
    gpio_config_t config = {};
    config.pin_bit_mask = (1ULL << touchPin);
    config.mode = GPIO_MODE_INPUT;
    config.pull_down_en = GPIO_PULLDOWN_ENABLE;
    config.pull_up_en = GPIO_PULLUP_DISABLE;
    config.intr_type = GPIO_INTR_HIGH_LEVEL;
    gpio_config(&config);

    gpio_set_intr_type(touchPin, GPIO_INTR_HIGH_LEVEL);
    gpio_wakeup_enable(touchPin, GPIO_INTR_HIGH_LEVEL);
    esp_sleep_enable_gpio_wakeup();
}

WakeupCause PowerManager::getWakeupCause() {
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    switch (cause) {
        case ESP_SLEEP_WAKEUP_TIMER: return WakeupCause::TIMER;
        case ESP_SLEEP_WAKEUP_GPIO:  return WakeupCause::TOUCH;
        default:                     return WakeupCause::POWER_ON;
    }
}
