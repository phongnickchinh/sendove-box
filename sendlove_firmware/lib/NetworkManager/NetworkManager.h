#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include "config.h"

/// Wi-Fi connection result status
enum class WiFiConnectResult : uint8_t {
    CONNECTED,
    FAILED,
    NO_CREDENTIALS,
    TIMEOUT
};

/// Wi-Fi, NTP time sync and OTA web server manager
class NetworkManager {
public:
    NetworkManager() = default;

    /// Initialize Wi-Fi STA mode
    void init();

    /// Connect to Wi-Fi network with specified credentials
    WiFiConnectResult connectWiFi(const char* ssid, const char* password);

    /// Disconnect Wi-Fi and power down RF
    void disconnectWiFi();

    /// Check if Wi-Fi connected and time is synchronized
    bool isReady() const;

    /// Check if Wi-Fi is connected
    bool isConnected() const;

    /// Force RF reconnect if Wi-Fi is disconnected (with fallback & timeout)
    bool ensureConnected(uint32_t timeoutMs = 5000);

    /// Tell the manager the chip just woke from light sleep. The next
    /// ensureConnected() will FORCE a re-association instead of trusting
    /// WiFi.status() — after sleep it usually still says WL_CONNECTED although the
    /// association died on the AP side (the CPU was asleep, so the driver couldn't
    /// process beacon-loss/deauth events). See ensureConnected().
    void notifyWakeFromSleep() { _forceReassociate = true; }

    /// Get current formatted time string ("14:30")
    void getTimeString(char* buffer, size_t maxLen) const;

    /// Get Wi-Fi RSSI signal strength
    int getWifiRSSI() const;

    /// Synchronize NTP time blocking within timeoutMs (called by background task)
    bool syncNtpTime(uint32_t timeoutMs = 5000);

    /// Check if time has been synchronized at least once
    bool isTimeSynced() const;

    /// Check if NTP sync is currently running in background
    bool isNtpSyncing() const { return _isNtpSyncing; }

    /// Periodic update task for web server only (NTP logic moved to background task)
    void update();

    /// Start OTA WebServer on port 80 and mDNS
    void startWebServer(const char* hostname = "sendlovebox");

    /// Stop WebServer and free resources
    void stopWebServer();

    /// Get pointer to WebServer instance
    WebServer* getWebServer();

    /// Check if WebServer is running
    bool isWebServerRunning() const;

    /// Start SoftAP Captive Portal for Wi-Fi provisioning
    void startProvisioningAP(const char* apSsid = "SendloveBox-Setup", const char* apPassword = "");

    /// Check if Wi-Fi provisioning complete
    bool isProvisioningDone() const;

    /// Check if Wi-Fi provisioning portal is currently active
    bool isProvisioningActive() const;

    /// Check if currently downloading media in progress
    bool isDownloadingMedia() const { return _isDownloadingMedia; }

    /// Check if Firebase sync is currently running in background
    bool isFirebaseSyncing() const { return _isFirebaseSyncing || _isSyncing; }

    /// Check if any network background sync (NTP, Firebase, Download) is in progress
    bool isSyncing() const { return _isSyncing || _isFirebaseSyncing || _isNtpSyncing || _isDownloadingMedia; }

    /// Total number of new messages (unread + waiting in the cloud)
    uint32_t getNumOfNewMsg() const { return _numOfNewMsg; }
    void setNumOfNewMsg(uint32_t num) { _numOfNewMsg = num; }
    void decrementNewMsgCount() { if (_numOfNewMsg > 0) _numOfNewMsg--; }

    /// Check if there are pending messages on the server (aborted due to full storage)
    bool hasPendingMessages() const { return _hasPendingMessages; }

    /// Full background sync on wake (Wi-Fi + NTP time + Firebase status, flags, messages, alarms)
    bool syncWakeup(uint8_t batteryPercent, bool isCharging, class IStorageProvider* storage = nullptr);

    /// Start the wake-up sync on the background task (doesn't block the UI task)
    void triggerWakeupSync(uint8_t batteryPercent, bool isCharging, class IStorageProvider* storage = nullptr);

    /// Background Firebase sync (backward compatible wrapper)
    bool syncFirebaseWakeup(uint8_t batteryPercent, bool isCharging, class IStorageProvider* storage = nullptr) {
        return syncWakeup(batteryPercent, isCharging, storage);
    }

    /// Start a background Firebase sync (backward compatible wrapper)
    void triggerFirebaseSync(uint8_t batteryPercent, bool isCharging, class IStorageProvider* storage = nullptr) {
        triggerWakeupSync(batteryPercent, isCharging, storage);
    }

    /// Set the callback fired when a media download completes
    void setOnDownloadComplete(std::function<void()> cb) { _onDownloadComplete = cb; }

    /// Set the callback that reports whether playback is active (to defer background sync and avoid CPU/SPI contention)
    void setPlaybackActiveCallback(std::function<bool()> cb) { _isPlaybackActiveCb = cb; }
    bool isPlaybackActive() const { return _isPlaybackActiveCb ? _isPlaybackActiveCb() : false; }

    /// Record the wake reason ("timer", "touch", "boot") for telemetry
    void setWakeupCause(const char* cause) {
        if (cause) {
            strncpy(_currentWakeCause, cause, sizeof(_currentWakeCause) - 1);
            _currentWakeCause[sizeof(_currentWakeCause) - 1] = '\0';
        }
    }

private:
    char _wifiSsid[WIFI_SSID_MAX_LEN] = "";
    char _wifiPassword[WIFI_PASS_MAX_LEN] = "";
    std::function<bool()> _isPlaybackActiveCb = nullptr;

    bool updateFirebaseStatus(uint8_t batteryPercent, bool isCharging);
    bool checkFirebaseFlags();
    bool syncFirebaseAlarms();
    bool pushFirebaseAlarms();
    /// true until alarm_list is first downloaded after boot (or after a failed download)
    bool _alarmsNeedFetch = true;
    /// User settings (brightness, volume): GET config (key range config_rev..playback_volume)
    /// when config_flag is set, or on the first sync after boot (the web may have changed
    /// them while the box was off). Retried on failure.
    bool syncFirebaseSettings();
    bool _settingsNeedFetch = true;
    /// PATCH the set flags back to false, ONE request for all flags (each request = 1 TLS handshake).
    void resetFlags(bool alarm, bool config, bool theme, bool music);
    bool checkAndDownloadNewMessages(class IStorageProvider* storage);

    /// Download a Firebase Storage file to the card (theme, alarm music). Writes to
    /// dst.part; a .part left from a previous attempt is resumed with
    /// `Range: bytes=N-` (MEMORY.md §29, proposal #3). Once `size` bytes are in,
    /// crc32 is checked and only then is it renamed to dst. Messages do NOT take
    /// this path (slots are written through SDStorageProvider, with no .part to resume).
    enum class DlResult : uint8_t { OK, ABORTED, FAILED };
    DlResult downloadFile(const char* storagePath, const char* dstPath, uint32_t size, uint32_t crc);

    /// Push the tail of the log to status/log_tail when there is a new error
    /// (MEMORY.md §29, proposal #2).
    void pushLogTail();

    /// Alarm music: download tracks that alarms use and the card lacks (at most
    /// ALARM_MUSIC_PER_SYNC per cycle, soonest alarm first) and delete tracks removed
    /// from the library. The music list is fetched only when needed: flag set, a
    /// track missing, first time. false = interrupted because an alarm/message
    /// started playing (retried next cycle).
    bool syncAlarmMusic();
    bool _musicNeedFetch = true;      // first time after boot / after a card remount / flag set
    uint32_t _seenMountEpoch = 0;     // the SdStore::mountEpoch already handled

    /// Theme: GET config/theme when the flag is set / first time / the card was just
    /// remounted; if rev differs from the copy in flash, download the package to
    /// /theme/t_<id>_r<rev>/ and ask Task_MediaPlayer to install it (ThemeStore).
    /// theme_flag is cleared only once the copy IN FLASH has the right rev (not when
    /// the download finishes). false = interrupted because an alarm/message started playing.
    bool syncTheme(bool themeFlag);
    bool _themeNeedFetch = true;
    uint32_t _seenThemeEpoch = 0;
    bool _themeFlag = false;          // theme_flag as read in checkFirebaseFlags(); cleared in syncTheme()
    bool downloadVoiceSegment(const String& rawVoiceUrl, class WiFiClientSecure& client,
                               class IStorageProvider* storage, const char* writeSlotId);

    static void wakeupSyncTaskWorker(void* param);

    /// The resident sync task: created once, then sleeps waiting for a notify.
    /// Creating/deleting a task every cycle would keep allocating and freeing 12KB
    /// of contiguous memory -> heap fragmentation (MEMORY.md §21).
    TaskHandle_t _syncTask = nullptr;
    uint8_t _syncBattery = 0;
    bool _syncCharging = false;
    class IStorageProvider* _syncStorage = nullptr;

    // --- Firebase Auth: the box's own idToken instead of the Database Secret ---
    // Holds the RAW JWT, no prefix: RTDB accepts NO auth header at all — only
    // `?auth=` (verified, see MEMORY.md §17). Inside the #if so the legacy mode
    // doesn't carry 1.4KB of BSS for nothing.
#if FIREBASE_USE_IDTOKEN
    char   _idToken[FIREBASE_ID_TOKEN_MAX_LEN] = "";
    time_t _idTokenExpiry = 0;
#endif

    /// URL build buffer SHARED by every Firebase request. A member, not a local,
    /// because the ~945-byte idToken has to go in the URL. Safe: all these calls
    /// run sequentially on the one network task, and HTTPClient::begin() copies
    /// the URL into its own String, so overwriting it afterwards is harmless.
    char _url[FIREBASE_URL_MAX_LEN] = "";

    /// Ensure a valid idToken. Prefers the refresh token in NVS (no password
    /// resent); signs in with the password only when there is none or the refresh fails.
    bool ensureIdToken(bool force = false);
    bool authWithPassword();
    bool authWithRefreshToken(const char* refreshToken);

    /// Append the auth parameter to `_url`: `?auth=<secret>` in legacy mode,
    /// `?auth=<idToken>` in the new mode. `sep` is '?' or '&' depending on whether
    /// the URL already has a query. There is NO header variant for RTDB — it
    /// rejects both `Bearer` and `Firebase` (MEMORY.md §17).
    void appendAuth(char sep);

    /// Firebase Storage is the OPPOSITE: it takes the header `Authorization:
    /// Firebase <idToken>`. Different service, different scheme. No-op while the
    /// Database Secret is in use.
    void addStorageAuthHeader(class HTTPClient& http);

    /// Call after every request: a 401 means the token is dead -> force a refresh next cycle
    void noteAuthFailure(int httpCode, const char* where);

    volatile bool _forceReassociate = false;
    volatile bool _isSyncing = false;
    volatile bool _isFirebaseSyncing = false;
    volatile bool _isNtpSyncing = false;
    volatile bool _hasPendingMessages = false;
    volatile uint32_t _numOfNewMsg = 0;
    volatile bool _isDownloadingMedia = false;
    std::function<void()> _onDownloadComplete = nullptr;

    bool _isTimeSynced = false;
    uint32_t _lastTimeSync = 0;

    // Telemetry & diagnostics for timer/touch wakeup
    char _currentWakeCause[12] = "boot";
    char _prevWakeCause[12] = "none";
    char _diagStep[24] = "boot";
    char _diagErr[24] = "none";
    char _prevDiagStep[24] = "none";
    char _prevDiagErr[24] = "none";
    uint32_t _lastWifiMs = 0;
    bool _lastAFlag = false;
    bool _lastAlarmsDirty = false;
    uint8_t _lastAlarmsCount = 0;
    int _lastFlagsHttp = 0;

    WebServer* _webServer = nullptr;
    bool _webServerRunning = false;

    WebServer* _captiveServer = nullptr;
    bool _provisioningDone = false;
    String _provisionedSsid;
    String _provisionedPass;

    void handleCaptiveRoot();
    void handleCaptiveSubmit();
    /// Returns the nearby Wi-Fi networks as JSON. The scan is asynchronous so it
    /// doesn't block the web server: the first call returns {"status":"scanning"}
    /// and the client polls again.
    void handleCaptiveScan();
    /// The URLs operating systems probe to check for internet access. Answer 302
    /// to the portal page so the device pops up its sign-in window.
    void handleCaptiveProbe();
    /// Alarms on the portal (AP mode, no internet). An edit here sets AlarmClock's
    /// dirty flag -> the first sync once online pushes over the cloud copy.
    void handleAlarmList();    // GET  /alarms
    void handleAlarmSave();    // POST /alarms/save   id?, time, en, rep
    void handleAlarmDelete();  // POST /alarms/delete id
    /// POST /time epoch — takes the phone's time when the box has never had NTP
    /// (AP mode right after power-up); without it alarms could never ring.
    void handleSetTime();
    String buildCaptivePortalHTML();
};

#endif // NETWORK_MANAGER_H
