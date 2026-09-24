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
    // Blob phai dung count * sizeof(AlarmItem). Lech nghia la blob ghi boi ban
    // firmware co AlarmItem khac kich thuoc (id[16] cu) -> doc vao se lech truong.
    // Tra 0: AlarmClock coi nhu chua co bao thuc, lan sync dau se tai lai tu cloud.
    if (_prefs.getBytesLength(KEY_ALARM_DATA) != count * sizeof(AlarmItem)) {
        // Hạ cờ dirty cùng lúc: blob đã bỏ mà cờ còn bật thì lần sync đầu "hộp thắng",
        // đẩy danh sách RỖNG lên đè mất toàn bộ báo thức trên cloud.
        _prefs.putBool(KEY_ALARM_DIRTY, false);
        _prefs.putUInt(KEY_ALARM_COUNT, 0);
        return 0;
    }
    size_t toRead = (count < maxCount) ? count : maxCount;
    size_t readBytes = _prefs.getBytes(KEY_ALARM_DATA, alarms, toRead * sizeof(AlarmItem));
    return readBytes / sizeof(AlarmItem);
}

bool ConfigManager::saveAlarmDirty(bool dirty) {
    return _prefs.putBool(KEY_ALARM_DIRTY, dirty) > 0;
}

bool ConfigManager::loadAlarmDirty() {
    return _prefs.getBool(KEY_ALARM_DIRTY, false);
}

void ConfigManager::loadSettings(UserSettings& out) {
    out.brightness = _prefs.getUChar(KEY_SET_BL, SETTINGS_DEFAULT_BRIGHTNESS);
    out.volume = _prefs.getUChar(KEY_SET_VOL, SETTINGS_DEFAULT_VOLUME);
    out.rev = _prefs.getUInt(KEY_SET_REV, 0);
}

bool ConfigManager::saveSettings(const UserSettings& s) {
    bool ok = _prefs.putUChar(KEY_SET_BL, s.brightness) > 0;
    ok = (_prefs.putUChar(KEY_SET_VOL, s.volume) > 0) && ok;
    // rev ghi SAU CÙNG: mất điện giữa chừng thì rev cũ còn đó -> lần sync sau đọc lại.
    ok = (_prefs.putUInt(KEY_SET_REV, s.rev) > 0) && ok;
    return ok;
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
