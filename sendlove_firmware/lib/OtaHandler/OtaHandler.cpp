#include "OtaHandler.h"
#include <Update.h>
#include "config.h"

void OtaHandler::sendJson(WebServer& server, int code, const String& body) {
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
        String err = Update.errorString();
        sendJson(server, 500, "{\"ready\":false,\"error\":\"" + err + "\"}");
        return;
    }

    if (md5.length() == 32) Update.setMD5(md5.c_str());

    _isUpdating = true;
    sendJson(server, 200, "{\"ready\":true}");
}

void OtaHandler::handleUploadDone(WebServer& server) {
    if (Update.hasError()) {
        String err = Update.errorString();
        _isUpdating = false;
        sendJson(server, 500, "{\"ok\":false,\"error\":\"" + err + "\"}");
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
        String body = "{\"ok\":true,\"version\":\"" + String(FW_VERSION) +
                      "\",\"updating\":" + String(_isUpdating ? "true" : "false") +
                      ",\"heap\":" + String(ESP.getFreeHeap()) + "}";
        sendJson(server, 200, body);
    });
}
