#include "ConfigManager.h"
#include "ScreenLogger.h"

// ============================================================================
// ConfigManager Implementation
// ============================================================================

bool ConfigManager::init(const char* namespaceName) {
    bool ok = _prefs.begin(namespaceName, false); // false = read-write
    if (!ok) {
        DLOG("[CFG] ERR: NVS open");
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
        // Dropped Wi-Fi saved
        return true;
    }
    DLOG("[CFG] ERR: Wi-Fi save");
    return false;
}

bool ConfigManager::loadWiFi(char* ssid, char* password) {
    String s = _prefs.getString(KEY_WIFI_SSID, "");
    String p = _prefs.getString(KEY_WIFI_PASS, "");

    if (s.isEmpty()) {
        DLOG("[CFG] no Wi-Fi config");
        return false;
    }

    strncpy(ssid, s.c_str(), WIFI_SSID_MAX_LEN - 1);
    ssid[WIFI_SSID_MAX_LEN - 1] = '\0';

    strncpy(password, p.c_str(), WIFI_PASS_MAX_LEN - 1);
    password[WIFI_PASS_MAX_LEN - 1] = '\0';

    // Dropped Wi-Fi loaded
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
        // Dropped Backup Wi-Fi saved
        return true;
    }
    return false;
}

bool ConfigManager::loadBackupWiFi(char* ssid, char* password) {
    String s = _prefs.getString(KEY_WIFI_SSID_BAK, "");
    String p = _prefs.getString(KEY_WIFI_PASS_BAK, "");

    if (s.isEmpty()) {
        // Dropped No backup Wi-Fi config found
        return false;
    }

    strncpy(ssid, s.c_str(), WIFI_SSID_MAX_LEN - 1);
    ssid[WIFI_SSID_MAX_LEN - 1] = '\0';

    strncpy(password, p.c_str(), WIFI_PASS_MAX_LEN - 1);
    password[WIFI_PASS_MAX_LEN - 1] = '\0';

    // Dropped Backup Wi-Fi loaded
    return true;
}

bool ConfigManager::clearAll() {
    bool ok = _prefs.clear();
    if (ok) {
        DLOG("[CFG] config cleared");
    }
    return ok;
}

// --- Firebase Sync & Alarms ---

bool ConfigManager::saveLastDownloadTimestamp(uint64_t ts) {
    size_t written = _prefs.putULong64(KEY_LAST_DL_TS, ts);
    // Dropped saveLastDownloadTimestamp
    return (written > 0);
}

uint64_t ConfigManager::loadLastDownloadTimestamp() {
    uint64_t ts = _prefs.getULong64(KEY_LAST_DL_TS, 0ULL);
    // Dropped loadLastDownloadTimestamp
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

bool ConfigManager::saveRefreshToken(const char* token) {
    if (token == nullptr || token[0] == '\0') return false;
    if (_prefs.putString(KEY_FB_REFRESH, token) > 0) return true;
    DLOG("[CFG] ERR: refresh token save");
    return false;
}

bool ConfigManager::loadRefreshToken(char* outToken, size_t maxLen) {
    if (outToken == nullptr || maxLen == 0) return false;
    String t = _prefs.getString(KEY_FB_REFRESH, "");
    if (t.isEmpty()) return false;

    strncpy(outToken, t.c_str(), maxLen - 1);
    outToken[maxLen - 1] = '\0';
    return true;
}

void ConfigManager::clearRefreshToken() {
    _prefs.remove(KEY_FB_REFRESH);
}
