#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include "config.h"

/// Kết quả kết nối Wi-Fi
enum class WiFiConnectResult : uint8_t {
    CONNECTED,          // Kết nối thành công
    FAILED,             // Thất bại
    NO_CREDENTIALS,     // Không có credentials trong NVS
    TIMEOUT             // Hết thời gian chờ
};

class NetworkManager {
public:
    NetworkManager() = default;

    /// Khởi tạo kết nối WiFi STA
    void init();

    /// Kết nối Wi-Fi với SSID và Password cụ thể
    WiFiConnectResult connectWiFi(const char* ssid, const char* password);

    /// Ngắt kết nối Wi-Fi (tiết kiệm năng lượng)
    void disconnectWiFi();

    /// Kiểm tra đã có mạng và giờ NTP đồng bộ thành công chưa
    bool isReady() const;

    /// Kiểm tra Wi-Fi đã được kết nối chưa
    bool isConnected() const;

    /// Ép khôi phục kết nối Wi-Fi nếu bị ngắt sau khi tỉnh giấc từ Sleep
    void ensureConnected();

    /// Lấy chuỗi hiển thị giờ hiện tại (VD: "14:30")
    String getTimeString() const;

    /// Lấy chuỗi hiển thị ngày hiện tại (VD: "THỨ BẢY, 26/07/2026")
    String getDateString() const;

    /// Lấy RSSI (VD: -50 dBm)
    int getWifiRSSI() const;

    /// Update định kỳ (nếu cần gọi trong loop/task)
    void update();

    // --- OTA Web Server (STA mode) ---

    /// Khởi tạo WebServer trên port 80 + mDNS (chạy khi đã có WiFi STA)
    void startWebServer(const char* hostname = "sendlovebox");

    /// Dừng WebServer và giải phóng memory
    void stopWebServer();

    /// Lấy pointer đến WebServer (để OtaHandler đăng ký routes)
    WebServer* getWebServer();

    /// Kiểm tra WebServer đang chạy không
    bool isWebServerRunning() const;

    // --- Wi-Fi Provisioning (SoftAP + Captive Portal) ---

    /// Bắt đầu chế độ SoftAP Captive Portal để cấu hình WiFi qua Web
    void startProvisioningAP(const char* apSsid = "SendloveBox-Setup", const char* apPassword = "");

    /// Kiểm tra provisioning đã hoàn tất chưa
    bool isProvisioningDone() const;

private:
    bool _isTimeSynced = false;
    uint32_t _lastTimeSync = 0;
    void syncTime();

    // OTA Web Server (STA mode)
    WebServer* _webServer = nullptr;
    bool _webServerRunning = false;

    // SoftAP Provisioning
    WebServer* _captiveServer = nullptr;
    bool _provisioningDone = false;
    String _provisionedSsid;
    String _provisionedPass;

    void handleCaptiveRoot();
    void handleCaptiveSubmit();
    String buildCaptivePortalHTML();
};

#endif // NETWORK_MANAGER_H
