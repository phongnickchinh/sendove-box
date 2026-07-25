#include "OtaHandler.h"
#include <Update.h>
#include "config.h"

// ============================================================================
// OtaHandler Implementation
// ============================================================================
// Port từ phong_ir/src/api/ota_handler.cpp, chuyển sang class-based.
// Thay đổi chính:
//   - Bỏ LED manager (Sendlove dùng màn hình)
//   - Thêm _isUpdating flag cho FreeRTOS task synchronization
//   - CORS headers cho web client cross-origin
// ============================================================================

void OtaHandler::sendJson(WebServer& server, int code, const String& body) {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Connection", "close");
    server.send(code, "application/json", body);
}

// ---------- POST /api/ota/begin ----------
// Client gửi: size (bytes) và md5 (hex string).
// ESP32 kiểm tra dung lượng flash, chuẩn bị Update, trả {"ready":true}.
void OtaHandler::handleBegin(WebServer& server) {
    if (!server.hasArg("size")) {
        sendJson(server, 400,
                 "{\"ready\":false,\"error\":\"missing 'size' param\"}");
        return;
    }

    size_t fwSize = (size_t)server.arg("size").toInt();
    String md5 = server.hasArg("md5") ? server.arg("md5") : "";

    if (fwSize == 0) {
        sendJson(server, 400, "{\"ready\":false,\"error\":\"invalid size\"}");
        return;
    }

    // Kiểm tra còn đủ flash không
    if (!Update.begin(fwSize)) {
        String err = Update.errorString();
        Serial.printf("[OTA] Begin error: %s\n", err.c_str());
        sendJson(server, 500, "{\"ready\":false,\"error\":\"" + err + "\"}");
        return;
    }

    // Set MD5 checksum nếu có
    if (md5.length() == 32) {
        Update.setMD5(md5.c_str());
        Serial.printf("[OTA] MD5 expected: %s\n", md5.c_str());
    }

    // Đánh dấu đang OTA → các task khác sẽ tạm dừng
    _isUpdating = true;

    Serial.printf("[OTA] Ready to receive %u bytes\n", fwSize);
    sendJson(server, 200, "{\"ready\":true}");
}

// ---------- POST /api/ota/upload ----------
// Được gọi SAU KHI toàn bộ upload hoàn tất.
void OtaHandler::handleUploadDone(WebServer& server) {
    if (Update.hasError()) {
        String err = Update.errorString();
        _isUpdating = false;
        Serial.printf("[OTA] FAILED: %s\n", err.c_str());
        sendJson(server, 500, "{\"ok\":false,\"error\":\"" + err + "\"}");
    } else {
        Serial.println("[OTA] SUCCESS — Restarting...");
        sendJson(server, 200,
                 "{\"ok\":true,\"msg\":\"Update thanh cong, dang restart...\"}");
        delay(1000);
        ESP.restart();
    }
}

// Upload handler — được gọi cho từng chunk dữ liệu
void OtaHandler::handleUploadData(WebServer& server) {
    HTTPUpload& upload = server.upload();

    switch (upload.status) {
    case UPLOAD_FILE_START:
        Serial.printf("[OTA] Receiving firmware: %s\n", upload.filename.c_str());
        // Update.begin() đã gọi ở /api/ota/begin, không cần gọi lại.
        break;

    case UPLOAD_FILE_WRITE:
        // Ghi từng chunk vào flash
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            Serial.printf("[OTA] Write error: %s\n", Update.errorString());
        }
        break;

    case UPLOAD_FILE_END:
        // Kết thúc ghi, xác nhận firmware
        if (Update.end(true)) {
            Serial.printf("[OTA] Upload complete: %u bytes\n", upload.totalSize);
        } else {
            Serial.printf("[OTA] End error: %s\n", Update.errorString());
        }
        break;

    case UPLOAD_FILE_ABORTED:
        _isUpdating = false;
        Update.abort();
        Serial.println("[OTA] Upload aborted");
        break;
    }
}

// ---------- Register Routes ----------
void OtaHandler::registerRoutes(WebServer& server) {
    server.on("/api/ota/begin", HTTP_POST,
              [this, &server]() { handleBegin(server); });

    server.on(
        "/api/ota/upload", HTTP_POST,
        [this, &server]() { handleUploadDone(server); },  // onComplete
        [this, &server]() { handleUploadData(server); }   // onUpload (per-chunk)
    );

    // Status endpoint — kiểm tra firmware version và OTA state
    server.on("/api/status", HTTP_GET, [this, &server]() {
        String body = "{\"ok\":true,\"version\":\"" + String(FW_VERSION) +
                      "\",\"updating\":" + String(_isUpdating ? "true" : "false") +
                      ",\"heap\":" + String(ESP.getFreeHeap()) + "}";
        sendJson(server, 200, body);
    });

    Serial.println("[OTA] Routes registered: /api/ota/begin, /api/ota/upload, /api/status");
}
