#ifndef OTA_HANDLER_H
#define OTA_HANDLER_H

#include <Arduino.h>
#include <WebServer.h>

// ============================================================================
// OtaHandler — OTA Firmware Update qua mạng LAN
// ============================================================================
// Cung cấp 2 HTTP endpoints cho OTA push từ PC/Web Client:
//   POST /api/ota/begin  — Chuẩn bị flash partition (size + MD5)
//   POST /api/ota/upload — Nhận firmware binary (multipart chunked)
//
// Sử dụng <Update.h> có sẵn của ESP-IDF/Arduino.
// Flag isUpdating() cho phép các task khác tạm dừng khi OTA đang chạy.
// ============================================================================

class OtaHandler {
public:
    /// Register OTA endpoints to WebServer
    void registerRoutes(WebServer& server);

    /// Check if OTA update is currently in progress
    bool isUpdating() const { return _isUpdating; }

    /// Gọi đều đặn từ vòng lặp. Đang nạp mà quá OTA_STALL_TIMEOUT_MS không có chunk
    /// nào thì huỷ phiên: TCP đứt giữa chừng không phải lúc nào cũng sinh ra
    /// UPLOAD_FILE_ABORTED, và thiếu chốt này thì _isUpdating kẹt true vĩnh viễn.
    void tickWatchdog();

    /// 0-100, cho màn hình chế độ OTA. 0 khi chưa nạp.
    uint8_t progressPercent() const;

private:
    volatile bool _isUpdating = false;
    volatile uint32_t _lastChunkMs = 0;

    static void sendJson(WebServer& server, int code, const char* body);
    void handleBegin(WebServer& server);
    void handleUploadDone(WebServer& server);
    void handleUploadData(WebServer& server);
};

#endif // OTA_HANDLER_H
