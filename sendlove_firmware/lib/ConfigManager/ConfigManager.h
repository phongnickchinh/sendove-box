#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>

// ============================================================================
// ConfigManager — Quản lý cấu hình lưu trữ trên NVS (Non-Volatile Storage)
// ============================================================================
// Chỉ lưu dữ liệu cần tồn tại qua mất điện:
// - Wi-Fi credentials (SSID + Password)
// - Wi-Fi backup (rollback khi đổi Wi-Fi thất bại)
// ============================================================================

/// Kích thước buffer tối đa cho SSID và Password
static constexpr size_t WIFI_SSID_MAX_LEN = 33;  // 32 chars + null terminator
static constexpr size_t WIFI_PASS_MAX_LEN = 65;  // 64 chars + null terminator

struct AlarmItem {
    char id[16] = "";
    char time[6] = "00:00"; // "HH:MM"
    bool isEnable = false;
    bool repeatable = false;
};

class ConfigManager {
public:
    /// Khởi tạo NVS namespace
    /// @param namespaceName Tên namespace NVS (ví dụ: "sendlove")
    /// @return true nếu thành công
    bool init(const char* namespaceName);

    /// Đóng NVS handle
    void end();

    // --- Wi-Fi Credentials ---

    /// Lưu Wi-Fi credentials vào NVS
    /// @return true nếu ghi thành công
    bool saveWiFi(const char* ssid, const char* password);

    /// Đọc Wi-Fi credentials từ NVS
    /// @param ssid Buffer nhận SSID (tối thiểu WIFI_SSID_MAX_LEN bytes)
    /// @param password Buffer nhận password (tối thiểu WIFI_PASS_MAX_LEN bytes)
    /// @return true nếu đọc thành công
    bool loadWiFi(char* ssid, char* password);

    /// Kiểm tra đã có Wi-Fi credentials trong NVS chưa
    /// @return true nếu đã lưu SSID
    bool hasWiFiConfig();

    // --- Wi-Fi Backup (Rollback) ---

    /// Lưu Wi-Fi hiện tại làm backup trước khi đổi sang Wi-Fi mới
    bool saveBackupWiFi(const char* ssid, const char* password);

    /// Đọc Wi-Fi backup
    bool loadBackupWiFi(char* ssid, char* password);

    /// Xóa toàn bộ config (factory reset)
    bool clearAll();

    // --- Firebase Sync & Alarms ---

    /// Lưu mốc timestamp (ms) của tin nhắn cuối cùng đã tải
    bool saveLastDownloadTimestamp(uint64_t ts);

    /// Đọc mốc timestamp (ms) của tin nhắn cuối cùng đã tải
    uint64_t loadLastDownloadTimestamp();

    /// Lưu danh sách báo thức
    bool saveAlarms(const AlarmItem* alarms, size_t count);

    /// Đọc danh sách báo thức
    size_t loadAlarms(AlarmItem* alarms, size_t maxCount);

    /// Tính toán số giây còn lại đến mốc báo thức gần nhất
    /// @param currentEpochTime Timestamp UNIX epoch hiện tại tính bằng giây
    /// @return Số giây còn lại, hoặc 0xFFFFFFFF nếu không có báo thức
    uint32_t getSecondsToNextAlarm(time_t currentEpochTime);

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
};

#endif // CONFIG_MANAGER_H
