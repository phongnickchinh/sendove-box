#include "NetworkManager.h"
#include "captive_portal_html.h"
#include <DNSServer.h>
#include <time.h>
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
