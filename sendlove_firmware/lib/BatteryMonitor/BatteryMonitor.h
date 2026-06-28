#ifndef BATTERY_MONITOR_H
#define BATTERY_MONITOR_H

#include <Arduino.h>

// ============================================================================
// BatteryMonitor — Đọc ADC pin Lipo, tính phần trăm
// ============================================================================
// Module chia sẻ — được gọi bởi:
// - NetworkHandler (báo cáo battery_percent lên Firebase)
// - UIController (hiển thị icon pin trên màn hình)
// ============================================================================

class BatteryMonitor {
public:
    /// Khởi tạo chân ADC
    /// @param adcPin Chân ADC nối với voltage divider
    /// @param dividerRatio Tỷ lệ voltage divider (VD: 2.0 cho R1=R2)
    void init(uint8_t adcPin, float dividerRatio = 2.0f);

    /// Đọc điện áp thô từ ADC (đã nhân tỷ lệ divider)
    /// @return Điện áp thực của pin (Volt)
    float readVoltage();

    /// Quy đổi điện áp → phần trăm pin (theo LiPo discharge curve)
    /// @return Phần trăm pin (0–100)
    uint8_t getPercentage();

    /// Kiểm tra pin yếu
    /// @param threshold Ngưỡng cảnh báo (mặc định 10%)
    /// @return true nếu pin dưới ngưỡng
    bool isLowBattery(uint8_t threshold = 10);

private:
    uint8_t _adcPin = 0;
    float   _dividerRatio = 2.0f;

    /// Ánh xạ tuyến tính voltage → percentage (đơn giản hóa)
    /// Có thể thay bằng bảng tra LiPo curve cho chính xác hơn
    uint8_t voltageToPercent(float voltage);
};

#endif // BATTERY_MONITOR_H
