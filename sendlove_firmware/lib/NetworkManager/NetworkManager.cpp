#include "NetworkManager.h"
#include "captive_portal_html.h"
#include <DNSServer.h>
#include <time.h>
#include <vector>
#include "esp_sntp.h"

static const byte DNS_PORT = 53;
static DNSServer dnsServer;

void NetworkManager::init() {
    WiFi.mode(WIFI_STA);
    // Initialize timezone once at boot
    configTzTime("ICT-7", NTP_SERVER_1, NTP_SERVER_2, NTP_SERVER_3);
}

WiFiConnectResult NetworkManager::connectWiFi(const char* ssid, const char* password) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start < WIFI_CONNECT_TIMEOUT_MS)) {
        delay(500);
    }

    if (WiFi.status() == WL_CONNECTED) {
        triggerNtpSync();
        return WiFiConnectResult::CONNECTED;
    }
    return WiFiConnectResult::FAILED;
}

void NetworkManager::disconnectWiFi() {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
}

bool NetworkManager::isReady() const {
    return (WiFi.status() == WL_CONNECTED) && _isTimeSynced;
}

bool NetworkManager::isConnected() const {
    return (WiFi.status() == WL_CONNECTED);
}

void NetworkManager::ensureConnected() {
    if (WiFi.status() != WL_CONNECTED) {
        WiFi.reconnect();
    }
}

bool NetworkManager::isTimeSynced() const {
    return _isTimeSynced;
}

void NetworkManager::update() {
    if (_captiveServer != nullptr) {
        dnsServer.processNextRequest();
        _captiveServer->handleClient();
        return;
    }

    if (_webServerRunning && _webServer != nullptr) {
        _webServer->handleClient();
    }
}

void NetworkManager::triggerNtpSync() {
    if (WiFi.status() != WL_CONNECTED) return;
    if (_isNtpSyncing) return;

    // Re-apply timezone mỗi lần sync để chống drift sau Light Sleep (thời gian bị lệch sau 1 ngày)
    configTzTime(TIMEZONE_ENV, NTP_SERVER_1, NTP_SERVER_2, NTP_SERVER_3);

    _isNtpSyncing = true;
    xTaskCreate(ntpTaskWorker, "NtpSync", 4096, this, 1, nullptr);
}

void NetworkManager::ntpTaskWorker(void* param) {
    NetworkManager* self = static_cast<NetworkManager*>(param);
    if (self == nullptr) {
        vTaskDelete(nullptr);
        return;
    }

    Serial.println("[NetworkManager] Background NTP sync started...");

    // Record RTC time BEFORE waiting for NTP response
    time_t rtcBefore = time(nullptr);

    struct tm timeinfo;
    // Wait for NTP update in background task (up to 10 seconds timeout)
    bool syncOk = getLocalTime(&timeinfo, 10000);

    if (syncOk) {
        time_t ntpNow = mktime(&timeinfo);
        time_t rtcNow = time(nullptr);

        if (!self->_isTimeSynced) {
            // First time sync after boot
            self->_isTimeSynced = true;
            Serial.printf("[NetworkManager] First NTP sync successful! Current year: %d\n", timeinfo.tm_year + 1900);
        } else {
            // Compare drift between NTP time and running RTC time
            long diffSec = std::abs((long)(ntpNow - rtcNow));
            Serial.printf("[NetworkManager] NTP Sync completed. RTC drift: %ld sec\n", diffSec);

            // If drift is larger than 5 seconds, settimeofday was already applied by getLocalTime/sntp.
            // If drift <= 5 seconds, settimeofday adjustment is minor and ignored to prevent screen jumps.
            if (diffSec > 5) {
                Serial.println("[NetworkManager] Large RTC drift detected (> 5s). System time updated.");
            }
        }
    } else {
        Serial.println("[NetworkManager] Background NTP sync timeout / failed.");
    }

    self->_isNtpSyncing = false;
    vTaskDelete(nullptr);
}

void NetworkManager::getTimeString(char* buffer, size_t maxLen) const {
    if (buffer == nullptr || maxLen == 0) return;
    if (!_isTimeSynced) {
        snprintf(buffer, maxLen, "00:00");
        return;
    }

    struct tm timeinfo;
    // Non-blocking read from internal ESP32 RTC (timeout = 0)
    if (!getLocalTime(&timeinfo, 0)) {
        snprintf(buffer, maxLen, "00:00");
        return;
    }

    strftime(buffer, maxLen, "%H:%M", &timeinfo);
}

void NetworkManager::getDateString(char* buffer, size_t maxLen) const {
    if (buffer == nullptr || maxLen == 0) return;
    if (!_isTimeSynced) {
        snprintf(buffer, maxLen, "Loading...");
        return;
    }

    struct tm timeinfo;
    // Non-blocking read from internal ESP32 RTC (timeout = 0)
    if (!getLocalTime(&timeinfo, 0)) {
        snprintf(buffer, maxLen, "Loading...");
        return;
    }

    const char* days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sar"};

    snprintf(buffer, maxLen, "%s, %02d.%02d",
             days[timeinfo.tm_wday],
             timeinfo.tm_mday,
             timeinfo.tm_mon + 1);
}

int NetworkManager::getWifiRSSI() const {
    if (WiFi.status() != WL_CONNECTED) return -100;
    return WiFi.RSSI();
}

void NetworkManager::startProvisioningAP(const char* apSsid, const char* apPassword) {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(apSsid, apPassword);

    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

    if (_captiveServer == nullptr) _captiveServer = new WebServer(80);

    _captiveServer->on("/", [this]() { handleCaptiveRoot(); });
    _captiveServer->on("/save", [this]() { handleCaptiveSubmit(); });
    _captiveServer->onNotFound([this]() { handleCaptiveRoot(); });

    _captiveServer->begin();
    _provisioningDone = false;
}

bool NetworkManager::isProvisioningDone() const {
    return _provisioningDone;
}

bool NetworkManager::isProvisioningActive() const {
    return (_captiveServer != nullptr && !_provisioningDone);
}

void NetworkManager::handleCaptiveRoot() {
    if (_captiveServer) _captiveServer->send(200, "text/html", buildCaptivePortalHTML());
}

#include "ConfigManager.h"

void NetworkManager::handleCaptiveSubmit() {
    if (!_captiveServer) return;

    if (_captiveServer->hasArg("ssid") && _captiveServer->hasArg("password")) {
        _provisionedSsid = _captiveServer->arg("ssid");
        _provisionedPass = _captiveServer->arg("password");
        _provisioningDone = true;

        ConfigManager cfg;
        if (cfg.init(NVS_NAMESPACE)) {
            cfg.saveWiFi(_provisionedSsid.c_str(), _provisionedPass.c_str());
            cfg.end();
        }

        String html = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1'></head>"
                      "<body style='font-family:sans-serif;text-align:center;padding-top:40px;'>"
                      "<h2>Saved Wi-Fi!</h2><p>Sendlove Box is restarting...</p></body></html>";
        _captiveServer->send(200, "text/html", html);
        delay(2000);
        ESP.restart();
    } else {
        _captiveServer->send(400, "text/plain", "Missing SSID or Password!");
    }
}

String NetworkManager::buildCaptivePortalHTML() {
    return String(FPSTR(CAPTIVE_PORTAL_HTML));
}

void NetworkManager::startWebServer(const char* hostname) {
    if (_webServerRunning) return;

    if (_webServer == nullptr) _webServer = new WebServer(80);

    MDNS.begin(hostname);
    _webServer->begin();
    _webServerRunning = true;
}

void NetworkManager::stopWebServer() {
    if (_webServer != nullptr) {
        _webServer->stop();
        delete _webServer;
        _webServer = nullptr;
    }
    _webServerRunning = false;
    MDNS.end();
}

WebServer* NetworkManager::getWebServer() {
    return _webServer;
}

bool NetworkManager::isWebServerRunning() const {
    return _webServerRunning;
}

// ============================================================================
// Firebase REST API Integration (Wakeup Lifecycle Sync)
// ============================================================================

#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "IStorageProvider.h"
#include "ConfigManager.h"
#include "DisplayDriver.h"

struct FirebaseTaskParams {
    NetworkManager* self;
    uint8_t batteryPercent;
    bool isCharging;
    IStorageProvider* storage;
    DisplayDriver* display;
};

void NetworkManager::triggerFirebaseSync(uint8_t batteryPercent, bool isCharging, IStorageProvider* storage) {
    if (WiFi.status() != WL_CONNECTED) return;
    if (_isFirebaseSyncing) return;

    _isFirebaseSyncing = true;
    FirebaseTaskParams* p = new FirebaseTaskParams{this, batteryPercent, isCharging, storage, nullptr};
    BaseType_t res = xTaskCreate(firebaseSyncTaskWorker, "FbSync", 12288, p, 2, nullptr);
    if (res != pdPASS) {
        _isFirebaseSyncing = false;
        delete p;
        Serial.println(F("[NetworkManager] Failed to create FbSync task (Out of RAM)!"));
    }
}

void NetworkManager::firebaseSyncTaskWorker(void* param) {
    FirebaseTaskParams* p = static_cast<FirebaseTaskParams*>(param);
    if (p && p->self) {
        p->self->syncFirebaseWakeup(p->batteryPercent, p->isCharging, p->storage);
        p->self->_isFirebaseSyncing = false;
        delete p;
    }
    vTaskDelete(nullptr);
}

bool NetworkManager::syncFirebaseWakeup(uint8_t batteryPercent, bool isCharging, IStorageProvider* storage) {
    // Chờ tối đa 5s cho Wi-Fi tái kết nối ổn định sau khi chip thức dậy từ Light Sleep
    uint32_t waitStart = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - waitStart < 5000)) {
        delay(100);
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println(F("[NetworkManager] Firebase Sync skipped: Wi-Fi not connected."));
        return false;
    }

    Serial.println(F("[NetworkManager] Starting Silent Firebase Wakeup Sync..."));

    // 1. Update Status (Heartbeat)
    updateFirebaseStatus(batteryPercent, isCharging);
    delay(150);

    // 2. Check Flags (Alarms, OTA, Pairing)
    checkFirebaseFlags();
    delay(150);

    // 3. Check and download new messages
    if (storage != nullptr) {
        checkAndDownloadNewMessages(storage);
    }

    Serial.println(F("[NetworkManager] Silent Firebase Wakeup Sync completed."));
    return true;
}

bool NetworkManager::updateFirebaseStatus(uint8_t batteryPercent, bool isCharging) {
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;

    char url[256];
    snprintf(url, sizeof(url), "https://%s/boxes/%s/status.json?auth=%s",
             FIREBASE_HOST, BOX_ID, FIREBASE_AUTH_SECRET);

    if (!http.begin(client, url)) return false;

    http.setTimeout(FIREBASE_TIMEOUT_MS);
    http.addHeader("Content-Type", "application/json");

    char payload[128];
    uint32_t now = (uint32_t)time(nullptr);
    snprintf(payload, sizeof(payload),
             "{\"online\":true,\"battery\":%d,\"is_charging\":%s,\"last_seen\":%u}",
             batteryPercent, isCharging ? "true" : "false", now);

    int httpCode = http.PATCH((uint8_t*)payload, strlen(payload));
    http.end();

    return (httpCode == HTTP_CODE_OK);
}

bool NetworkManager::checkFirebaseFlags() {
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;

    char url[256];
    snprintf(url, sizeof(url), "https://%s/boxes/%s/flags.json?auth=%s",
             FIREBASE_HOST, BOX_ID, FIREBASE_AUTH_SECRET);

    if (!http.begin(client, url)) return false;
    http.setTimeout(FIREBASE_TIMEOUT_MS);

    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();

    if (payload == "null" || payload.length() <= 2) return true;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) return false;

    bool syncAlarmsFlag = doc["sync_alarms_flag"] | false;
    bool emergencyOta = doc["emergency_ota"] | false;
    bool normalOta = doc["normal_ota"] | false;

    if (syncAlarmsFlag) {
        syncFirebaseAlarms();
    }

    // Reset cờ sau khi đọc
    WiFiClientSecure patchClient;
    patchClient.setInsecure();
    HTTPClient patchHttp;

    char patchUrl[256];
    snprintf(patchUrl, sizeof(patchUrl), "https://%s/boxes/%s/flags.json?auth=%s",
             FIREBASE_HOST, BOX_ID, FIREBASE_AUTH_SECRET);

    if (patchHttp.begin(patchClient, patchUrl)) {
        patchHttp.setTimeout(FIREBASE_TIMEOUT_MS);
        patchHttp.addHeader("Content-Type", "application/json");
        patchHttp.PATCH("{\"sync_alarms_flag\":false,\"emergency_ota\":false,\"normal_ota\":false}");
        patchHttp.end();
    }

    return true;
}

bool NetworkManager::syncFirebaseAlarms() {
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;

    char url[256];
    snprintf(url, sizeof(url), "https://%s/boxes/%s/config/alarm_list.json?auth=%s",
             FIREBASE_HOST, BOX_ID, FIREBASE_AUTH_SECRET);

    if (!http.begin(client, url)) return false;
    http.setTimeout(FIREBASE_TIMEOUT_MS);

    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();

    if (payload == "null" || payload.length() <= 2) return true;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) return false;

    AlarmItem alarms[MAX_ALARMS];
    size_t count = 0;

    JsonObject obj = doc.as<JsonObject>();
    for (JsonPair kv : obj) {
        if (count >= MAX_ALARMS) break;
        JsonObject alarmObj = kv.value().as<JsonObject>();
        strncpy(alarms[count].id, kv.key().c_str(), sizeof(alarms[count].id) - 1);
        const char* tStr = alarmObj["time"] | "00:00";
        strncpy(alarms[count].time, tStr, sizeof(alarms[count].time) - 1);
        alarms[count].isEnable = alarmObj["is_enable"] | false;
        alarms[count].repeatable = alarmObj["repeatable"] | false;
        count++;
    }

    ConfigManager cfg;
    if (cfg.init(NVS_NAMESPACE)) {
        cfg.saveAlarms(alarms, count);
        cfg.end();
    }

    return true;
}

bool NetworkManager::checkAndDownloadNewMessages(IStorageProvider* storage) {
    if (!storage) return false;

    ConfigManager cfg;
    uint64_t lastTs = 0;
    if (cfg.init(NVS_NAMESPACE)) {
        lastTs = cfg.loadLastDownloadTimestamp();
        cfg.end();
    }

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;

    char url[384];
    snprintf(url, sizeof(url),
             "https://%s/messages/%s.json?auth=%s",
             FIREBASE_HOST, BOX_ID, FIREBASE_AUTH_SECRET);

    if (!http.begin(client, url)) {
        Serial.println(F("[NetworkManager] HTTP init failed for messages endpoint."));
        return false;
    }
    http.setTimeout(FIREBASE_TIMEOUT_MS);

    int httpCode = http.GET();
    if (httpCode < 0) {
        // Tự thử lại lần 2 sau 500ms nếu ổ cắm TCP vừa khôi phục sau khi chip tỉnh dậy từ Light Sleep
        delay(500);
        httpCode = http.GET();
    }

    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("[NetworkManager] Firebase HTTP GET failed with code: %d\n", httpCode);
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();

    if (payload == "null" || payload.length() <= 2) {
        return true;
    }

    // Zero-copy JSON parsing từ RAM string (hỗ trợ chuỗi dài bất kỳ như bin_url mà không bị xén)
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);

    if (err) {
        Serial.printf("[NetworkManager] JSON parse error: %s\n", err.c_str());
        return false;
    }

    if (doc.isNull()) {
        return true;
    }

    // Tạo danh sách các tin nhắn từ JsonObject hoặc JsonArray (Firebase tự động biến đổi tùy theo dạng key)
    std::vector<JsonObject> msgList;
    if (doc.is<JsonObject>()) {
        JsonObject obj = doc.as<JsonObject>();
        for (JsonPair kv : obj) {
            if (kv.value().is<JsonObject>()) {
                msgList.push_back(kv.value().as<JsonObject>());
            }
        }
    } else if (doc.is<JsonArray>()) {
        JsonArray arr = doc.as<JsonArray>();
        for (JsonVariant v : arr) {
            if (v.is<JsonObject>()) {
                msgList.push_back(v.as<JsonObject>());
            }
        }
    }

    if (msgList.empty()) {
        return true;
    }

    uint64_t maxTs = lastTs;
    bool hasNewMsg = false;

    bool downloadedAnyMedia = false;

    for (JsonObject msg : msgList) {
        uint64_t ts = msg["timestamp"] | 0ULL;

        if (ts <= lastTs) {
            continue;
        }

        hasNewMsg = true;
        if (ts > maxTs) maxTs = ts;

        // Tìm kiếm linh hoạt tất cả các biến thể đặt tên key (snake_case, camelCase...)
        String rawMediaUrl = "";
        const char* candidateKeys[] = {
            "bin_url", "binUrl", 
            "video_url", "videoUrl", 
            "image_url", "imageUrl", 
            "media_url", "mediaUrl", 
            "url"
        };
        for (const char* k : candidateKeys) {
            if (msg.containsKey(k) && !msg[k].isNull()) {
                String val = msg[k].as<String>();
                val.trim();
                if (val.length() > 0 && val != "null") {
                    rawMediaUrl = val;
                    break;
                }
            }
        }

        if (rawMediaUrl.length() > 0) {
            String fullUrl = rawMediaUrl;
            
            // Xử lý tự động convert relative path hoặc gs:// thành HTTP Download URL của Firebase Storage API
            if (!fullUrl.startsWith("http")) {
                if (fullUrl.startsWith("gs://")) {
                    int slashIdx = fullUrl.indexOf('/', 5);
                    if (slashIdx > 0) fullUrl = fullUrl.substring(slashIdx + 1);
                }
                if (fullUrl.startsWith("/")) fullUrl.remove(0, 1);
                fullUrl.replace("/", "%2F"); // Đổi / thành %2F
                fullUrl = "https://firebasestorage.googleapis.com/v0/b/iot-app-839a2.firebasestorage.app/o/" + fullUrl + "?alt=media";
            }

            _isDownloadingMedia = true;
            Serial.printf("[NetworkManager] Downloading bin media from: %s\n", fullUrl.c_str());
            
            if (http.begin(client, fullUrl.c_str())) {
                http.setTimeout(30000);
                int code = http.GET();
                if (code == HTTP_CODE_OK) {
                    int len = http.getSize();
                    int initialLen = len;
                    int totalRead = 0;
                    Serial.printf("[NetworkManager] HTTP GET OK. Content-Length: %d\n", len);
                    char writeSlotId[16] = "";
                    if (!storage->getNextWriteSlotIdentifier(writeSlotId, sizeof(writeSlotId))) {
                        Serial.println(F("[NetworkManager] Download skipped: Storage is FULL (5 unread messages)."));
                        http.end();
                        _isDownloadingMedia = false;
                        break;
                    }

                    WiFiClient* stream = http.getStreamPtr();
                    if (storage->openForWrite(writeSlotId)) {
                        uint8_t buffer[256];
                        while (http.connected() && (len > 0 || len == -1)) {
                            size_t sizeAvail = stream->available();
                            if (sizeAvail) {
                                size_t toRead = (sizeAvail < sizeof(buffer)) ? sizeAvail : sizeof(buffer);
                                int c = stream->readBytes(buffer, toRead);
                                storage->writeChunk(buffer, c);
                                totalRead += c;
                                if (len > 0) len -= c;
                            }
                            delay(1);
                        }
                        storage->closeWrite();
                        Serial.printf("[NetworkManager] Media download to Slot %s completed. totalRead: %d bytes\n", writeSlotId, totalRead);
                        downloadedAnyMedia = true;
                        if (_onDownloadComplete) {
                            _onDownloadComplete();
                        }
                    }
                } else {
                    Serial.printf("[NetworkManager] HTTP download failed with code: %d\n", code);
                }
                http.end();
            }
            _isDownloadingMedia = false;
        } else {
            Serial.println(F("[NetworkManager] Message skipped: No valid media URL found."));
        }
    }


    if (maxTs > lastTs) {
        if (cfg.init(NVS_NAMESPACE)) {
            cfg.saveLastDownloadTimestamp(maxTs);
            cfg.end();
            Serial.printf("[NetworkManager] Updated last_download_ts to %llu\n", (unsigned long long)maxTs);
        }
    }

    return downloadedAnyMedia;
}
