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
    /// Đăng ký OTA routes vào WebServer
    void registerRoutes(WebServer& server);

    /// Kiểm tra OTA đang diễn ra (các task khác nên tạm dừng)
    bool isUpdating() const { return _isUpdating; }

private:
    volatile bool _isUpdating = false;

    static void sendJson(WebServer& server, int code, const String& body);
    void handleBegin(WebServer& server);
    void handleUploadDone(WebServer& server);
    void handleUploadData(WebServer& server);
};

#endif // OTA_HANDLER_H
