#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>
#include "config.h"

// ============================================================================
// ConfigManager — configuration stored in NVS (Non-Volatile Storage)
// ============================================================================
// Holds what must survive a power loss: Wi-Fi credentials (+ a backup for
// rollback), the sync timestamp, alarms, user settings and the auth token.
// ============================================================================

struct AlarmItem {
    // 24 chars, not 16: the backend generates ids "alarm_<ms>" = 19 chars. A
    // 16-char buffer cut the tail off -> pushing it back to the cloud created a
    // DIFFERENT key. Changing the size makes an old NVS blob no longer match ->
    // loadAlarms() discards it and the first sync downloads the list again.
    char id[24] = "";
    char time[6] = "00:00"; // "HH:MM"
    bool isEnable = false;
    bool repeatable = false;
    // Alarm music. Empty = beep. Changing the struct size -> an old NVS blob has the
    // wrong size -> loadAlarms() drops the blob AND clears the dirty flag (otherwise
    // the first sync would push an empty list and wipe the alarms in the cloud).
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
    /// Open the NVS namespace
    /// @param namespaceName NVS namespace (e.g. "sendlove")
    /// @return true on success
    bool init(const char* namespaceName);

    /// Close the NVS handle
    void end();

    // --- Wi-Fi Credentials ---

    /// Save Wi-Fi credentials to NVS
    /// @return true if written
    bool saveWiFi(const char* ssid, const char* password);

    /// Load Wi-Fi credentials from NVS
    /// @param ssid buffer for the SSID (at least WIFI_SSID_MAX_LEN bytes)
    /// @param password buffer for the password (at least WIFI_PASS_MAX_LEN bytes)
    /// @return true if loaded
    bool loadWiFi(char* ssid, char* password);

    /// Whether NVS holds Wi-Fi credentials
    /// @return true if an SSID is stored
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

    /// The "alarm list was edited on the box (portal / a one-shot alarm turning
    /// itself off) and not yet pushed to the cloud" flag. Kept in NVS so it
    /// survives the reboot that follows saving Wi-Fi on the portal.
    bool saveAlarmDirty(bool dirty);
    bool loadAlarmDirty();

    // --- User settings (brightness, volume) ---

    /// If never saved, returns the config.h defaults — not an error.
    void loadSettings(UserSettings& out);
    bool saveSettings(const UserSettings& s);

    // --- Firebase Auth ---

    /// Save the Firebase Auth refresh token (exchanged for a new idToken without
    /// resending the password). It doesn't expire with time.
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
