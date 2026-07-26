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

private:
    volatile bool _isUpdating = false;

    static void sendJson(WebServer& server, int code, const char* body);
    void handleBegin(WebServer& server);
    void handleUploadDone(WebServer& server);
    void handleUploadData(WebServer& server);
};

#endif // OTA_HANDLER_H
