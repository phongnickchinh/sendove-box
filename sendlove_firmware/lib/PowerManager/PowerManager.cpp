#include "PowerManager.h"
#include "esp_sleep.h"
#include "driver/gpio.h"

// ============================================================================
// PowerManager Implementation
// ============================================================================

void PowerManager::init(gpio_num_t touchPin) {
    _touchPin = touchPin;

    // Cấu hình chân touch là INPUT
    gpio_set_direction(_touchPin, GPIO_MODE_INPUT);
    gpio_set_pull_mode(_touchPin, GPIO_PULLDOWN_ONLY);
}

void PowerManager::configureTimerWakeup(uint64_t sleepDurationUs) {
    esp_sleep_enable_timer_wakeup(sleepDurationUs);
}

void PowerManager::configureTouchWakeup(gpio_num_t touchPin) {
    // Cấu hình GPIO wakeup: thức dậy khi chân touch chuyển HIGH
    esp_sleep_enable_gpio_wakeup();
    gpio_wakeup_enable(touchPin, GPIO_INTR_HIGH_LEVEL);
}

void PowerManager::enterLightSleep() {
    // Đảm bảo Wi-Fi đã tắt trước khi ngủ để tiết kiệm năng lượng
    // (NetworkHandler chịu trách nhiệm gọi disconnectWiFi() trước)

    Serial.println(F("[PowerManager] Entering Light-sleep..."));
    Serial.flush(); // Đảm bảo log được gửi hết trước khi ngủ

    esp_light_sleep_start();

    // === Code tiếp tục chạy từ đây sau khi thức dậy ===
    Serial.println(F("[PowerManager] Woke up from Light-sleep"));
}

WakeupCause PowerManager::getWakeupCause() {
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();

    switch (cause) {
        case ESP_SLEEP_WAKEUP_TIMER:
            Serial.println(F("[PowerManager] Wakeup cause: TIMER"));
            return WakeupCause::TIMER;

        case ESP_SLEEP_WAKEUP_GPIO:
            Serial.println(F("[PowerManager] Wakeup cause: TOUCH (GPIO)"));
            return WakeupCause::TOUCH;

        default:
            // Lần khởi động đầu tiên hoặc reset
            Serial.println(F("[PowerManager] Wakeup cause: POWER_ON / RESET"));
            return WakeupCause::POWER_ON;
    }
}
