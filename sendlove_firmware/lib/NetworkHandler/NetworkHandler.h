#ifndef NETWORK_HANDLER_H
#define NETWORK_HANDLER_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>

// Forward declarations
class ConfigManager;
class SDCardManager;
class TimeManager;

// ============================================================================
// NetworkHandler — Wi-Fi, SoftAP Provisioning, Firebase API, Stream Download
// ============================================================================
// Phục vụ Task 2: Task_NetworkHandler
// Bao gồm cả Wi-Fi Provisioning (SoftAP + Captive Portal)
// ============================================================================

/// Kết quả kết nối Wi-Fi
enum class WiFiConnectResult : uint8_t {
    CONNECTED,          // Kết nối thành công
    FAILED,             // Thất bại (sai mật khẩu, không tìm thấy mạng)
    NO_CREDENTIALS,     // Không có credentials trong NVS
    TIMEOUT             // Hết thời gian chờ
};

/// Kết quả kiểm tra tin nhắn mới
struct MessageCheckResult {
    bool hasNewMessage = false;
    String videoUrl;
    String voiceUrl;
    String alarmTime;
    int8_t timezone = 7;
    bool isAlarmActive = false;
    // Wi-Fi config từ Firebase (nếu receiver đổi Wi-Fi)
    String newWifiSsid;
    String newWifiPass;
};

class NetworkHandler {
public:
    /// Khởi tạo module
    /// @param configMgr Con trỏ tới ConfigManager (để đọc/ghi Wi-Fi NVS)
    /// @param sdCard Con trỏ tới SDCardManager (để ghi file tải về)
    /// @param timeMgr Con trỏ tới TimeManager (để sync NTP)
    void init(ConfigManager* configMgr, SDCardManager* sdCard, TimeManager* timeMgr);

    // --- Wi-Fi Connection ---

    /// Thử kết nối Wi-Fi từ credentials trong NVS
    /// @return Kết quả kết nối
    WiFiConnectResult connectFromNVS();

    /// Kết nối Wi-Fi với SSID và password cụ thể
    WiFiConnectResult connectWiFi(const char* ssid, const char* password);

    /// Ngắt kết nối Wi-Fi (tiết kiệm năng lượng)
    void disconnectWiFi();

    /// Kiểm tra trạng thái kết nối
    bool isConnected();

    // --- Wi-Fi Provisioning (SoftAP + Captive Portal) ---

    /// Bắt đầu chế độ SoftAP + Captive Portal
    /// Hàm này block cho đến khi người dùng nhập xong Wi-Fi credentials
    /// @param apSsid Tên mạng SoftAP (mặc định: "SendloveBox-Setup")
    /// @param apPassword Mật khẩu SoftAP (mặc định: "" = open)
    void startProvisioningAP(const char* apSsid, const char* apPassword = "");

    /// Kiểm tra provisioning đã hoàn tất chưa
    bool isProvisioningDone();

    // --- Firebase API ---

    /// Kiểm tra tin nhắn mới và đọc cấu hình từ Firebase
    /// @param boxId ID của hộp (VD: "box_id_001")
    /// @param firebaseHost Firebase Realtime Database host
    /// @return Kết quả chứa thông tin tin nhắn, cấu hình
    MessageCheckResult checkFirebase(const char* boxId, const char* firebaseHost);

    /// Tải file từ URL và ghi thẳng vào SD card (stream, không buffer toàn bộ)
    /// @param url URL file trên Firebase Storage
    /// @param sdPath Đường dẫn file trên thẻ SD
    /// @return true nếu tải thành công
    bool downloadFileToSD(const char* url, const char* sdPath);

    /// Cập nhật trạng thái hộp lên Firebase
    /// @param boxId ID hộp
    /// @param firebaseHost Firebase host
    /// @param batteryPercent Phần trăm pin
    /// @return true nếu cập nhật thành công
    bool updateBoxStatus(const char* boxId, const char* firebaseHost,
                         uint8_t batteryPercent);

    /// Đánh dấu tin nhắn đã đọc trên Firebase (has_new_msg = false)
    bool markMessageRead(const char* boxId, const char* firebaseHost);

private:
    ConfigManager*  _configMgr = nullptr;
    SDCardManager*  _sdCard    = nullptr;
    TimeManager*    _timeMgr   = nullptr;

    // SoftAP Provisioning
    WebServer*      _captiveServer = nullptr;
    bool            _provisioningDone = false;
    String          _provisionedSsid;
    String          _provisionedPass;

    /// Handler cho trang chính của Captive Portal
    void handleCaptiveRoot();

    /// Handler cho form submit credentials
    void handleCaptiveSubmit();

    /// Tạo HTML cho trang Captive Portal
    String buildCaptivePortalHTML();
};

#endif // NETWORK_HANDLER_H
