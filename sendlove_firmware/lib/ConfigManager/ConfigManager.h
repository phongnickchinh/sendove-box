#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>
#include "config.h"

// ConfigManager — NVS storage for what must survive a power loss: Wi-Fi
// credentials, the sync timestamp, alarms, user settings and the auth token.

struct AlarmItem {
    // 24 chars: backend ids "alarm_<ms>" are 19 chars; a truncated id would be
    // pushed back as a different key.
    char id[24] = "";
    char time[6] = "00:00"; // "HH:MM"
    bool isEnable = false;
    bool repeatable = false;
    // Alarm music; empty = beep. Changing this struct's size makes loadAlarms()
    // drop the old NVS blob AND clear the dirty flag (or an empty list would be pushed).
    char musicId[24] = "";
    uint8_t volume = 80;   // 0..100, per alarm (product decision: default 80)
    bool ramp = true;      // fade in from 30% of the chosen level over ALARM_RAMP_MS
};

/// User settings made on the web (boxes/<id>/config). rev = the config_rev last applied.
struct UserSettings {
    uint8_t brightness = SETTINGS_DEFAULT_BRIGHTNESS;
    uint8_t volume = SETTINGS_DEFAULT_VOLUME;
    uint32_t rev = 0;
};

class ConfigManager {
public:
    /// Open the NVS namespace.
    bool init(const char* namespaceName);

    /// Close the NVS handle
    void end();

    // --- Wi-Fi Credentials ---

    /// Save Wi-Fi credentials.
    bool saveWiFi(const char* ssid, const char* password);

    /// Load Wi-Fi credentials (buffers of WIFI_SSID_MAX_LEN / WIFI_PASS_MAX_LEN).
    bool loadWiFi(char* ssid, char* password);

    /// Whether an SSID is stored.
    bool hasWiFiConfig();

    // --- Wi-Fi Backup (Rollback) ---

    /// Save the current Wi-Fi as a backup before switching to a new one
    bool saveBackupWiFi(const char* ssid, const char* password);

    /// Load the Wi-Fi backup
    bool loadBackupWiFi(char* ssid, char* password);

    /// Erase all config (factory reset)
    bool clearAll();

    // --- Firebase Sync & Alarms ---

    /// Save the timestamp (ms) of the last downloaded message
    bool saveLastDownloadTimestamp(uint64_t ts);

    /// Load the timestamp (ms) of the last downloaded message
    uint64_t loadLastDownloadTimestamp();

    /// Save the alarm list
    bool saveAlarms(const AlarmItem* alarms, size_t count);

    /// Load the alarm list
    size_t loadAlarms(AlarmItem* alarms, size_t maxCount);

    /// "Alarms edited on the box, not yet pushed" flag. In NVS so it survives the
    /// reboot after saving Wi-Fi on the portal.
    bool saveAlarmDirty(bool dirty);
    bool loadAlarmDirty();

    // --- User settings (brightness, volume) ---

    /// If never saved, returns the config.h defaults — not an error.
    void loadSettings(UserSettings& out);
    bool saveSettings(const UserSettings& s);

    // --- Firebase Auth ---

    /// Save the Firebase Auth refresh token.
    bool saveRefreshToken(const char* token);

    /// Load the refresh token. Returns false if never saved.
    bool loadRefreshToken(char* outToken, size_t maxLen);

    /// Delete the refresh token (when Firebase reports it revoked/invalid)
    void clearRefreshToken();

private:
    Preferences _prefs;

    // NVS keys
    static constexpr const char* KEY_WIFI_SSID     = "wifi_ssid";
    static constexpr const char* KEY_WIFI_PASS     = "wifi_pass";
    static constexpr const char* KEY_WIFI_SSID_BAK = "wifi_ssid_bak";
    static constexpr const char* KEY_WIFI_PASS_BAK = "wifi_pass_bak";
    static constexpr const char* KEY_LAST_DL_TS    = "last_dl_ts";
    static constexpr const char* KEY_ALARM_COUNT   = "alarm_cnt";
    static constexpr const char* KEY_ALARM_DATA    = "alarm_data";
    static constexpr const char* KEY_ALARM_DIRTY   = "alarm_dirty";
    static constexpr const char* KEY_FB_REFRESH    = "fb_refresh";
    static constexpr const char* KEY_SET_BL        = "set_bl";
    static constexpr const char* KEY_SET_VOL       = "set_vol";
    static constexpr const char* KEY_SET_REV       = "set_rev";
};

#endif // CONFIG_MANAGER_H
