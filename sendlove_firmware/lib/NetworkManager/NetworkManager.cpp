#include "NetworkManager.h"
#include "captive_portal_html.h"
#include <DNSServer.h>
#include <time.h>
#include "esp_sntp.h"

static const byte DNS_PORT = 53;
static DNSServer dnsServer;

void NetworkManager::init() {
    Serial.println(F("[Network] Connecting to WiFi..."));
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

WiFiConnectResult NetworkManager::connectWiFi(const char* ssid, const char* password) {
    Serial.printf("[Network] Connecting to %s...\n", ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start < WIFI_CONNECT_TIMEOUT_MS)) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[Network] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
        syncTime();
        return WiFiConnectResult::CONNECTED;
    }

    Serial.println(F("[Network] Connection failed."));
    return WiFiConnectResult::FAILED;
}

void NetworkManager::disconnectWiFi() {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    _isTimeSynced = false;
    Serial.println(F("[Network] WiFi disconnected and powered down."));
}

bool NetworkManager::isReady() const {
    return (WiFi.status() == WL_CONNECTED) && _isTimeSynced;
}

bool NetworkManager::isConnected() const {
    return (WiFi.status() == WL_CONNECTED);
}

void NetworkManager::ensureConnected() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println(F("[Network] Restoring WiFi RF connection after sleep..."));
        WiFi.reconnect();
    }
}

void NetworkManager::update() {
    // SoftAP Captive Portal mode
    if (_captiveServer != nullptr) {
        dnsServer.processNextRequest();
        _captiveServer->handleClient();
        return;
    }

    // OTA WebServer mode (STA)
    if (_webServerRunning && _webServer != nullptr) {
        _webServer->handleClient();
    }

    if (WiFi.status() != WL_CONNECTED) {
        _isTimeSynced = false;
        return;
    }

    uint32_t now = millis();
    uint32_t syncInterval = _isTimeSynced ? 3600000 : 15000;

    if (now - _lastTimeSync >= syncInterval || _lastTimeSync == 0) {
        _lastTimeSync = now;
        syncTime();
    }
}

void NetworkManager::syncTime() {
    Serial.printf("[Network] WiFi Connected! IP: %s, RSSI: %d dBm\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
    Serial.println(F("[Network] Syncing NTP time..."));

    configTzTime("ICT-7", NTP_SERVER_1, NTP_SERVER_2, NTP_SERVER_3);

    struct tm timeinfo;
    // Chờ đồng bộ thời gian (timeout 10 giây) qua API getLocalTime chuẩn của ESP32
    if (getLocalTime(&timeinfo, 10000)) {
        _isTimeSynced = true;
        Serial.println(F("[Network] Time synced successfully!"));
        Serial.print(F("[Network] Current time: "));
        Serial.println(getTimeString());
    } else {
        _isTimeSynced = false;
        Serial.println(F("[Network] NTP sync timeout. Retrying in 15 seconds..."));
    }
}

String NetworkManager::getTimeString() const {
    if (!_isTimeSynced) return "00:00";

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 10)) {
        return "00:00";
    }

    char buffer[10];
    strftime(buffer, sizeof(buffer), "%H:%M", &timeinfo);
    return String(buffer);
}

String NetworkManager::getDateString() const {
    if (!_isTimeSynced) return "Loading...";

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 10)) {
        return "Loading...";
    }


    // Dùng ASCII không dấu vì font mặc định của LGFX (Roboto/FreeSans) chỉ hỗ trợ bộ ký tự ASCII
    const char* days[] = {"CHU NHAT", "THU HAI", "THU BA", "THU TU", "THU NAM", "THU SAU", "THU BAY"};

    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%s, %02d/%02d/%04d",
             days[timeinfo.tm_wday],
             timeinfo.tm_mday,
             timeinfo.tm_mon + 1,
             timeinfo.tm_year + 1900);

    return String(buffer);
}

int NetworkManager::getWifiRSSI() const {
    if (WiFi.status() != WL_CONNECTED) return -100;
    return WiFi.RSSI();
}

// --- SoftAP Provisioning ---

void NetworkManager::startProvisioningAP(const char* apSsid, const char* apPassword) {
    Serial.printf("[Network] Starting SoftAP: %s\n", apSsid);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(apSsid, apPassword);

    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

    if (_captiveServer == nullptr) {
        _captiveServer = new WebServer(80);
    }

    _captiveServer->on("/", [this]() { handleCaptiveRoot(); });
    _captiveServer->on("/save", [this]() { handleCaptiveSubmit(); });
    _captiveServer->onNotFound([this]() { handleCaptiveRoot(); });

    _captiveServer->begin();
    _provisioningDone = false;
    Serial.println(F("[Network] Captive Portal server started at 192.168.4.1"));
}

bool NetworkManager::isProvisioningDone() const {
    return _provisioningDone;
}

void NetworkManager::handleCaptiveRoot() {
    if (_captiveServer) {
        _captiveServer->send(200, "text/html", buildCaptivePortalHTML());
    }
}

void NetworkManager::handleCaptiveSubmit() {
    if (!_captiveServer) return;

    if (_captiveServer->hasArg("ssid") && _captiveServer->hasArg("password")) {
        _provisionedSsid = _captiveServer->arg("ssid");
        _provisionedPass = _captiveServer->arg("password");
        _provisioningDone = true;

        String html = "<html><body><h2>Cai dat thanh cong!</h2><p>Sendlove Box dang ket noi Wi-Fi...</p></body></html>";
        _captiveServer->send(200, "text/html", html);

        Serial.printf("[Network] Provisioned SSID: %s\n", _provisionedSsid.c_str());
    } else {
        _captiveServer->send(400, "text/plain", "Thieu SSID hoac Password!");
    }
}

String NetworkManager::buildCaptivePortalHTML() {
    return String(FPSTR(CAPTIVE_PORTAL_HTML));
}

// --- OTA Web Server (STA mode) ---

void NetworkManager::startWebServer(const char* hostname) {
    if (_webServerRunning) {
        Serial.println(F("[Network] WebServer already running."));
        return;
    }

    if (_webServer == nullptr) {
        _webServer = new WebServer(80);
    }

    // Khởi tạo mDNS
    if (MDNS.begin(hostname)) {
        Serial.printf("[Network] mDNS started: http://%s.local\n", hostname);
    } else {
        Serial.println(F("[Network] mDNS failed!"));
    }

    _webServer->begin();
    _webServerRunning = true;
    Serial.printf("[Network] WebServer started on port 80 (IP: %s)\n",
                  WiFi.localIP().toString().c_str());
}

void NetworkManager::stopWebServer() {
    if (_webServer != nullptr) {
        _webServer->stop();
        delete _webServer;
        _webServer = nullptr;
    }
    _webServerRunning = false;
    MDNS.end();
    Serial.println(F("[Network] WebServer stopped."));
}

WebServer* NetworkManager::getWebServer() {
    return _webServer;
}

bool NetworkManager::isWebServerRunning() const {
    return _webServerRunning;
}
