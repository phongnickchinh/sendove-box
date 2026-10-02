#ifndef OTA_HANDLER_H
#define OTA_HANDLER_H

#include <Arduino.h>
#include <WebServer.h>

// OtaHandler — OTA firmware update over the LAN:
//   POST /api/ota/begin  — prepare the flash partition (size + MD5)
//   POST /api/ota/upload — receive the firmware binary (multipart)

class OtaHandler {
public:
    /// Register OTA endpoints to WebServer
    void registerRoutes(WebServer& server);

    /// Check if OTA update is currently in progress
    bool isUpdating() const { return _isUpdating; }

    /// Call regularly: aborts a flash that got no chunk for OTA_STALL_TIMEOUT_MS,
    /// so a dropped connection can't leave _isUpdating stuck.
    void tickWatchdog();

    /// 0-100, for the OTA mode screen. 0 when not flashing.
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
