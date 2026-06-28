#include "TimeManager.h"

// ============================================================================
// TimeManager Implementation
// ============================================================================

bool TimeManager::syncNTP(int8_t timezoneOffsetHours) {
    _timezoneOffset = timezoneOffsetHours;

    // Cấu hình timezone cho hàm localtime()
    // Format: "UTC-7" có nghĩa là GMT+7 (POSIX convention ngược dấu)
    char tzStr[16];
    snprintf(tzStr, sizeof(tzStr), "UTC%d", -_timezoneOffset);
    setenv("TZ", tzStr, 1);
    tzset();

    // Khởi động NTP sync
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");

    // Chờ đồng bộ (timeout 10 giây)
    Serial.println(F("[TimeManager] Syncing NTP..."));
    struct tm timeInfo;
    uint32_t start = millis();
    while (!getLocalTime(&timeInfo, 1000)) {
        if (millis() - start > 10000) {
            Serial.println(F("[TimeManager] ERROR: NTP sync timeout"));
            _synced = false;
            return false;
        }
    }

    Serial.printf("[TimeManager] NTP synced: %02d:%02d:%02d %02d/%02d/%04d\n",
                  timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec,
                  timeInfo.tm_mday, timeInfo.tm_mon + 1, timeInfo.tm_year + 1900);
    _synced = true;
    return true;
}

bool TimeManager::getCurrentTime(struct tm* timeInfo) {
    if (!getLocalTime(timeInfo, 100)) {
        return false;
    }
    return true;
}

bool TimeManager::isAlarmTriggered(const char* alarmTimeStr) {
    if (!_synced || alarmTimeStr == nullptr) {
        return false;
    }

    // Parse chuỗi "HH:MM"
    int alarmHour = 0, alarmMin = 0;
    if (sscanf(alarmTimeStr, "%d:%d", &alarmHour, &alarmMin) != 2) {
        Serial.println(F("[TimeManager] ERROR: Invalid alarm time format"));
        return false;
    }

    struct tm now;
    if (!getCurrentTime(&now)) {
        return false;
    }

    // So sánh giờ và phút
    return (now.tm_hour == alarmHour && now.tm_min == alarmMin);
}

void TimeManager::setTimezone(int8_t timezoneOffsetHours) {
    _timezoneOffset = timezoneOffsetHours;

    char tzStr[16];
    snprintf(tzStr, sizeof(tzStr), "UTC%d", -_timezoneOffset);
    setenv("TZ", tzStr, 1);
    tzset();

    Serial.printf("[TimeManager] Timezone set to GMT%+d\n", _timezoneOffset);
}

bool TimeManager::isSynced() const {
    return _synced;
}
