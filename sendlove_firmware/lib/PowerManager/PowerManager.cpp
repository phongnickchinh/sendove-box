#include "PowerManager.h"
#include <esp_sleep.h>

void PowerManager::init(gpio_num_t touchPin) {
    _touchPin = touchPin;
    configureTouchWakeup(_touchPin);
    Serial.printf("[Power] PowerManager initialized with touch pin GPIO_%d\n", _touchPin);
}

void PowerManager::initBattery(uint8_t adcPin, float dividerRatio) {
    _adcPin = adcPin;
    _dividerRatio = dividerRatio;
    pinMode(_adcPin, INPUT);
    analogReadResolution(12); // ESP32 12-bit ADC (0 - 4095)
    Serial.printf("[Power] Battery monitor initialized on ADC pin %d (ratio: %.1f)\n", _adcPin, _dividerRatio);
}

float PowerManager::getBatteryVoltage() {
    // TODO: Khi gắn pin trực tiếp vào phần cứng (qua mạch chia điện áp ADC),
    // hãy xóa bỏ kiểm tra (_adcPin == 0) và logic fallback USB mode ở bên dưới.
    if (_adcPin == 0) return 4.0f; // Mặc định 4.0V (90% pin) khi chưa cấu hình ADC pin

    uint32_t rawSum = 0;
    for (int i = 0; i < 10; i++) {
        rawSum += analogRead(_adcPin);
        delay(2);
    }
    float rawAvg = (float)rawSum / 10.0f;

    // Quy đổi ADC (0-4095 -> 0-3.3V) và nhân tỷ lệ voltage divider
    float pinVoltage = (rawAvg / 4095.0f) * 3.3f;
    float batteryVoltage = pinVoltage * _dividerRatio;

    // TODO [MOCK/TESTING USB POWER]:
    // Khi cắm cáp USB (chưa gắn pin), chân ADC chưa cắm pin sẽ bị trôi (floating/0V).
    // Nếu điện áp đọc được < 1.0V, coi như đang cấp nguồn USB trực tiếp (không dùng pin) -> Giả lập 4.0V (90% Pin)
    // để tránh hệ thống luôn hiểu nhầm là pin 0% và tự động nhảy vào DEEP_SLEEP liên tục.
    if (batteryVoltage < 1.0f) {
        // Serial.println(F("[Power] USB Power detected (No battery attached) -> Mocking 4.0V"));
        return 4.0f;
    }

    return batteryVoltage;
}

uint8_t PowerManager::getBatteryPercentage() {
    float voltage = getBatteryVoltage();
    return voltageToPercent(voltage);
}

bool PowerManager::isLowBattery(uint8_t threshold) {
    return (getBatteryPercentage() < threshold);
}

SleepMode PowerManager::getRecommendedSleepMode(uint8_t lowBatteryThreshold) {
    if (isLowBattery(lowBatteryThreshold)) {
        Serial.println(F("[Power] Low battery detected -> Recommended: DEEP_SLEEP"));
        return SleepMode::DEEP_SLEEP;
    }
    return SleepMode::LIGHT_SLEEP;
}

void PowerManager::enterSleep(SleepMode mode, uint64_t sleepDurationUs, DisplayDriver* display) {
    if (mode == SleepMode::DEEP_SLEEP) {
        enterDeepSleep(sleepDurationUs, display);
    } else {
        enterLightSleep(sleepDurationUs, display);
    }
}

#include "driver/gpio.h"

void PowerManager::enterLightSleep(uint64_t sleepDurationUs, DisplayDriver* display) {

    // 1. Chờ cảm ứng TTP223 nhả hẳn ra (LOW) trước khi vào ngủ
    // Tránh trường hợp tay vừa chạm xong chưa nhả đã vào ngủ làm trigger wakeup ngay lập tức (<1ms)
    uint32_t waitStart = millis();
    while (digitalRead(_touchPin) == HIGH && (millis() - waitStart < 2000)) {
        delay(10);
    }

    // 2. Tắt màn hình TFT (Tắt backlight + đưa ST7789 vào sleep mode)
    if (display != nullptr) {
        display->turnOff();
    }

    // 3. Cấu hình nguồn thức dậy (Timer + Touch GPIO)
    configureTimerWakeup(sleepDurationUs);
    configureTouchWakeup(_touchPin);

    // 4. Tiến hành ngủ Light-sleep (CPU dừng, RAM giữ nguyên)
    esp_light_sleep_start();

    // 5. Ngay khi tỉnh dậy: Đọc ngay lý do thức dậy TRƯỚC KHI bật màn hình
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();

    // 6. Bật lại màn hình TFT (full re-init bao gồm fill đen)
    if (display != nullptr && cause != ESP_SLEEP_WAKEUP_TIMER) {
        display->turnOn();
        // Không cần gọi display->clear() vì turnOn() đã gọi _tft.init() + fillScreen(TFT_BLACK)
    }

    // 7. IN BANNER RÕ RÀNG TRÊN SERIAL MONITOR ĐỂ ĐÁNH DẤU NGUỒN WAKEUP
    // [FIX-3] USB CDC cần thời gian re-enumerate sau Light Sleep, chờ thêm
    delay(200); // Chờ USB CDC re-enumerate (tối thiểu 200ms cho ESP32-C3)
    if (Serial) {
        Serial.println(F("\n=================================================="));
        if (cause == ESP_SLEEP_WAKEUP_GPIO) {
            Serial.println(F(" *** [WAKEUP SUCCESS] TOUCH SENSOR (TTP223 / GPIO_10) WOKE UP THE SYSTEM! ***"));
        } else if (cause == ESP_SLEEP_WAKEUP_TIMER) {
            Serial.println(F(" *** [WAKEUP] TIMER WOKE UP THE SYSTEM (SLEEP TIMEOUT EXPIRED) ***"));
        } else {
            Serial.printf(" *** [WAKEUP] WOKEN UP BY OTHER SOURCE (Cause Code: %d) ***\n", (int)cause);
        }
        Serial.println(F("==================================================\n"));
    }
}

void PowerManager::enterDeepSleep(uint64_t sleepDurationUs, DisplayDriver* display) {
    Serial.printf("[Power] Entering DEEP SLEEP for %llu s... (Will reboot on wakeup)\n", sleepDurationUs / 1000000ULL);

    // 1. Tắt màn hình TFT
    if (display != nullptr) {
        display->turnOff();
    }

    // 2. Cấu hình wakeup nguồn Deep Sleep
    esp_sleep_enable_timer_wakeup(sleepDurationUs);

    // TODO [HARDWARE LIMITATION - ESP32-C3]:
    // Trên vi điều khiển ESP32-C3, tính năng Deep Sleep GPIO Wakeup CHỈ HỖ TRỢ các chân RTC GPIO (GPIO 0 -> GPIO 5).
    // Chân TTP223 hiện tại gắn ở GPIO 10 (không phải RTC GPIO), do đó KHÔNG THỂ kích hoạt wakeup từ DEEP SLEEP bằng cảm ứng chạm TTP223.
    // -> Khi gắn pin chính thức, nếu muốn dùng Deep Sleep + chạm TTP223 để mở hộp, cần chuyển chân TTP223 sang GPIO 0, 1, 2, 3, 4 hoặc 5!
    // -> Ở chế độ Light-sleep (chân GPIO 0-21), GPIO 10 vẫn hoạt động đánh thức bình thường.
    if (_touchPin <= GPIO_NUM_5) {
        esp_deep_sleep_enable_gpio_wakeup((1ULL << _touchPin), ESP_GPIO_WAKEUP_GPIO_HIGH);
    } else {
        Serial.printf("[Power] WARNING: GPIO_%d is NOT RTC GPIO on ESP32-C3! Deep sleep touch wakeup disabled.\n", _touchPin);
    }

    esp_deep_sleep_start();
    // esp_deep_sleep_start() không bao giờ return vì chip reboot khi thức dậy
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


/**
 * @brief Cấu hình chân GPIO cảm ứng làm nguồn đánh thức chip khỏi Light-Sleep khi chạm vào (mức HIGH)
 * @param touchPin Chân GPIO của cảm ứng
 */
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
        case ESP_SLEEP_WAKEUP_TIMER:
            return WakeupCause::TIMER;
        case ESP_SLEEP_WAKEUP_GPIO:
            return WakeupCause::TOUCH;
        default:
            return WakeupCause::POWER_ON;
    }
}
