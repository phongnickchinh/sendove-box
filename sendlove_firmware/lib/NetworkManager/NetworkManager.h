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

    /// Cờ OTA (`emergency_ota`/`normal_ota` trên cloud) đã bật chưa. Đọc là xoá —
    /// main.cpp dùng nó để mở cửa sổ OTA rồi tự đóng.
    bool takeOtaRequest() {
        if (!_otaRequested) return false;
        _otaRequested = false;
        return true;
    }

    /// Check if Wi-Fi is connected
    bool isConnected() const;

    /// Force RF reconnect if Wi-Fi is disconnected (with fallback & timeout)
    bool ensureConnected(uint32_t timeoutMs = 5000);

    /// Báo chip vừa tỉnh khỏi Light Sleep. Lần ensureConnected() kế tiếp sẽ ÉP tái
    /// lập association thay vì tin WiFi.status() — sau khi ngủ, biến trạng thái này
    /// thường vẫn là WL_CONNECTED dù association đã chết ở phía AP (CPU ngủ nên
    /// driver không xử lý được beacon-loss/deauth event). Xem ensureConnected().
    void notifyWakeFromSleep() { _forceReassociate = true; }

    /// Get current formatted time string ("14:30")
    void getTimeString(char* buffer, size_t maxLen) const;

    /// Get current formatted date string
    void getDateString(char* buffer, size_t maxLen) const;

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

    /// Trả về tổng số tin nhắn mới (chưa đọc + đang chờ trên mây)
    uint32_t getNumOfNewMsg() const { return _numOfNewMsg; }
    void setNumOfNewMsg(uint32_t num) { _numOfNewMsg = num; }
    void decrementNewMsgCount() { if (_numOfNewMsg > 0) _numOfNewMsg--; }

    /// Check if there are pending messages on the server (aborted due to full storage)
    bool hasPendingMessages() const { return _hasPendingMessages; }

    /// Đồng bộ toàn diện ngầm khi thức dậy (Wi-Fi ensure + NTP Time + Firebase Status, Flags, Messages, Alarms)
    bool syncWakeup(uint8_t batteryPercent, bool isCharging, class IStorageProvider* storage = nullptr);

    /// Kích hoạt Wakeup Sync ngầm trên background task (không làm block UI Task)
    void triggerWakeupSync(uint8_t batteryPercent, bool isCharging, class IStorageProvider* storage = nullptr);

    /// Đồng bộ dữ liệu Firebase ngầm (backward compatible wrapper)
    bool syncFirebaseWakeup(uint8_t batteryPercent, bool isCharging, class IStorageProvider* storage = nullptr) {
        return syncWakeup(batteryPercent, isCharging, storage);
    }

    /// Kích hoạt Firebase Sync ngầm (backward compatible wrapper)
    void triggerFirebaseSync(uint8_t batteryPercent, bool isCharging, class IStorageProvider* storage = nullptr) {
        triggerWakeupSync(batteryPercent, isCharging, storage);
    }

    /// Set callback khi tải media hoàn tất
    void setOnDownloadComplete(std::function<void()> cb) { _onDownloadComplete = cb; }

    /// Set callback để kiểm tra trạng thái đang phát video (để hoãn sync ngầm tránh nghẽn CPU/SPI)
    void setPlaybackActiveCallback(std::function<bool()> cb) { _isPlaybackActiveCb = cb; }
    bool isPlaybackActive() const { return _isPlaybackActiveCb ? _isPlaybackActiveCb() : false; }

private:
    char _wifiSsid[WIFI_SSID_MAX_LEN] = "";
    char _wifiPassword[WIFI_PASS_MAX_LEN] = "";
    std::function<bool()> _isPlaybackActiveCb = nullptr;

    bool updateFirebaseStatus(uint8_t batteryPercent, bool isCharging);
    bool checkFirebaseFlags();
    bool syncFirebaseAlarms();
    bool pushFirebaseAlarms();
    /// true tới khi tải được alarm_list lần đầu sau boot (hoặc lần tải trước hỏng)
    bool _alarmsNeedFetch = true;
    bool checkAndDownloadNewMessages(class IStorageProvider* storage);
    bool downloadVoiceSegment(const String& rawVoiceUrl, class WiFiClientSecure& client,
                               class IStorageProvider* storage, const char* writeSlotId);

    static void wakeupSyncTaskWorker(void* param);

    /// Task sync thường trú: tạo một lần, sau đó ngủ chờ notify. Tạo/xoá task
    /// mỗi chu kỳ (cách cũ) là xin-trả 12KB liền mạch liên tục -> vụn heap (§21).
    TaskHandle_t _syncTask = nullptr;
    uint8_t _syncBattery = 0;
    bool _syncCharging = false;
    class IStorageProvider* _syncStorage = nullptr;

    // --- Firebase Auth: idToken riêng của box thay cho Database Secret ---
    // Giữ JWT THÔ, không kèm tiền tố. Trước đây giữ dạng "Bearer <jwt>" cho
    // addHeader(), nhưng RTDB KHÔNG nhận header nào cả — chỉ nhận `?auth=`
    // (đo thật 2026-09-05, xem MEMORY.md §17). Đặt trong #if để chế độ cũ
    // không phải gánh 1.4KB BSS vô ích.
#if FIREBASE_USE_IDTOKEN
    char   _idToken[FIREBASE_ID_TOKEN_MAX_LEN] = "";
    time_t _idTokenExpiry = 0;
#endif

    /// Buffer dựng URL, dùng CHUNG cho mọi request Firebase. Là thành viên chứ
    /// không phải biến cục bộ vì idToken ~945 byte phải nằm trong URL.
    /// An toàn: mọi lời gọi này chạy tuần tự trong cùng network task, và
    /// HTTPClient::begin() copy URL vào String riêng nên ghi đè sau đó vô hại.
    char _url[FIREBASE_URL_MAX_LEN] = "";

    /// Đảm bảo có idToken còn hạn. Ưu tiên refresh token trong NVS (không phải
    /// gửi lại mật khẩu); chỉ đăng nhập bằng mật khẩu khi chưa có/refresh hỏng.
    bool ensureIdToken(bool force = false);
    bool authWithPassword();
    bool authWithRefreshToken(const char* refreshToken);

    /// Nối tham số auth vào cuối `_url`: `?auth=<secret>` ở chế độ cũ,
    /// `?auth=<idToken>` ở chế độ mới. `sep` là '?' hoặc '&' tuỳ URL đã có
    /// query chưa. KHÔNG có bản dùng header cho RTDB — RTDB từ chối cả
    /// `Bearer` lẫn `Firebase` (MEMORY.md §17).
    void appendAuth(char sep);

    /// Firebase Storage thì NGƯỢC LẠI: nó nhận header `Authorization: Firebase
    /// <idToken>`. Khác dịch vụ, khác scheme. No-op khi còn dùng Database Secret.
    void addStorageAuthHeader(class HTTPClient& http);

    /// Gọi sau mỗi request: 401 nghĩa là token chết -> ép lấy lại ở chu kỳ sau
    void noteAuthFailure(int httpCode, const char* where);

    volatile bool _forceReassociate = false;
    volatile bool _otaRequested = false;
    volatile bool _isSyncing = false;
    volatile bool _isFirebaseSyncing = false;
    volatile bool _isNtpSyncing = false;
    volatile bool _hasPendingMessages = false;
    volatile uint32_t _numOfNewMsg = 0;
    volatile bool _isDownloadingMedia = false;
    std::function<void()> _onDownloadComplete = nullptr;

    bool _isTimeSynced = false;
    uint32_t _lastTimeSync = 0;

    WebServer* _webServer = nullptr;
    bool _webServerRunning = false;

    WebServer* _captiveServer = nullptr;
    bool _provisioningDone = false;
    String _provisionedSsid;
    String _provisionedPass;

    void handleCaptiveRoot();
    void handleCaptiveSubmit();
    /// Tra ve JSON danh sach Wi-Fi xung quanh. Quet bat dong bo nen khong chan
    /// web server: lan goi dau tra {"status":"scanning"}, client poll lai.
    void handleCaptiveScan();
    /// Cac URL do he dieu hanh goi de kiem tra "co internet khong". Tra 302 ve
    /// trang portal de may tu bat cua so dang nhap.
    void handleCaptiveProbe();
    /// Báo thức trên portal (AP mode, không có Internet). Sửa ở đây bật cờ dirty
    /// trong AlarmClock -> lần sync đầu tiên khi lên mạng đẩy đè lên cloud.
    void handleAlarmList();    // GET  /alarms
    void handleAlarmSave();    // POST /alarms/save   id?, time, en, rep
    void handleAlarmDelete();  // POST /alarms/delete id
    /// POST /time epoch — lấy giờ điện thoại khi hộp chưa từng có NTP (AP mode
    /// sau khi cắm điện), không có thì báo thức không bao giờ kêu được.
    void handleSetTime();
    String buildCaptivePortalHTML();
};

#endif // NETWORK_MANAGER_H
