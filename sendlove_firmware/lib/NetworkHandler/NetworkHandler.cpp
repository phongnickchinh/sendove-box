#include "NetworkHandler.h"
#include "ConfigManager.h"
#include "SDCardManager.h"
#include "TimeManager.h"
#include "config.h"
#include <ArduinoJson.h>
#include <DNSServer.h>

// ============================================================================
// NetworkHandler Implementation
// ============================================================================

// DNS server cho Captive Portal (chuyển mọi domain → IP của SoftAP)
static DNSServer dnsServer;
static const byte DNS_PORT = 53;

void NetworkHandler::init(ConfigManager* configMgr, SDCardManager* sdCard,
                          TimeManager* timeMgr) {
    _configMgr = configMgr;
    _sdCard    = sdCard;
    _timeMgr   = timeMgr;

    Serial.println(F("[Network] Initialized"));
}

// ============================================================================
// Wi-Fi Connection
// ============================================================================

WiFiConnectResult NetworkHandler::connectFromNVS() {
    if (_configMgr == nullptr || !_configMgr->hasWiFiConfig()) {
        Serial.println(F("[Network] No Wi-Fi credentials in NVS"));
        return WiFiConnectResult::NO_CREDENTIALS;
    }

    char ssid[WIFI_SSID_MAX_LEN];
    char pass[WIFI_PASS_MAX_LEN];

    if (!_configMgr->loadWiFi(ssid, pass)) {
        return WiFiConnectResult::NO_CREDENTIALS;
    }

    return connectWiFi(ssid, pass);
}

WiFiConnectResult NetworkHandler::connectWiFi(const char* ssid, const char* password) {
    Serial.printf("[Network] Connecting to Wi-Fi: %s\n", ssid);

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > WIFI_CONNECT_TIMEOUT_MS) {
            Serial.println(F("[Network] Wi-Fi connection TIMEOUT"));
            WiFi.disconnect();
            return WiFiConnectResult::TIMEOUT;
        }
        delay(250);
    }

    Serial.printf("[Network] Connected! IP: %s\n", WiFi.localIP().toString().c_str());

    // Đồng bộ NTP nếu có TimeManager
    if (_timeMgr != nullptr) {
        _timeMgr->syncNTP();
    }

    return WiFiConnectResult::CONNECTED;
}

void NetworkHandler::disconnectWiFi() {
    WiFi.disconnect(true); // true = tắt hẳn Wi-Fi radio
    WiFi.mode(WIFI_OFF);
    Serial.println(F("[Network] Wi-Fi disconnected & radio off"));
}

bool NetworkHandler::isConnected() {
    return (WiFi.status() == WL_CONNECTED);
}

// ============================================================================
// Wi-Fi Provisioning (SoftAP + Captive Portal)
// ============================================================================

void NetworkHandler::startProvisioningAP(const char* apSsid, const char* apPassword) {
    Serial.printf("[Network] Starting SoftAP: %s\n", apSsid);

    _provisioningDone = false;

    // Bật SoftAP mode
    WiFi.mode(WIFI_AP);
    if (strlen(apPassword) > 0) {
        WiFi.softAP(apSsid, apPassword);
    } else {
        WiFi.softAP(apSsid); // Open network
    }

    IPAddress apIP = WiFi.softAPIP();
    Serial.printf("[Network] SoftAP IP: %s\n", apIP.toString().c_str());

    // DNS server: chuyển mọi domain → IP của SoftAP (Captive Portal)
    dnsServer.start(DNS_PORT, "*", apIP);

    // Web server
    if (_captiveServer != nullptr) {
        delete _captiveServer;
    }
    _captiveServer = new WebServer(80);

    _captiveServer->on("/", HTTP_GET, [this]() { handleCaptiveRoot(); });
    _captiveServer->on("/save", HTTP_POST, [this]() { handleCaptiveSubmit(); });
    _captiveServer->onNotFound([this]() { handleCaptiveRoot(); }); // Redirect mọi path

    _captiveServer->begin();
    Serial.println(F("[Network] Captive Portal started. Waiting for user input..."));

    // Block cho đến khi người dùng nhập xong
    while (!_provisioningDone) {
        dnsServer.processNextRequest();
        _captiveServer->handleClient();
        delay(10);
    }

    // Dọn dẹp
    _captiveServer->stop();
    delete _captiveServer;
    _captiveServer = nullptr;
    dnsServer.stop();
    WiFi.softAPdisconnect(true);

    // Lưu credentials vào NVS
    if (_configMgr != nullptr) {
        _configMgr->saveWiFi(_provisionedSsid.c_str(), _provisionedPass.c_str());
    }

    Serial.printf("[Network] Provisioning done! SSID=%s\n", _provisionedSsid.c_str());
}

bool NetworkHandler::isProvisioningDone() {
    return _provisioningDone;
}

void NetworkHandler::handleCaptiveRoot() {
    _captiveServer->send(200, "text/html", buildCaptivePortalHTML());
}

void NetworkHandler::handleCaptiveSubmit() {
    if (_captiveServer->hasArg("ssid") && _captiveServer->hasArg("pass")) {
        _provisionedSsid = _captiveServer->arg("ssid");
        _provisionedPass = _captiveServer->arg("pass");

        String response = "<html><body style='font-family:sans-serif;text-align:center;"
                          "padding:40px;background:#1a1a2e;color:#eee;'>"
                          "<h2>&#10004; Da luu!</h2>"
                          "<p>Sendlove Box se ket noi Wi-Fi: <b>"
                          + _provisionedSsid +
                          "</b></p><p>Vui long doi...</p></body></html>";

        _captiveServer->send(200, "text/html", response);
        delay(1000); // Cho phép trình duyệt nhận response
        _provisioningDone = true;
    } else {
        _captiveServer->send(400, "text/plain", "Missing SSID or Password");
    }
}

String NetworkHandler::buildCaptivePortalHTML() {
    return String(
        "<!DOCTYPE html><html><head>"
        "<meta charset='UTF-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>Sendlove Box Setup</title>"
        "<style>"
        "body{font-family:'Segoe UI',sans-serif;background:#1a1a2e;color:#eee;"
        "display:flex;justify-content:center;align-items:center;min-height:100vh;margin:0;}"
        ".card{background:#16213e;border-radius:16px;padding:32px;width:320px;"
        "box-shadow:0 8px 32px rgba(0,0,0,0.4);}"
        "h2{text-align:center;color:#e94560;margin-bottom:24px;}"
        "label{display:block;margin-bottom:4px;font-size:14px;color:#aaa;}"
        "input[type=text],input[type=password]{width:100%;padding:12px;margin-bottom:16px;"
        "border:1px solid #333;border-radius:8px;background:#0f3460;color:#fff;"
        "font-size:16px;box-sizing:border-box;}"
        "input:focus{outline:none;border-color:#e94560;}"
        "button{width:100%;padding:14px;background:#e94560;color:#fff;border:none;"
        "border-radius:8px;font-size:16px;cursor:pointer;font-weight:bold;}"
        "button:hover{background:#c73651;}"
        ".logo{text-align:center;font-size:32px;margin-bottom:8px;}"
        "</style></head><body>"
        "<div class='card'>"
        "<div class='logo'>&#x1F49D;</div>"
        "<h2>Sendlove Box</h2>"
        "<form action='/save' method='POST'>"
        "<label>Wi-Fi Name (SSID)</label>"
        "<input type='text' name='ssid' placeholder='Ten mang Wi-Fi' required>"
        "<label>Password</label>"
        "<input type='password' name='pass' placeholder='Mat khau'>"
        "<button type='submit'>Ket noi</button>"
        "</form></div></body></html>"
    );
}

// ============================================================================
// Firebase API
// ============================================================================

MessageCheckResult NetworkHandler::checkFirebase(const char* boxId,
                                                  const char* firebaseHost) {
    MessageCheckResult result;
    HTTPClient http;

    // Đọc toàn bộ node của box
    String url = String("https://") + firebaseHost + "/boxes/" + boxId + ".json";
    http.begin(url);

    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("[Network] Firebase GET failed: %d\n", httpCode);
        http.end();
        return result;
    }

    String payload = http.getString();
    http.end();

    // Parse JSON
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        Serial.printf("[Network] JSON parse error: %s\n", err.c_str());
        return result;
    }

    // Messaging
    result.hasNewMessage = doc["messaging"]["has_new_msg"] | false;
    result.videoUrl      = doc["messaging"]["video_url"].as<String>();
    result.voiceUrl      = doc["messaging"]["voice_url"].as<String>();

    // Config
    result.alarmTime     = doc["config"]["alarm_time"].as<String>();
    result.timezone      = doc["config"]["timezone"] | 7;
    result.isAlarmActive = doc["config"]["is_alarm_active"] | false;

    // Wi-Fi config (nếu receiver đổi Wi-Fi từ web app)
    result.newWifiSsid   = doc["config"]["wifi_ssid"].as<String>();
    result.newWifiPass   = doc["config"]["wifi_pass"].as<String>();

    Serial.printf("[Network] Firebase check: hasMsg=%d, alarm=%s\n",
                  result.hasNewMessage, result.alarmTime.c_str());

    return result;
}

bool NetworkHandler::downloadFileToSD(const char* url, const char* sdPath) {
    if (_sdCard == nullptr) return false;

    HTTPClient http;
    http.begin(url);

    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("[Network] Download failed: %d\n", httpCode);
        http.end();
        return false;
    }

    int contentLen = http.getSize();
    Serial.printf("[Network] Downloading %d bytes → %s\n", contentLen, sdPath);

    // Mở file trên SD để ghi stream
    if (!_sdCard->openFileForWrite(sdPath)) {
        http.end();
        return false;
    }

    // Stream từ HTTP → SD card (buffer 1KB)
    WiFiClient* stream = http.getStreamPtr();
    uint8_t buffer[1024];
    size_t totalWritten = 0;

    while (http.connected() && (contentLen > 0 || contentLen == -1)) {
        size_t available = stream->available();
        if (available > 0) {
            size_t toRead = min(available, sizeof(buffer));
            size_t bytesRead = stream->readBytes(buffer, toRead);

            size_t written = _sdCard->appendChunk(buffer, bytesRead);
            totalWritten += written;

            if (contentLen > 0) {
                contentLen -= bytesRead;
            }
        }
        delay(1); // Yield cho FreeRTOS
    }

    _sdCard->closeWriteFile();
    http.end();

    Serial.printf("[Network] Download complete: %u bytes written\n", totalWritten);
    return true;
}

bool NetworkHandler::updateBoxStatus(const char* boxId, const char* firebaseHost,
                                      uint8_t batteryPercent) {
    HTTPClient http;

    String url = String("https://") + firebaseHost + "/boxes/" + boxId + "/status.json";
    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    // Tạo JSON payload
    JsonDocument doc;
    doc["battery_percent"]     = batteryPercent;
    doc["is_online"]           = true;
    doc["last_sync_timestamp"] = (unsigned long)time(nullptr);

    String payload;
    serializeJson(doc, payload);

    int httpCode = http.PUT(payload);
    http.end();

    if (httpCode == HTTP_CODE_OK) {
        Serial.printf("[Network] Status updated: battery=%d%%\n", batteryPercent);
        return true;
    }

    Serial.printf("[Network] Status update failed: %d\n", httpCode);
    return false;
}

bool NetworkHandler::markMessageRead(const char* boxId, const char* firebaseHost) {
    HTTPClient http;

    String url = String("https://") + firebaseHost + "/boxes/" + boxId
                 + "/messaging/has_new_msg.json";
    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    int httpCode = http.PUT("false");
    http.end();

    if (httpCode == HTTP_CODE_OK) {
        Serial.println(F("[Network] Message marked as read on Firebase"));
        return true;
    }
    return false;
}
