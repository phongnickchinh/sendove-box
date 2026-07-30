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

    /// Force RF reconnect if Wi-Fi is disconnected
    void ensureConnected();

    /// Get current formatted time string ("14:30")
    void getTimeString(char* buffer, size_t maxLen) const;

    /// Get current formatted date string
    void getDateString(char* buffer, size_t maxLen) const;

    /// Get Wi-Fi RSSI signal strength
    int getWifiRSSI() const;

    /// Trigger non-blocking NTP time sync in background task
    void triggerNtpSync();

    /// Check if time has been synchronized at least once
    bool isTimeSynced() const;

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
    bool isFirebaseSyncing() const { return _isFirebaseSyncing; }

    /// Đồng bộ dữ liệu Firebase ngầm khi thức dậy (Status, Flags, Messages, Alarms)
    bool syncFirebaseWakeup(uint8_t batteryPercent, bool isCharging, class IStorageProvider* storage = nullptr);

    /// Set callback khi tải media hoàn tất
    void setOnDownloadComplete(std::function<void()> cb) { _onDownloadComplete = cb; }

    /// Kích hoạt Firebase Sync ngầm trên background task (không làm block UI Task)
    void triggerFirebaseSync(uint8_t batteryPercent, bool isCharging, class IStorageProvider* storage = nullptr);

private:
    bool updateFirebaseStatus(uint8_t batteryPercent, bool isCharging);
    bool checkFirebaseFlags();
    bool syncFirebaseAlarms();
    bool checkAndDownloadNewMessages(class IStorageProvider* storage);

    static void firebaseSyncTaskWorker(void* param);
    volatile bool _isFirebaseSyncing = false;
    volatile bool _isDownloadingMedia = false;
    std::function<void()> _onDownloadComplete = nullptr;

    bool _isTimeSynced = false;
    volatile bool _isNtpSyncing = false;
    uint32_t _lastTimeSync = 0;

    static void ntpTaskWorker(void* param);

    WebServer* _webServer = nullptr;
    bool _webServerRunning = false;

    WebServer* _captiveServer = nullptr;
    bool _provisioningDone = false;
    String _provisionedSsid;
    String _provisionedPass;

    void handleCaptiveRoot();
    void handleCaptiveSubmit();
    String buildCaptivePortalHTML();
};

#endif // NETWORK_MANAGER_H
