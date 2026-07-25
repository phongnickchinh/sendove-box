#include "NetworkManager.h"
#include "captive_portal_html.h"
#include <DNSServer.h>
#include <time.h>
#include "esp_sntp.h"

static const byte DNS_PORT = 53;
static DNSServer dnsServer;

void NetworkManager::init() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

WiFiConnectResult NetworkManager::connectWiFi(const char* ssid, const char* password) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start < WIFI_CONNECT_TIMEOUT_MS)) {
        delay(500);
    }

    if (WiFi.status() == WL_CONNECTED) {
        syncTime();
        return WiFiConnectResult::CONNECTED;
    }
    return WiFiConnectResult::FAILED;
}

void NetworkManager::disconnectWiFi() {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    _isTimeSynced = false;
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

void NetworkManager::update() {
    if (_captiveServer != nullptr) {
        dnsServer.processNextRequest();
        _captiveServer->handleClient();
        return;
    }

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
    configTzTime("ICT-7", NTP_SERVER_1, NTP_SERVER_2, NTP_SERVER_3);

    struct tm timeinfo;
    _isTimeSynced = getLocalTime(&timeinfo, 10000);
}

String NetworkManager::getTimeString() const {
    if (!_isTimeSynced) return "00:00";

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 10)) return "00:00";

    char buffer[10];
    strftime(buffer, sizeof(buffer), "%H:%M", &timeinfo);
    return String(buffer);
}

String NetworkManager::getDateString() const {
    if (!_isTimeSynced) return "Loading...";

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 10)) return "Loading...";

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

void NetworkManager::handleCaptiveRoot() {
    if (_captiveServer) _captiveServer->send(200, "text/html", buildCaptivePortalHTML());
}

void NetworkManager::handleCaptiveSubmit() {
    if (!_captiveServer) return;

    if (_captiveServer->hasArg("ssid") && _captiveServer->hasArg("password")) {
        _provisionedSsid = _captiveServer->arg("ssid");
        _provisionedPass = _captiveServer->arg("password");
        _provisioningDone = true;

        String html = "<html><body><h2>Success!</h2><p>Connecting Wi-Fi...</p></body></html>";
        _captiveServer->send(200, "text/html", html);
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
