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
