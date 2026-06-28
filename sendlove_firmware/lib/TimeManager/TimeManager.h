#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include <Arduino.h>
#include <time.h>

// ============================================================================
// TimeManager — Quản lý thời gian: NTP sync, giờ local, kiểm tra alarm
// ============================================================================
// Module chia sẻ — được gọi bởi:
// - NetworkHandler (sau khi có Wi-Fi → syncNTP)
// - PowerManager (kiểm tra alarm khi thức dậy)
// - UIController (hiển thị đồng hồ)
// ============================================================================

class TimeManager {
public:
    /// Đồng bộ thời gian qua NTP server
    /// @param timezoneOffsetHours Múi giờ (VD: 7 cho GMT+7)
    /// @return true nếu sync thành công
    bool syncNTP(int8_t timezoneOffsetHours = 7);

    /// Lấy giờ local hiện tại
    /// @param timeInfo Con trỏ struct tm để nhận kết quả
    /// @return true nếu thời gian hợp lệ (đã sync NTP)
    bool getCurrentTime(struct tm* timeInfo);

    /// Kiểm tra xem giờ hiện tại có khớp với giờ báo thức không
    /// @param alarmTimeStr Chuỗi giờ báo thức (format "HH:MM")
    /// @return true nếu giờ hiện tại khớp (cùng giờ và phút)
    bool isAlarmTriggered(const char* alarmTimeStr);

    /// Cập nhật múi giờ
    /// @param timezoneOffsetHours Múi giờ mới
    void setTimezone(int8_t timezoneOffsetHours);

    /// Kiểm tra xem NTP đã được đồng bộ chưa
    bool isSynced() const;

private:
    int8_t _timezoneOffset = 7; // Mặc định GMT+7 (Việt Nam)
    bool   _synced = false;
};

#endif // TIME_MANAGER_H
