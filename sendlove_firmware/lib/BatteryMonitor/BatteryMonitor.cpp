#include "BatteryMonitor.h"
#include "config.h"

// ============================================================================
// BatteryMonitor Implementation
// ============================================================================

void BatteryMonitor::init(uint8_t adcPin, float dividerRatio) {
    _adcPin = adcPin;
    _dividerRatio = dividerRatio;

    // ESP32-C3 ADC: 12-bit resolution (0–4095), 0–2.5V range (with attenuation)
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db); // Cho phép đo đến ~2.5V

    Serial.printf("[BatteryMonitor] Initialized on GPIO%d, divider ratio=%.1f\n",
                  _adcPin, _dividerRatio);
}

float BatteryMonitor::readVoltage() {
    // Đọc giá trị ADC thô (0–4095)
    int rawADC = analogRead(_adcPin);

    // Chuyển đổi sang điện áp tại chân ADC
    // ESP32-C3 với ADC_11db: Vref ~2.5V
    float adcVoltage = (rawADC / 4095.0f) * 2.5f;

    // Nhân với tỷ lệ voltage divider để ra điện áp thực của pin
    float batteryVoltage = adcVoltage * _dividerRatio;

    return batteryVoltage;
}

uint8_t BatteryMonitor::getPercentage() {
    float voltage = readVoltage();
    return voltageToPercent(voltage);
}

bool BatteryMonitor::isLowBattery(uint8_t threshold) {
    return getPercentage() < threshold;
}

uint8_t BatteryMonitor::voltageToPercent(float voltage) {
    // Bảng tra đơn giản hóa cho pin LiPo 3.7V:
    // 4.20V = 100%
    // 3.90V = 80%
    // 3.80V = 60%
    // 3.70V = 40%
    // 3.60V = 20%
    // 3.30V = 5%
    // 3.00V = 0% (ngưỡng cắt)

    if (voltage >= BATTERY_FULL_VOLTAGE)  return 100;
    if (voltage <= BATTERY_EMPTY_VOLTAGE) return 0;

    // Ánh xạ tuyến tính đơn giản (có thể cải thiện bằng lookup table)
    float range = BATTERY_FULL_VOLTAGE - BATTERY_EMPTY_VOLTAGE;
    float percent = ((voltage - BATTERY_EMPTY_VOLTAGE) / range) * 100.0f;

    return (uint8_t)constrain(percent, 0.0f, 100.0f);
}
