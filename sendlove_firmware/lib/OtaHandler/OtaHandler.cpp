#include "OtaHandler.h"
#include <Update.h>
#include "ScreenLogger.h"
#include "config.h"

void OtaHandler::sendJson(WebServer& server, int code, const char* body) {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Connection", "close");
    server.send(code, "application/json", body);
}

void OtaHandler::handleBegin(WebServer& server) {
    if (!server.hasArg("size")) {
        sendJson(server, 400, "{\"ready\":false,\"error\":\"missing 'size' param\"}");
        return;
    }

    size_t fwSize = (size_t)server.arg("size").toInt();
    String md5 = server.hasArg("md5") ? server.arg("md5") : "";

    if (fwSize == 0) {
        sendJson(server, 400, "{\"ready\":false,\"error\":\"invalid size\"}");
        return;
    }

    if (!Update.begin(fwSize)) {
        char errBuf[128];
        snprintf(errBuf, sizeof(errBuf), "{\"ready\":false,\"error\":\"%s\"}", Update.errorString());
        sendJson(server, 500, errBuf);
        return;
    }

    if (md5.length() == 32) Update.setMD5(md5.c_str());

    // The watchdog clock starts at begin, not at the first chunk: a client that
    // calls begin and then dies must be cleaned up too.
    _lastChunkMs = millis();
    _isUpdating = true;
    sendJson(server, 200, "{\"ready\":true}");
}

void OtaHandler::handleUploadDone(WebServer& server) {
    if (Update.hasError()) {
        char errBuf[128];
        snprintf(errBuf, sizeof(errBuf), "{\"ok\":false,\"error\":\"%s\"}", Update.errorString());
        _isUpdating = false;
        sendJson(server, 500, errBuf);
    } else {
        sendJson(server, 200, "{\"ok\":true,\"msg\":\"Update success, restarting...\"}");
        delay(1000);
        ESP.restart();
    }
}

void OtaHandler::handleUploadData(WebServer& server) {
    HTTPUpload& upload = server.upload();

    switch (upload.status) {
    case UPLOAD_FILE_START:
        break;

    case UPLOAD_FILE_WRITE:
        _lastChunkMs = millis();
        Update.write(upload.buf, upload.currentSize);
        break;

    case UPLOAD_FILE_END:
        Update.end(true);
        break;

    case UPLOAD_FILE_ABORTED:
        _isUpdating = false;
        Update.abort();
        break;
    }
}

void OtaHandler::registerRoutes(WebServer& server) {
    server.on("/api/ota/begin", HTTP_POST, [this, &server]() { handleBegin(server); });

    server.on(
        "/api/ota/upload", HTTP_POST,
        [this, &server]() { handleUploadDone(server); },
        [this, &server]() { handleUploadData(server); }
    );

    server.on("/api/status", HTTP_GET, [this, &server]() {
        char body[128];
        snprintf(body, sizeof(body), "{\"ok\":true,\"version\":\"%s\",\"updating\":%s,\"heap\":%u}",
                 FW_VERSION, _isUpdating ? "true" : "false", ESP.getFreeHeap());
        sendJson(server, 200, body);
    });
}

void OtaHandler::tickWatchdog() {
    if (!_isUpdating) return;
    if (millis() - _lastChunkMs < OTA_STALL_TIMEOUT_MS) return;

    // Aborting is safe: Update.write() only writes the NON-running partition, and
    // otadata changes only when Update.end(true) succeeds -> the running image is intact.
    Update.abort();
    _isUpdating = false;
    DLOG("[OTA] huy: %lus khong co chunk nao", (unsigned long)(OTA_STALL_TIMEOUT_MS / 1000));
}

uint8_t OtaHandler::progressPercent() const {
    if (!_isUpdating || Update.size() == 0) return 0;
    return (uint8_t)((uint64_t)Update.progress() * 100 / Update.size());
}
