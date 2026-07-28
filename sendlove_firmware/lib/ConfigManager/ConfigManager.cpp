#include "ConfigManager.h"

// ============================================================================
// ConfigManager Implementation
// ============================================================================

bool ConfigManager::init(const char* namespaceName) {
    bool ok = _prefs.begin(namespaceName, false); // false = read-write
    if (!ok) {
        Serial.println(F("[ConfigManager] ERROR: Failed to open NVS namespace"));
    } else {
        Serial.println(F("[ConfigManager] NVS initialized"));
    }
    return ok;
}

void ConfigManager::end() {
    _prefs.end();
}

// --- Wi-Fi Credentials ---

bool ConfigManager::saveWiFi(const char* ssid, const char* password) {
    size_t written = 0;
    written += _prefs.putString(KEY_WIFI_SSID, ssid);
    written += _prefs.putString(KEY_WIFI_PASS, password);

    if (written > 0) {
        Serial.printf("[ConfigManager] Wi-Fi saved: SSID=%s\n", ssid);
        return true;
    }
    Serial.println(F("[ConfigManager] ERROR: Failed to save Wi-Fi"));
    return false;
}

bool ConfigManager::loadWiFi(char* ssid, char* password) {
    String s = _prefs.getString(KEY_WIFI_SSID, "");
    String p = _prefs.getString(KEY_WIFI_PASS, "");

    if (s.isEmpty()) {
        Serial.println(F("[ConfigManager] No Wi-Fi config found in NVS"));
        return false;
    }

    strncpy(ssid, s.c_str(), WIFI_SSID_MAX_LEN - 1);
    ssid[WIFI_SSID_MAX_LEN - 1] = '\0';

    strncpy(password, p.c_str(), WIFI_PASS_MAX_LEN - 1);
    password[WIFI_PASS_MAX_LEN - 1] = '\0';

    Serial.printf("[ConfigManager] Wi-Fi loaded: SSID=%s\n", ssid);
    return true;
}

bool ConfigManager::hasWiFiConfig() {
    String ssid = _prefs.getString(KEY_WIFI_SSID, "");
    return !ssid.isEmpty();
}

// --- Wi-Fi Backup (Rollback) ---

bool ConfigManager::saveBackupWiFi(const char* ssid, const char* password) {
    size_t written = 0;
    written += _prefs.putString(KEY_WIFI_SSID_BAK, ssid);
    written += _prefs.putString(KEY_WIFI_PASS_BAK, password);

    if (written > 0) {
        Serial.printf("[ConfigManager] Backup Wi-Fi saved: SSID=%s\n", ssid);
        return true;
    }
    return false;
}

bool ConfigManager::loadBackupWiFi(char* ssid, char* password) {
    String s = _prefs.getString(KEY_WIFI_SSID_BAK, "");
    String p = _prefs.getString(KEY_WIFI_PASS_BAK, "");

    if (s.isEmpty()) {
        Serial.println(F("[ConfigManager] No backup Wi-Fi config found"));
        return false;
    }

    strncpy(ssid, s.c_str(), WIFI_SSID_MAX_LEN - 1);
    ssid[WIFI_SSID_MAX_LEN - 1] = '\0';

    strncpy(password, p.c_str(), WIFI_PASS_MAX_LEN - 1);
    password[WIFI_PASS_MAX_LEN - 1] = '\0';

    Serial.printf("[ConfigManager] Backup Wi-Fi loaded: SSID=%s\n", ssid);
    return true;
}

bool ConfigManager::clearAll() {
    bool ok = _prefs.clear();
    if (ok) {
        Serial.println(F("[ConfigManager] All config cleared (factory reset)"));
    }
    return ok;
}

// --- Firebase Sync & Alarms ---

bool ConfigManager::saveLastDownloadTimestamp(uint64_t ts) {
    size_t bytes = _prefs.putBytes(KEY_LAST_DL_TS, &ts, sizeof(uint64_t));
    return (bytes == sizeof(uint64_t));
}

uint64_t ConfigManager::loadLastDownloadTimestamp() {
    uint64_t ts = 0;
    _prefs.getBytes(KEY_LAST_DL_TS, &ts, sizeof(uint64_t));
    return ts;
}

bool ConfigManager::saveAlarms(const AlarmItem* alarms, size_t count) {
    if (!alarms && count > 0) return false;
    _prefs.putUInt(KEY_ALARM_COUNT, (uint32_t)count);
    if (count > 0) {
        size_t written = _prefs.putBytes(KEY_ALARM_DATA, alarms, count * sizeof(AlarmItem));
        return (written == count * sizeof(AlarmItem));
    }
    return true;
}

size_t ConfigManager::loadAlarms(AlarmItem* alarms, size_t maxCount) {
    if (!alarms || maxCount == 0) return 0;
    uint32_t count = _prefs.getUInt(KEY_ALARM_COUNT, 0);
    if (count == 0) return 0;
    size_t toRead = (count < maxCount) ? count : maxCount;
    size_t readBytes = _prefs.getBytes(KEY_ALARM_DATA, alarms, toRead * sizeof(AlarmItem));
    return readBytes / sizeof(AlarmItem);
}

uint32_t ConfigManager::getSecondsToNextAlarm(time_t currentEpochTime) {
    if (currentEpochTime <= 0) return 0xFFFFFFFF;

    struct tm timeinfo;
    if (localtime_r(&currentEpochTime, &timeinfo) == nullptr) return 0xFFFFFFFF;

    uint32_t currentSecOfDay = timeinfo.tm_hour * 3600 + timeinfo.tm_min * 60 + timeinfo.tm_sec;

    AlarmItem alarms[10];
    size_t count = loadAlarms(alarms, 10);
    if (count == 0) return 0xFFFFFFFF;

    uint32_t minSecRemaining = 0xFFFFFFFF;

    for (size_t i = 0; i < count; i++) {
        if (!alarms[i].isEnable) continue;

        int aHour = 0, aMin = 0;
        if (sscanf(alarms[i].time, "%d:%d", &aHour, &aMin) != 2) continue;

        uint32_t alarmSecOfDay = aHour * 3600 + aMin * 60;
        int32_t diff = (int32_t)alarmSecOfDay - (int32_t)currentSecOfDay;

        if (diff <= 0) {
            // Đã qua mốc giờ trong ngày
            if (alarms[i].repeatable) {
                diff += 86400; // Sang ngày hôm sau
            } else {
                continue; // One-shot đã qua
            }
        }

        if ((uint32_t)diff < minSecRemaining) {
            minSecRemaining = (uint32_t)diff;
        }
    }

    return minSecRemaining;
}
