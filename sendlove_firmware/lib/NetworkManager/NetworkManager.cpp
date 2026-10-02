#include "NetworkManager.h"
#include "AlarmClock.h"
#include "captive_portal_html.h"
#include <ArduinoJson.h>
#include "firebase_root_ca.h"
#include <DNSServer.h>
#include <time.h>
#include <vector>
#include <algorithm>
#include "esp_sntp.h"
#include "ScreenLogger.h"

// 2020-09-13. Below this the RTC is unset and mbedTLS rejects certs (BADCERT_FUTURE).
static constexpr time_t MIN_VALID_EPOCH = 1600000000;

static const byte DNS_PORT = 53;
static DNSServer dnsServer;
static volatile bool s_ntpSyncDone = false;

static void onNtpSyncCallback(struct timeval *tv) {
    s_ntpSyncDone = true;
}

void NetworkManager::init() {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.persistent(false);

    sntp_set_time_sync_notification_cb(onNtpSyncCallback);

    configTzTime(TIMEZONE_ENV, NTP_SERVER_1, NTP_SERVER_2, NTP_SERVER_3);
}

WiFiConnectResult NetworkManager::connectWiFi(const char* ssid, const char* password) {
    DLOG("[NET] connect: %s", ssid ? ssid : "(null)");
    if (ssid != nullptr) {
        strncpy(_wifiSsid, ssid, sizeof(_wifiSsid) - 1);
        _wifiSsid[sizeof(_wifiSsid) - 1] = '\0';
    }
    if (password != nullptr) {
        strncpy(_wifiPassword, password, sizeof(_wifiPassword) - 1);
        _wifiPassword[sizeof(_wifiPassword) - 1] = '\0';
    }

    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(_wifiSsid, _wifiPassword);

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start < WIFI_CONNECT_TIMEOUT_MS)) {
        delay(200);
    }

    if (WiFi.status() == WL_CONNECTED) {
        DLOG("[NET] WiFi OK");
        return WiFiConnectResult::CONNECTED;
    }
    DLOG("[NET] WiFi FAIL");
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

bool NetworkManager::ensureConnected(uint32_t timeoutMs) {
    // Reconnecting would switch to WIFI_STA and kill the portal's SoftAP.
    if (isProvisioningActive()) {
        DLOG("[NET] ensureConnected skip: provisioning AP");
        return false;
    }

    // After light sleep WiFi.status() can still say WL_CONNECTED though the AP dropped
    // us (the driver missed the deauth; a 2s timer wake is too short to auto-reconnect),
    // and every http.GET() then returns -1. So always re-associate after waking, and
    // require an IP: WL_CONNECTED doesn't mean DHCP is done.
    if (!_forceReassociate && WiFi.status() == WL_CONNECTED && (uint32_t)WiFi.localIP() != 0) {
        return true;
    }

    if (_wifiSsid[0] == '\0') {
        DLOG("[NET] no creds to reconnect");
        return false;
    }

    DLOG("[NET] re-assoc (wake=%d st=%d)", _forceReassociate ? 1 : 0, (int)WiFi.status());
    _forceReassociate = false;

    uint32_t start = millis();

    // WiFi.reconnect() trusts the same stale driver state: disconnect and begin() anew.
    WiFi.disconnect(false);
    delay(50);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(_wifiSsid, _wifiPassword);

    while (millis() - start < timeoutMs) {
        if (WiFi.status() == WL_CONNECTED && (uint32_t)WiFi.localIP() != 0) {
            DLOG("[NET] WiFi OK (%lums)", (unsigned long)(millis() - start));
            return true;
        }
        delay(100);
    }

    DLOG("[NET] WiFi FAIL %lums st=%d", (unsigned long)(millis() - start), (int)WiFi.status());
    return false;
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

bool NetworkManager::syncNtpTime(uint32_t timeoutMs) {
    if (WiFi.status() != WL_CONNECTED) {
        if (!ensureConnected(timeoutMs)) {
            DLOG("[NET] NTP skip: no wifi");
            return false;
        }
    }

    if (_isTimeSynced && (millis() - _lastTimeSync < 60000)) {
        return true;
    }

    _isNtpSyncing = true;

    s_ntpSyncDone = false;
    configTzTime(TIMEZONE_ENV, NTP_SERVER_1, NTP_SERVER_2, NTP_SERVER_3);

    uint32_t start = millis();
    while (!s_ntpSyncDone && (millis() - start < timeoutMs)) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    if (s_ntpSyncDone) {
        _isTimeSynced = true;
        _lastTimeSync = millis();
        time_t now = time(nullptr);
        struct tm ti;
        localtime_r(&now, &ti);
        DLOG("[NET] NTP OK: %02d:%02d:%02d", ti.tm_hour, ti.tm_min, ti.tm_sec);
    } else {
        DLOG("[NET] NTP timeout");
    }

    _isNtpSyncing = false;
    return s_ntpSyncDone;
}

void NetworkManager::getTimeString(char* buffer, size_t maxLen) const {
    if (buffer == nullptr || maxLen == 0) return;
    if (!_isTimeSynced) {
        snprintf(buffer, maxLen, "00:00");
        return;
    }

    struct tm timeinfo;
    // timeout 0: non-blocking
    if (!getLocalTime(&timeinfo, 0)) {
        snprintf(buffer, maxLen, "00:00");
        return;
    }

    strftime(buffer, maxLen, "%H:%M", &timeinfo);
}

int NetworkManager::getWifiRSSI() const {
    if (WiFi.status() != WL_CONNECTED) return -100;
    return WiFi.RSSI();
}

// OS connectivity-probe paths (Android, Apple, Windows). A 302 makes the device
// show "sign-in required" and open the portal. Do NOT serve the content Windows
// expects (NCSI): it would conclude there is real internet.
static const char* const CAPTIVE_PROBE_PATHS[] = {
    "/generate_204", "/gen_204",
    "/hotspot-detect.html", "/library/test/success.html",
    "/connecttest.txt", "/ncsi.txt", "/redirect", "/fwlink"
};

void NetworkManager::startProvisioningAP(const char* apSsid, const char* apPassword) {
    // Allocate FIRST: isProvisioningActive() checks _captiveServer, and ensureConnected()
    // must not drag Wi-Fi back to STA while the AP is coming up.
    if (_captiveServer == nullptr) _captiveServer = new WebServer(80);
    _provisioningDone = false;

    // Otherwise the ESP keeps retrying old credentials and drops out of AP mode.
    WiFi.setAutoReconnect(false);
    WiFi.disconnect(true, true);
    delay(100);

    // AP_STA: scanning needs a live STA. Set AFTER disconnect() so STA stays idle.
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(apSsid, apPassword);

    // Captive DNS: every name resolves to the box.
    dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

    _captiveServer->on("/", [this]() { handleCaptiveRoot(); });
    _captiveServer->on("/save", [this]() { handleCaptiveSubmit(); });
    _captiveServer->on("/scan", [this]() { handleCaptiveScan(); });
    _captiveServer->on("/alarms", HTTP_GET, [this]() { handleAlarmList(); });
    _captiveServer->on("/alarms/save", HTTP_POST, [this]() { handleAlarmSave(); });
    _captiveServer->on("/alarms/delete", HTTP_POST, [this]() { handleAlarmDelete(); });
    _captiveServer->on("/time", HTTP_POST, [this]() { handleSetTime(); });
    for (size_t i = 0; i < sizeof(CAPTIVE_PROBE_PATHS) / sizeof(CAPTIVE_PROBE_PATHS[0]); i++) {
        _captiveServer->on(CAPTIVE_PROBE_PATHS[i], [this]() { handleCaptiveProbe(); });
    }
    // 200, not 302: a redirect would bounce the portal page's own sub-requests.
    _captiveServer->onNotFound([this]() { handleCaptiveRoot(); });

    _captiveServer->begin();

    // Pre-scan so the list is ready when the page opens.
    WiFi.scanNetworks(true, false);
}

void NetworkManager::handleCaptiveProbe() {
    if (!_captiveServer) return;
    String target = "http://" + WiFi.softAPIP().toString() + "/";
    _captiveServer->sendHeader("Location", target, true);
    _captiveServer->sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    _captiveServer->send(302, "text/plain", "");
}

// SSIDs may contain quotes and backslashes.
static String jsonEscape(const String& s) {
    String out;
    out.reserve(s.length() + 8);
    for (size_t i = 0; i < s.length(); i++) {
        // unsigned: UTF-8 bytes > 127 must not fall into the control-char branch as negatives.
        unsigned char ch = (unsigned char)s[i];
        if (ch == '"' || ch == 0x5C) { out += (char)0x5C; out += (char)ch; }
        else if (ch < 0x20)            { out += ' '; }
        else                            { out += (char)ch; }
    }
    return out;
}

void NetworkManager::handleCaptiveScan() {
    if (!_captiveServer) return;

    int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_FAILED) {          // -2: never run (or results just deleted)
        WiFi.scanNetworks(true, false);
        _captiveServer->send(200, "application/json", "{\"status\":\"scanning\"}");
        return;
    }
    if (n == WIFI_SCAN_RUNNING) {         // -1: scan in progress
        _captiveServer->send(200, "application/json", "{\"status\":\"scanning\"}");
        return;
    }

    String json = "{\"status\":\"done\",\"nets\":[";
    bool first = true;
    for (int i = 0; i < n; i++) {
        String ssid = WiFi.SSID(i);
        if (ssid.length() == 0) continue;  // hidden network, can't be picked
        if (!first) json += ',';
        first = false;
        json += "{\"ssid\":\"" + jsonEscape(ssid) + "\",\"rssi\":" + String(WiFi.RSSI(i))
              + ",\"lock\":" + String(WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? 0 : 1) + "}";
    }
    json += "]}";

    // Resets scanComplete() to -2, so the next poll rescans (the "rescan" button).
    WiFi.scanDelete();
    _captiveServer->send(200, "application/json", json);
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

        strncpy(_wifiSsid, _provisionedSsid.c_str(), sizeof(_wifiSsid) - 1);
        _wifiSsid[sizeof(_wifiSsid) - 1] = '\0';
        strncpy(_wifiPassword, _provisionedPass.c_str(), sizeof(_wifiPassword) - 1);
        _wifiPassword[sizeof(_wifiPassword) - 1] = '\0';

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

// Portal alarm handlers. They run in the NetworkController task: touch only
// AlarmClock (own mutex), never SPI/the display.

static void sendJsonResult(WebServer* srv, int code, bool ok, const char* err = nullptr) {
    JsonDocument doc;
    doc["ok"] = ok;
    if (err) doc["err"] = err;
    String out;
    serializeJson(doc, out);
    srv->send(code, "application/json", out);
}

void NetworkManager::handleAlarmList() {
    if (!_captiveServer) return;
    AlarmItem alarms[MAX_ALARMS];
    size_t count = AlarmClock::instance().list(alarms, MAX_ALARMS);

    JsonDocument doc;
    time_t now = time(nullptr);
    doc["now"] = (now >= MIN_VALID_EPOCH) ? (int64_t)now : 0;
    doc["max"] = MAX_ALARMS;
    uint32_t rev = 0;
    doc["dirty"] = AlarmClock::instance().isDirty(&rev);
    JsonArray items = doc["items"].to<JsonArray>();
    for (size_t i = 0; i < count; i++) {
        JsonObject a = items.add<JsonObject>();
        a["id"] = alarms[i].id;
        a["time"] = alarms[i].time;
        a["en"] = alarms[i].isEnable;
        a["rep"] = alarms[i].repeatable;
    }
    String out;
    serializeJson(doc, out);
    _captiveServer->send(200, "application/json", out);
}

void NetworkManager::handleAlarmSave() {
    if (!_captiveServer) return;
    String id = _captiveServer->arg("id");
    String t = _captiveServer->arg("time");
    bool en = _captiveServer->arg("en") != "0";
    bool rep = _captiveServer->arg("rep") == "1";

    if (!AlarmClock::isValidTime(t.c_str())) {
        sendJsonResult(_captiveServer, 400, false, "Giờ không hợp lệ");
        return;
    }
    if (!AlarmClock::instance().upsert(id.c_str(), t.c_str(), en, rep)) {
        sendJsonResult(_captiveServer, 400, false,
                       id.length() ? "Không tìm thấy báo thức" : "Đã đủ 10 báo thức");
        return;
    }
    sendJsonResult(_captiveServer, 200, true);
}

void NetworkManager::handleAlarmDelete() {
    if (!_captiveServer) return;
    if (!AlarmClock::instance().remove(_captiveServer->arg("id").c_str())) {
        sendJsonResult(_captiveServer, 404, false, "Không tìm thấy báo thức");
        return;
    }
    sendJsonResult(_captiveServer, 200, true);
}

void NetworkManager::handleSetTime() {
    if (!_captiveServer) return;
    long long epoch = atoll(_captiveServer->arg("epoch").c_str());
    // Phone time only while there is no NTP time: no page on the AP may move the clock back.
    if (_isTimeSynced || time(nullptr) >= MIN_VALID_EPOCH) {
        sendJsonResult(_captiveServer, 200, true);
        return;
    }
    if (epoch < MIN_VALID_EPOCH) {
        sendJsonResult(_captiveServer, 400, false, "epoch");
        return;
    }
    struct timeval tv = { .tv_sec = (time_t)epoch, .tv_usec = 0 };
    settimeofday(&tv, nullptr);
    // Standby shows real time instead of 00:00; NTP corrects it once online.
    _isTimeSynced = true;
    DLOG("[NET] gio lay tu portal: %lld", epoch);
    sendJsonResult(_captiveServer, 200, true);
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

// ---- Firebase REST sync ----

#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "IStorageProvider.h"
#include "ConfigManager.h"
#include "DisplayDriver.h"
#include "Settings.h"
#include "SDCardManager.h"
#include "SdLog.h"
#include "SdStore.h"
#include "MusicStore.h"
#include "ThemeStore.h"

// Guards the claim of the _isSyncing flag between tasks of different priority.
static portMUX_TYPE s_syncMux = portMUX_INITIALIZER_UNLOCKED;

// Every WiFiClientSecure to Firebase goes through here (never setInsecure() at a
// call site). setCACert(), NOT setCACertBundle(): the Arduino core embeds no default
// bundle. Do NOT shorten setHandshakeTimeout(): on weak Wi-Fi it produces failures
// that look like certificate errors.
#ifndef FIREBASE_TLS_VERIFY
// Fail closed: `#if` on an undefined macro is 0, i.e. a silent setInsecure().
#error "FIREBASE_TLS_VERIFY chua duoc dinh nghia (xem include/config.h)"
#endif

static void configureTlsClient(WiFiClientSecure& client) {
#if FIREBASE_TLS_VERIFY
    client.setCACert(FIREBASE_ROOT_CA);
#else
    client.setInsecure();
#endif
}

// HTTPClient only returns -1; this logs the real mbedTLS error (bad cert vs unreachable).
static void logTlsError(WiFiClientSecure& client, const char* where) {
    char buf[100] = "";
    int err = client.lastError(buf, sizeof(buf));
    if (err != 0) {
        DLOG("[NET] tls %s: %s", where, buf);
    }
}

// Appends `auth=<idToken | Database Secret>` to _url; `sep` is '?' or '&'.
// A query, NOT a header: RTDB answers 401 to `Authorization: Bearer/Firebase <idToken>`
// (see MEMORY.md §17). Don't change this back.
void NetworkManager::appendAuth(char sep) {
    size_t len = strlen(_url);
    if (len >= sizeof(_url)) return;
#if FIREBASE_USE_IDTOKEN
    if (_idToken[0] == '\0') return;
    int n = snprintf(_url + len, sizeof(_url) - len, "%cauth=%s", sep, _idToken);
#else
    int n = snprintf(_url + len, sizeof(_url) - len, "%cauth=%s", sep, FIREBASE_AUTH_SECRET);
#endif
    // A truncated token gives a 401 that looks like a rule rejection (MEMORY.md §11): be loud.
    if (n < 0 || (size_t)n >= sizeof(_url) - len) {
        DLOG("[NET] auth query BI CAT CUT (url %u)", (unsigned)strlen(_url));
    }
}

// No byte for this long = dead stream (http.setTimeout covers one read only).
static const uint32_t DOWNLOAD_STALL_TIMEOUT_MS = 30000;

// WakeSync is created once and then waits for notifies: allocating and freeing a
// 12KB stack every cycle fragments the heap mbedTLS needs (MEMORY.md §21).
void NetworkManager::triggerWakeupSync(uint8_t batteryPercent, bool isCharging, IStorageProvider* storage) {
    if (isPlaybackActive()) {
        DLOG("[NET] sync skip: video playing");
        return;
    }

    // Called from two tasks (UIController, MediaPlayer): claim _isSyncing atomically,
    // or two syncs could write the same slot.
    bool claimed = false;
    portENTER_CRITICAL(&s_syncMux);
    if (!_isSyncing) {
        _isSyncing = true;
        claimed = true;
    }
    portEXIT_CRITICAL(&s_syncMux);
    if (!claimed) return;

    // Members suffice: only one sync runs at a time.
    _syncBattery  = batteryPercent;
    _syncCharging = isCharging;
    _syncStorage  = storage;

    if (_syncTask == nullptr) {
        BaseType_t res = xTaskCreate(wakeupSyncTaskWorker, "WakeSync", 12288, this, 2, &_syncTask);
        if (res != pdPASS) {
            _syncTask = nullptr;
            _isSyncing = false;
            DLOG("[NET] WakeSync task RAM!");
        }
        return;  // a new task runs its first pass right away
    }

    xTaskNotifyGive(_syncTask);
}

void NetworkManager::wakeupSyncTaskWorker(void* param) {
    NetworkManager* self = static_cast<NetworkManager*>(param);
    for (;;) {
        if (self != nullptr) {
            self->syncWakeup(self->_syncBattery, self->_syncCharging, self->_syncStorage);
        }
        // An earlier notify is kept in the count, so no round is lost.
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    }
}

bool NetworkManager::syncWakeup(uint8_t batteryPercent, bool isCharging, IStorageProvider* storage) {
    _isSyncing = true;

    // Keep the previous cycle's diagnostics
    strncpy(_prevWakeCause, _currentWakeCause, sizeof(_prevWakeCause) - 1);
    _prevWakeCause[sizeof(_prevWakeCause) - 1] = '\0';
    strncpy(_prevDiagStep, _diagStep, sizeof(_prevDiagStep) - 1);
    _prevDiagStep[sizeof(_prevDiagStep) - 1] = '\0';
    strncpy(_prevDiagErr, _diagErr, sizeof(_prevDiagErr) - 1);
    _prevDiagErr[sizeof(_prevDiagErr) - 1] = '\0';

    strncpy(_diagStep, "start", sizeof(_diagStep) - 1);
    strncpy(_diagErr, "none", sizeof(_diagErr) - 1);

    if (isPlaybackActive()) {
        strncpy(_diagErr, "playback_active", sizeof(_diagErr) - 1);
        _isSyncing = false;
        return false;
    }

    // 0. Card absent: try a remount. Safe here: nothing is playing and only this task
    // opens download files.
    if (SdStore::state() == SdStore::State::ABSENT) {
        SdStore::tryRemount();
    }

    // 1. Wi-Fi. 12s: a fresh associate + handshake + DHCP after light sleep takes 3-8s.
    uint32_t wifiStart = millis();
    if (!ensureConnected(12000)) {
        _lastWifiMs = millis() - wifiStart;
        DLOG("[NET] sync skip: no wifi");
        strncpy(_diagStep, "wifi_fail", sizeof(_diagStep) - 1);
        strncpy(_diagErr, "wifi_timeout", sizeof(_diagErr) - 1);
        _isSyncing = false;
        return false;
    }
    _lastWifiMs = millis() - wifiStart;
    strncpy(_diagStep, "wifi_ok", sizeof(_diagStep) - 1);

    if (isPlaybackActive()) {
        strncpy(_diagErr, "playback_active", sizeof(_diagErr) - 1);
        _isSyncing = false;
        return false;
    }

    // A TLS session needs ~35-45KB of heap (16KB buffers each way, not adjustable on
    // ESP32). http.GET() = -1 with Wi-Fi alive: check these numbers first. maxblk tells
    // fragmentation from out-of-RAM (MEMORY.md §21).
    DLOG("[NET] sync start heap=%u maxblk=%u", (unsigned)ESP.getFreeHeap(),
         (unsigned)ESP.getMaxAllocHeap());

    // 2. NTP
    syncNtpTime(5000);

    // Time gate, REQUIRED with setCACert(): with time ≈ 0 mbedTLS returns BADCERT_FUTURE
    // and every handshake fails.
    if (time(nullptr) < MIN_VALID_EPOCH) {
        // Clear the flag, or syncNtpTime() short-circuits for 60s without asking NTP again.
        _isTimeSynced = false;
        DLOG("[NET] time invalid -> NTP retry 15s");
        syncNtpTime(15000);
    }
    if (time(nullptr) < MIN_VALID_EPOCH) {
        // Distinct marker: an NTP failure, not TLS/network (no connection opened yet).
        DLOG("[NET] sync abort: time invalid");
        strncpy(_diagStep, "ntp_fail", sizeof(_diagStep) - 1);
        strncpy(_diagErr, "time_invalid", sizeof(_diagErr) - 1);
        _isSyncing = false;
        return false;
    }
    strncpy(_diagStep, "ntp_ok", sizeof(_diagStep) - 1);

    if (isPlaybackActive()) {
        strncpy(_diagErr, "playback_active", sizeof(_diagErr) - 1);
        _isSyncing = false;
        return false;
    }

    // 2b. idToken BEFORE any Firebase call, and after the time gate (expiry is compared
    // with time(nullptr)). Usually reused from RAM.
    if (!ensureIdToken()) {
        DLOG("[NET] sync abort: khong lay duoc idToken");
        strncpy(_diagStep, "token_fail", sizeof(_diagStep) - 1);
        strncpy(_diagErr, "idtoken_fail", sizeof(_diagErr) - 1);
        _isSyncing = false;
        return false;
    }
    strncpy(_diagStep, "token_ok", sizeof(_diagStep) - 1);

    // 3. Flags FIRST, so status carries the latest alarm data
    checkFirebaseFlags();
    vTaskDelay(pdMS_TO_TICKS(100));

    if (isPlaybackActive()) {
        strncpy(_diagErr, "playback_active", sizeof(_diagErr) - 1);
        _isSyncing = false;
        return false;
    }

    // 4. Status (heartbeat + diagnostics)
    updateFirebaseStatus(batteryPercent, isCharging);
    // Log tail: only on a new error or the first sync after boot.
    pushLogTail();
    vTaskDelay(pdMS_TO_TICKS(100));

    if (isPlaybackActive()) {
        strncpy(_diagErr, "playback_active", sizeof(_diagErr) - 1);
        _isSyncing = false;
        return false;
    }

    // 4a. Theme (only when flagged or the rev differs)
    if (!syncTheme(_themeFlag) || isPlaybackActive()) {
        strncpy(_diagErr, "playback_active", sizeof(_diagErr) - 1);
        _isSyncing = false;
        return false;
    }

    // 4b. Alarm music BEFORE messages: alarms have a deadline.
    if (!syncAlarmMusic() || isPlaybackActive()) {
        strncpy(_diagErr, "playback_active", sizeof(_diagErr) - 1);
        _isSyncing = false;
        return false;
    }

    // 5. Messages. The "slots full" gate belongs HERE only: gating the whole cycle
    // would silence heartbeat, flags and alarm sync on a full box.
    if (storage != nullptr) {
        if (storage->isFull()) {
            // _numOfNewMsg is only set behind this gate; after a reset while full it would
            // stay 0 forever ("No new messages" yet "slots full"). Raise the floor here.
            uint8_t unread = storage->getUnreadCount();
            if (_numOfNewMsg < unread) _numOfNewMsg = unread;
            // Full with unread == 0 means the SD card isn't mounted (MEMORY.md §26).
            DLOG("[NET] msg skip: het slot, unread=%u", (unsigned)unread);
        } else {
            checkAndDownloadNewMessages(storage);
        }
    }

    strncpy(_diagStep, "sync_done", sizeof(_diagStep) - 1);
    _isSyncing = false;
    return true;
}

// ---- Firebase Auth: the box's own idToken instead of the admin Database Secret ----
// Both auth endpoints chain to GTS Root R4 (already in firebase_root_ca.h).

// Storage, unlike RTDB, takes a header: "Firebase <idToken>" (measured on the real
// bucket, MEMORY.md §19).
// A String rather than a ~1.4KB stack buffer: TASK_STACK_NETWORK is only 6144.
void NetworkManager::addStorageAuthHeader(HTTPClient& http) {
#if FIREBASE_USE_IDTOKEN
    if (_idToken[0] != '\0') {
        String h = "Firebase ";
        h += _idToken;
        http.addHeader("Authorization", h);
    }
#else
    (void)http;
#endif
}

void NetworkManager::noteAuthFailure(int httpCode, const char* where) {
#if FIREBASE_USE_IDTOKEN
    if (httpCode == 401 || httpCode == 403) {
        // Zero the expiry so the next cycle gets a new token. No retry here: a 401 almost
        // always means a rule rejection, not expiry.
        DLOG("[NET] auth %d @ %s -> se lay token moi", httpCode, where);
        _idTokenExpiry = 0;
    }
#else
    (void)httpCode; (void)where;
#endif
}

bool NetworkManager::ensureIdToken(bool force) {
#if !FIREBASE_USE_IDTOKEN
    (void)force;
    return true;   // still on the Database Secret, no token needed
#else
    // 60s margin so the token can't expire mid-sync.
    if (!force && _idToken[0] != '\0' && time(nullptr) < _idTokenExpiry - 60) {
        return true;
    }

    // Prefer the refresh token: it doesn't expire and keeps the password off the wire.
    char refresh[FIREBASE_REFRESH_TOKEN_MAX_LEN] = "";
    bool haveRefresh = false;
    {
        ConfigManager cfg;
        if (cfg.init(NVS_NAMESPACE)) {
            haveRefresh = cfg.loadRefreshToken(refresh, sizeof(refresh));
            cfg.end();
        }
    }

    if (haveRefresh && authWithRefreshToken(refresh)) return true;

    if (haveRefresh) {
        // Refresh token revoked -> discard it, sign in from scratch.
        DLOG("[NET] refresh token hong -> dang nhap lai");
        ConfigManager cfg;
        if (cfg.init(NVS_NAMESPACE)) {
            cfg.clearRefreshToken();
            cfg.end();
        }
    }

    return authWithPassword();
#endif
}

#if FIREBASE_USE_IDTOKEN
// Parses idToken/refreshToken/expiresIn (filtered: only these 3 fields are allocated).
// Takes a String, NOT a Stream: both endpoints answer chunked, and the raw stream's
// hex chunk-size line parses as a number -> "missing idToken" with no JSON error
// (MEMORY.md §18). Only http.getString() decodes chunked encoding.
static bool parseAuthResponse(const String& body, char* outToken, size_t tokenLen,
                              time_t& outExpiry, char* outRefresh, size_t refreshLen,
                              bool snakeCase) {
    JsonDocument filter;
    filter[snakeCase ? "id_token"      : "idToken"]      = true;
    filter[snakeCase ? "refresh_token" : "refreshToken"] = true;
    filter[snakeCase ? "expires_in"    : "expiresIn"]    = true;

    JsonDocument doc;
    DeserializationError err =
        deserializeJson(doc, body, DeserializationOption::Filter(filter));
    if (err) {
        DLOG("[NET] auth JSON err: %s", err.c_str());
        return false;
    }

    const char* idTok   = doc[snakeCase ? "id_token"      : "idToken"];
    const char* refTok  = doc[snakeCase ? "refresh_token" : "refreshToken"];
    const char* expires = doc[snakeCase ? "expires_in"    : "expiresIn"];

    if (idTok == nullptr || idTok[0] == '\0') {
        // The first 60 chars hold only "kind"/"error", never a token.
        DLOG("[NET] auth: thieu idToken; body=%s", body.substring(0, 60).c_str());
        return false;
    }
    // expiresIn is a STRING of seconds ("3600").
    long ttl = (expires != nullptr) ? atol(expires) : 3600;
    if (ttl <= 0) ttl = 3600;

    // Raw JWT, no "Bearer " prefix: it goes into `?auth=` (MEMORY.md §17).
    int n = snprintf(outToken, tokenLen, "%s", idTok);
    if (n < 0 || (size_t)n >= tokenLen) {
        // A truncated token would 401 every later request: fail instead.
        DLOG("[NET] auth: idToken qua dai (%d)", n);
        outToken[0] = '\0';
        return false;
    }
    outExpiry = time(nullptr) + ttl;

    if (refTok != nullptr && refTok[0] != '\0') {
        strncpy(outRefresh, refTok, refreshLen - 1);
        outRefresh[refreshLen - 1] = '\0';
    } else {
        outRefresh[0] = '\0';
    }
    return true;
}

bool NetworkManager::authWithRefreshToken(const char* refreshToken) {
    WiFiClientSecure client;
    configureTlsClient(client);
    HTTPClient http;

    char url[160];
    snprintf(url, sizeof(url),
             "https://securetoken.googleapis.com/v1/token?key=%s", FIREBASE_API_KEY);

    if (!http.begin(client, url)) return false;
    http.setTimeout(FIREBASE_TIMEOUT_MS);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    char body[FIREBASE_REFRESH_TOKEN_MAX_LEN + 64];
    snprintf(body, sizeof(body), "grant_type=refresh_token&refresh_token=%s", refreshToken);

    int code = http.POST((uint8_t*)body, strlen(body));
    if (code != HTTP_CODE_OK) {
        DLOG("[NET] token refresh fail: %d", code);
        if (code < 0) logTlsError(client, "refresh");
        http.end();
        return false;
    }

    char newRefresh[FIREBASE_REFRESH_TOKEN_MAX_LEN] = "";
    // getString(): the response is chunked (see parseAuthResponse).
    String resp = http.getString();
    http.end();

    bool ok = parseAuthResponse(resp, _idToken,
                                sizeof(_idToken), _idTokenExpiry,
                                newRefresh, sizeof(newRefresh), true);

    if (ok && newRefresh[0] != '\0') {
        ConfigManager cfg;
        if (cfg.init(NVS_NAMESPACE)) {
            cfg.saveRefreshToken(newRefresh);
            cfg.end();
        }
    }
    if (ok) DLOG("[NET] token refreshed");
    return ok;
}

bool NetworkManager::authWithPassword() {
    WiFiClientSecure client;
    configureTlsClient(client);
    HTTPClient http;

    char url[190];
    snprintf(url, sizeof(url),
             "https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key=%s",
             FIREBASE_API_KEY);

    if (!http.begin(client, url)) return false;
    http.setTimeout(FIREBASE_TIMEOUT_MS);
    http.addHeader("Content-Type", "application/json");

    char body[256];
    snprintf(body, sizeof(body),
             "{\"email\":\"%s\",\"password\":\"%s\",\"returnSecureToken\":true}",
             BOX_AUTH_EMAIL, BOX_AUTH_PASSWORD);

    int code = http.POST((uint8_t*)body, strlen(body));
    if (code != HTTP_CODE_OK) {
        // 400 with PASSWORD_LOGIN_DISABLED = Email/Password not enabled in the Console.
        // 400 with EMAIL_NOT_FOUND / INVALID_PASSWORD = provision script not run, or
        // config_secrets.h filled in wrong.
        DLOG("[NET] signIn fail: %d", code);
        if (code < 0) logTlsError(client, "signin");
        else DLOG("[NET] signIn: %s", http.getString().substring(0, 80).c_str());
        http.end();
        return false;
    }

    char newRefresh[FIREBASE_REFRESH_TOKEN_MAX_LEN] = "";
    // getString(): the response is chunked (see parseAuthResponse).
    String resp = http.getString();
    http.end();

    bool ok = parseAuthResponse(resp, _idToken,
                                sizeof(_idToken), _idTokenExpiry,
                                newRefresh, sizeof(newRefresh), false);

    if (ok && newRefresh[0] != '\0') {
        ConfigManager cfg;
        if (cfg.init(NVS_NAMESPACE)) {
            cfg.saveRefreshToken(newRefresh);
            cfg.end();
        }
    }
    if (ok) DLOG("[NET] signIn OK");
    return ok;
}
#else
bool NetworkManager::authWithRefreshToken(const char*) { return false; }
bool NetworkManager::authWithPassword() { return false; }
#endif

bool NetworkManager::updateFirebaseStatus(uint8_t batteryPercent, bool isCharging) {
    WiFiClientSecure client;
    configureTlsClient(client);
    HTTPClient http;

    snprintf(_url, sizeof(_url), "https://%s/boxes/%s/status.json",
             FIREBASE_HOST, BOX_ID);
    appendAuth('?');

    if (!http.begin(client, _url)) return false;

    http.setTimeout(FIREBASE_TIMEOUT_MS);
    http.addHeader("Content-Type", "application/json");

    // 640: a full payload is ~480 bytes. A truncated one is broken JSON (checked below).
    char payload[640];
    uint32_t now = (uint32_t)time(nullptr);
    int plen = snprintf(payload, sizeof(payload),
             "{\"online\":true,\"battery\":%d,\"is_charging\":%s,\"last_seen\":%u,"
             "\"fw\":\"%s\",\"config_rev\":%lu,\"sd_state\":\"%s\",\"sd_free_mb\":%lu,"
             "\"music_n\":%u,\"theme_rev\":%lu,"
             "\"diag\":{"
             "\"wake\":\"%s\","
             "\"step\":\"%s\","
             "\"err\":\"%s\","
             "\"wifi_ms\":%u,"
             "\"a_flag\":%s,"
             "\"dirty\":%s,"
             "\"alm_cnt\":%u,"
             "\"flags_http\":%d,"
             "\"prev_wake\":\"%s\","
             "\"prev_step\":\"%s\","
             "\"prev_err\":\"%s\""
             "}}",
             batteryPercent, isCharging ? "true" : "false", now,
             FW_VERSION, (unsigned long)Settings::appliedRev.load(),
             SdStore::stateName(), (unsigned long)SdStore::freeMB(),
             (unsigned)MusicStore::count(), (unsigned long)ThemeStore::rev(),
             _currentWakeCause,
             _diagStep,
             _diagErr,
             (unsigned)_lastWifiMs,
             _lastAFlag ? "true" : "false",
             _lastAlarmsDirty ? "true" : "false",
             (unsigned)_lastAlarmsCount,
             _lastFlagsHttp,
             _prevWakeCause,
             _prevDiagStep,
             _prevDiagErr);
    if (plen < 0 || plen >= (int)sizeof(payload)) {
        DLOG("[NET] status payload BI CAT (%d)", plen);
        http.end();
        return false;
    }

    int httpCode = http.PATCH((uint8_t*)payload, strlen(payload));
    noteAuthFailure(httpCode, "status");
    if (httpCode < 0) {
        // First TLS session of the cycle: shows whether the handshake passes at all.
        DLOG("[NET] status PATCH %d, heap=%u", httpCode, (unsigned)ESP.getFreeHeap());
        logTlsError(client, "status");
    } else {
        // DEBUG_SCREEN: heap left after the handshake. Remove once settled.
        DLOG("[NET] status OK heap=%u", (unsigned)ESP.getFreeHeap());
    }
    http.end();

    return (httpCode == HTTP_CODE_OK);
}

bool NetworkManager::checkFirebaseFlags() {
    WiFiClientSecure client;
    configureTlsClient(client);
    HTTPClient http;

    snprintf(_url, sizeof(_url), "https://%s/boxes/%s/flags.json",
             FIREBASE_HOST, BOX_ID);
    appendAuth('?');

    if (!http.begin(client, _url)) {
        _lastFlagsHttp = -99;
        strncpy(_diagStep, "flags_begin_err", sizeof(_diagStep) - 1);
        strncpy(_diagErr, "begin_fail", sizeof(_diagErr) - 1);
        return false;
    }
    http.setTimeout(FIREBASE_TIMEOUT_MS);

    int httpCode = http.GET();
    _lastFlagsHttp = httpCode;
    noteAuthFailure(httpCode, "flags");
    if (httpCode != HTTP_CODE_OK) {
        DLOG("[NET] flags GET fail: %d", httpCode);
        if (httpCode < 0) logTlsError(client, "flags");
        http.end();
        strncpy(_diagStep, "flags_get_err", sizeof(_diagStep) - 1);
        snprintf(_diagErr, sizeof(_diagErr), "get_%d", httpCode);
        return false;
    }

    String payload = http.getString();
    http.end();

    // `a_flag` is the name the backend writes (MEMORY.md §20). There are no OTA
    // flags: OTA is a mode the user enters by touch (STATE_OTA in main.cpp).
    bool alarmFlag = false;
    bool configFlag = false;
    bool musicFlag = false;
    if (payload != "null" && payload.length() > 2) {
        JsonDocument doc;
        if (deserializeJson(doc, payload)) {
            strncpy(_diagStep, "flags_json_err", sizeof(_diagStep) - 1);
            strncpy(_diagErr, "json_fail", sizeof(_diagErr) - 1);
            return false;
        }
        alarmFlag = doc["a_flag"] | false;
        configFlag = doc["config_flag"] | false;
        musicFlag = doc["music_flag"] | false;
        _themeFlag = doc["theme_flag"] | false;
    }
    _lastAFlag = alarmFlag;
    // Alarms or the music library changed -> refetch the music list.
    if (alarmFlag || musicFlag) _musicNeedFetch = true;

    // Reset flags BEFORE downloading, so a web edit made meanwhile re-raises them.
    // A failed fetch is retried through the *NeedFetch members.
    resetFlags(alarmFlag, configFlag, false, musicFlag);

    if (configFlag || _settingsNeedFetch) {
        _settingsNeedFetch = !syncFirebaseSettings();
    }

    // Two-way alarm sync (rules in AlarmClock.h): unpushed box edits -> PUT the whole
    // list (box wins); otherwise download on a_flag or on the first sync after boot.
    uint32_t alarmRev = 0;
    bool isDirty = AlarmClock::instance().isDirty(&alarmRev);
    _lastAlarmsDirty = isDirty;
    if (isDirty) {
        if (pushFirebaseAlarms()) {
            AlarmClock::instance().markPushed(alarmRev);
            _alarmsNeedFetch = false;
            strncpy(_diagStep, "alm_push_ok", sizeof(_diagStep) - 1);
        } else {
            strncpy(_diagStep, "alm_push_fail", sizeof(_diagStep) - 1);
            strncpy(_diagErr, "put_fail", sizeof(_diagErr) - 1);
        }
    } else if (alarmFlag || _alarmsNeedFetch) {
        DLOG("[NET] flags: sync alarms");
        bool fetchOk = syncFirebaseAlarms();
        _alarmsNeedFetch = !fetchOk;
        if (fetchOk) {
            strncpy(_diagStep, "alm_fetch_ok", sizeof(_diagStep) - 1);
        } else {
            strncpy(_diagStep, "alm_fetch_fail", sizeof(_diagStep) - 1);
            strncpy(_diagErr, "fetch_fail", sizeof(_diagErr) - 1);
        }
    } else {
        strncpy(_diagStep, "alm_idle", sizeof(_diagStep) - 1);
    }

    AlarmItem alarmsTmp[MAX_ALARMS];
    _lastAlarmsCount = (uint8_t)AlarmClock::instance().list(alarmsTmp, MAX_ALARMS);

    return true;
}

// Box -> cloud: PUT replaces ALL of boxes/<id>/config/alarm_list. a_flag is not
// raised, or the next cycle would download what was just pushed.
bool NetworkManager::pushFirebaseAlarms() {
    AlarmItem alarms[MAX_ALARMS];
    size_t count = AlarmClock::instance().list(alarms, MAX_ALARMS);

    uint64_t nowMs = (uint64_t)time(nullptr) * 1000ULL;
    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();
    for (size_t i = 0; i < count; i++) {
        JsonObject a = root[alarms[i].id].to<JsonObject>();
        a["id"] = alarms[i].id;
        a["time"] = alarms[i].time;
        a["is_enable"] = alarms[i].isEnable;
        a["repeatable"] = alarms[i].repeatable;
        // Send the music fields too, or the PUT wipes the choice made on the web
        // (mandatory case, MEMORY.md §28).
        if (alarms[i].musicId[0]) a["music_id"] = alarms[i].musicId;
        a["volume"] = alarms[i].volume;
        a["ramp"] = alarms[i].ramp;
        // Matches the backend schema (BaseModel).
        a["created_at"] = nowMs;
        a["updated_at"] = nowMs;
    }
    String body;
    serializeJson(doc, body);  // {} -> RTDB deletes the node ("delete all")

    WiFiClientSecure client;
    configureTlsClient(client);
    HTTPClient http;

    snprintf(_url, sizeof(_url), "https://%s/boxes/%s/config/alarm_list.json",
             FIREBASE_HOST, BOX_ID);
    appendAuth('?');

    if (!http.begin(client, _url)) return false;
    http.setTimeout(FIREBASE_TIMEOUT_MS);
    http.addHeader("Content-Type", "application/json");
    int code = http.PUT(body);
    noteAuthFailure(code, "alarms put");
    if (code < 0) logTlsError(client, "alarms put");
    http.end();

    DLOG("[NET] alarms PUT %u -> %d", (unsigned)count, code);
    return code == HTTP_CODE_OK;
}

void NetworkManager::resetFlags(bool alarm, bool config, bool theme, bool music) {
    if (!alarm && !config && !theme && !music) return;

    char body[96] = "{";
    size_t n = 1;
    auto add = [&](bool on, const char* key) {
        if (!on) return;
        n += snprintf(body + n, sizeof(body) - n, "%s\"%s\":false", n > 1 ? "," : "", key);
    };
    add(alarm, "a_flag");
    add(config, "config_flag");
    add(theme, "theme_flag");
    add(music, "music_flag");
    snprintf(body + n, sizeof(body) - n, "}");

    WiFiClientSecure client;
    configureTlsClient(client);
    HTTPClient http;
    snprintf(_url, sizeof(_url), "https://%s/boxes/%s/flags.json", FIREBASE_HOST, BOX_ID);
    appendAuth('?');
    if (!http.begin(client, _url)) return;
    http.setTimeout(FIREBASE_TIMEOUT_MS);
    http.addHeader("Content-Type", "application/json");
    int pc = http.PATCH(body);
    noteAuthFailure(pc, "flags reset");
    http.end();
}

// User settings: a key-range query ("config_rev".."playback_volume") fetches just
// the 4 setting keys, without wifi_config, alarm_list or the theme.
// Do NOT use `?shallow=true`: it returns `true` instead of the values.
bool NetworkManager::syncFirebaseSettings() {
    WiFiClientSecure client;
    configureTlsClient(client);
    HTTPClient http;

    snprintf(_url, sizeof(_url),
             "https://%s/boxes/%s/config.json?orderBy=%%22%%24key%%22"
             "&startAt=%%22config_rev%%22&endAt=%%22playback_volume%%22",
             FIREBASE_HOST, BOX_ID);
    appendAuth('&');

    if (!http.begin(client, _url)) return false;
    http.setTimeout(FIREBASE_TIMEOUT_MS);
    int code = http.GET();
    noteAuthFailure(code, "settings");
    if (code != HTTP_CODE_OK) {
        DLOG("[NET] settings GET fail: %d", code);
        if (code < 0) logTlsError(client, "settings");
        http.end();
        return false;
    }
    String payload = http.getString();
    http.end();

    int bl = -1, vol = -1;
    uint32_t rev = 0;
    if (payload != "null" && payload.length() > 2) {
        JsonDocument doc;
        if (deserializeJson(doc, payload)) return false;
        // -1 = keep the old value
        if (doc["display_brightness"].is<int>()) bl = doc["display_brightness"].as<int>();
        if (doc["playback_volume"].is<int>()) vol = doc["playback_volume"].as<int>();
        rev = doc["config_rev"] | 0u;
    }
    // Nothing changed -> no NVS write.
    if (bl < 0 && vol < 0) return true;
    if (rev == Settings::appliedRev.load() && rev != 0 &&
        (bl < 0 || bl == (int)Settings::brightness.load()) &&
        (vol < 0 || vol == (int)Settings::volume.load())) {
        return true;
    }
    Settings::apply(bl, vol, rev);
    return true;
}

bool NetworkManager::syncFirebaseAlarms() {
    WiFiClientSecure client;
    configureTlsClient(client);
    HTTPClient http;

    snprintf(_url, sizeof(_url), "https://%s/boxes/%s/config/alarm_list.json",
             FIREBASE_HOST, BOX_ID);
    appendAuth('?');

    if (!http.begin(client, _url)) return false;
    http.setTimeout(FIREBASE_TIMEOUT_MS);

    int httpCode = http.GET();
    noteAuthFailure(httpCode, "alarms");
    if (httpCode != HTTP_CODE_OK) {
        DLOG("[NET] alarms GET fail: %d", httpCode);
        if (httpCode < 0) logTlsError(client, "alarms");
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();

    AlarmItem alarms[MAX_ALARMS];
    size_t count = 0;

    // "null" = every alarm deleted: don't return early, the old list must be cleared.
    if (payload != "null" && payload.length() > 2) {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, payload);
        if (err) return false;

        JsonObject obj = doc.as<JsonObject>();
        for (JsonPair kv : obj) {
            if (count >= MAX_ALARMS) break;
            JsonObject alarmObj = kv.value().as<JsonObject>();
            const char* tStr = alarmObj["time"] | "";
            // A truncated id would be pushed back as a new key (duplicate alarm): skip it.
            if (strlen(kv.key().c_str()) >= sizeof(alarms[count].id) ||
                !AlarmClock::isValidTime(tStr)) {
                DLOG("[NET] alarm bo qua: %s", kv.key().c_str());
                continue;
            }
            strncpy(alarms[count].id, kv.key().c_str(), sizeof(alarms[count].id) - 1);
            strncpy(alarms[count].time, tStr, sizeof(alarms[count].time) - 1);
            alarms[count].isEnable = alarmObj["is_enable"] | false;
            alarms[count].repeatable = alarmObj["repeatable"] | false;
            const char* mid = alarmObj["music_id"] | "";
            if (strlen(mid) < sizeof(alarms[count].musicId)) {
                strncpy(alarms[count].musicId, mid, sizeof(alarms[count].musicId) - 1);
            }
            int vol = alarmObj["volume"] | 80;
            alarms[count].volume = (uint8_t)(vol < 0 ? 0 : (vol > 100 ? 100 : vol));
            alarms[count].ramp = alarmObj["ramp"] | true;
            count++;
        }
    }

    AlarmClock::instance().replaceFromCloud(alarms, count);
    return true;
}

// Storage object path -> download URL ('/' encoded as %2F).
static String storageDownloadUrl(String path) {
    path.replace("/", "%2F");
    return "https://firebasestorage.googleapis.com/v0/b/iot-app-839a2.firebasestorage.app/o/" + path + "?alt=media";
}

// A message's media field: an absolute URL, gs://bucket/path or a bare storage path.
static String resolveMediaUrl(const String& raw) {
    if (raw.startsWith("http")) return raw;
    String path = raw;
    if (path.startsWith("gs://")) {
        int slashIdx = path.indexOf('/', 5);
        if (slashIdx > 0) path = path.substring(slashIdx + 1);
    }
    if (path.startsWith("/")) path.remove(0, 1);
    return storageDownloadUrl(path);
}

NetworkManager::DlResult NetworkManager::downloadFile(const char* storagePath, const char* dstPath,
                                                     uint32_t size, uint32_t crc) {
    SDCardManager* card = SdStore::card();
    if (!card || !storagePath || !dstPath || size == 0) return DlResult::FAILED;

    // Already on the card (crc was checked at rename).
    if (card->getFileSize(dstPath) == (int32_t)size) return DlResult::OK;

    char part[96];
    snprintf(part, sizeof(part), "%s.part", dstPath);

    int32_t have = card->getFileSize(part);
    if (have < 0) have = 0;
    if ((uint32_t)have > size) {  // .part of another version
        card->deleteFile(part);
        have = 0;
    }

    if ((uint32_t)have < size) {
        String url = storageDownloadUrl(storagePath);

        WiFiClientSecure client;
        configureTlsClient(client);
        HTTPClient http;
        if (!http.begin(client, url)) return DlResult::FAILED;
        http.setTimeout(30000);
        addStorageAuthHeader(http);
        if (have > 0) {
            char range[40];
            snprintf(range, sizeof(range), "bytes=%ld-", (long)have);
            http.addHeader("Range", range);
        }
        int code = http.GET();
        noteAuthFailure(code, "file");
        if (code < 0) logTlsError(client, "file");

        bool append;
        if (code == 206 && have > 0) {
            append = true;
        } else if (code == HTTP_CODE_OK) {
            append = false;  // server ignored Range
            have = 0;
        } else {
            DLOG("[NET] file GET %d: %s", code, dstPath);
            http.end();
            // 416: .part complete or mismatched -> restart next time.
            if (code == 416) card->deleteFile(part);
            return DlResult::FAILED;
        }

        DLOG("[NET] tai %s tu %ld/%lu", dstPath, (long)have, (unsigned long)size);
        if (!card->openGenWrite(part, append)) {
            http.end();
            SdStore::noteIoError();
            return DlResult::FAILED;
        }

        WiFiClient* stream = http.getStreamPtr();
        uint8_t buffer[2048];
        uint32_t got = (uint32_t)have;
        uint32_t lastData = millis();
        bool writeErr = false, aborted = false;
        while (got < size && http.connected()) {
            // An alarm started ringing: stop, keep .part to resume.
            if (isPlaybackActive()) {
                aborted = true;
                break;
            }
            size_t avail = stream->available();
            if (avail == 0) {
                if (millis() - lastData > DOWNLOAD_STALL_TIMEOUT_MS) break;
                vTaskDelay(pdMS_TO_TICKS(5));
                continue;
            }
            size_t want = avail < sizeof(buffer) ? avail : sizeof(buffer);
            if (want > size - got) want = size - got;
            // read(), NOT readBytes() (see the message download loop).
            int n = stream->read(buffer, want);
            if (n <= 0) continue;
            if (card->genWrite(buffer, (size_t)n) != (size_t)n) {
                writeErr = true;
                break;
            }
            got += (uint32_t)n;
            lastData = millis();
        }
        card->closeGenWrite();
        http.end();

        if (writeErr) {
            DLOG("[NET] ghi the FAIL @%lu: %s", (unsigned long)got, dstPath);
            SdStore::noteIoError();
            return DlResult::FAILED;
        }
        if (aborted) return DlResult::ABORTED;
        if (got < size) {
            DLOG("[NET] tai do %lu/%lu, lan sau tai tiep", (unsigned long)got, (unsigned long)size);
            return DlResult::FAILED;
        }
    }

    bool readOk = false;
    uint32_t actual = SdStore::crc32File(part, size, &readOk);
    if (!readOk || actual != crc) {
        DLOG("[NET] crc sai %s (%08lx != %08lx)", dstPath, (unsigned long)actual, (unsigned long)crc);
        card->deleteFile(part);
        return DlResult::FAILED;
    }
    card->deleteFile(dstPath);
    if (!card->renameFile(part, dstPath)) return DlResult::FAILED;
    SdStore::refreshFree();
    return DlResult::OK;
}

bool NetworkManager::syncTheme(bool themeFlag) {
    if (!SdStore::card() || !ThemeStore::partitionPresent()) return true;

    uint32_t epoch = SdStore::mountEpoch.load();
    if (epoch != _seenThemeEpoch) {
        _seenThemeEpoch = epoch;
        _themeNeedFetch = true;
    }
    // A downloaded bundle is waiting for Task_MediaPlayer to install it.
    if (ThemeStore::installPending()) return true;
    if (!themeFlag && !_themeNeedFetch) return true;

    WiFiClientSecure client;
    configureTlsClient(client);
    HTTPClient http;
    snprintf(_url, sizeof(_url), "https://%s/boxes/%s/config/theme.json", FIREBASE_HOST, BOX_ID);
    appendAuth('?');
    if (!http.begin(client, _url)) return true;
    http.setTimeout(FIREBASE_TIMEOUT_MS);
    int code = http.GET();
    noteAuthFailure(code, "theme");
    if (code != HTTP_CODE_OK) {
        DLOG("[NET] theme GET fail: %d", code);
        http.end();
        return true;
    }
    String payload = http.getString();
    http.end();

    if (payload == "null" || payload.length() <= 2) {  // no theme ever saved
        _themeNeedFetch = false;
        if (themeFlag) resetFlags(false, false, true, false);
        return true;
    }
    JsonDocument doc;
    if (deserializeJson(doc, payload)) return true;
    const char* id = doc["theme_id"] | "";
    uint32_t rev = doc["rev"] | 0u;
    // Theme from an old web build (no theme_id/rev/assets): saving it again on the web fixes it.
    if (!id[0] || rev == 0 || strlen(id) > 20) {
        DLOG("[NET] theme cu, can luu lai tren web");
        _themeNeedFetch = false;
        if (themeFlag) resetFlags(false, false, true, false);
        return true;
    }

    char dir[48];
    snprintf(dir, sizeof(dir), "/theme/%s_r%lu", id, (unsigned long)rev);

    if (ThemeStore::valid() && ThemeStore::rev() == rev && strcmp(ThemeStore::themeId(), id) == 0) {
        // Flash is current: lower the flag only now, and clean old bundles off the card.
        _themeNeedFetch = false;
        if (themeFlag) resetFlags(false, false, true, false);
        SDCardManager* card = SdStore::card();
        if (card) {
            card->listDir("/theme", [](const char* name, bool isDir, void* keep) {
                if (!isDir || strncmp(name, "t_", 2) != 0) return;
                char path[48];
                snprintf(path, sizeof(path), "/theme/%s", name);
                if (strcmp(path, static_cast<const char*>(keep)) != 0) SdStore::removeTree(path);
            }, dir);
        }
        return true;
    }

    static const struct { const char* key; const char* file; } ASSETS[] = {
        {"bg", "bg.bin"}, {"f_time", "f_time.vlw"}, {"f_date", "f_date.vlw"}};
    JsonObject assets = doc["assets"].as<JsonObject>();
    for (const auto& a : ASSETS) {
        JsonObject o = assets[a.key].as<JsonObject>();
        if (o.isNull()) continue;
        const char* sp = o["path"] | "";
        uint32_t size = o["size"] | 0u;
        uint32_t crc = o["crc32"] | 0u;
        if (!sp[0] || size == 0 || size > 200000) continue;
        char dst[80];
        snprintf(dst, sizeof(dst), "%s/%s", dir, a.file);
        DlResult r = downloadFile(sp, dst, size, crc);
        if (r == DlResult::ABORTED) return false;
        if (r != DlResult::OK) {
            DLOG("[NET] theme asset %s FAIL", a.key);
            return true;  // retried next cycle
        }
    }
    // layout.json is written LAST: its presence marks the bundle complete.
    char path[80];
    snprintf(path, sizeof(path), "%s/layout.json", dir);
    if (!SdStore::writeAtomic(path, (const uint8_t*)payload.c_str(), payload.length())) return true;

    ThemeStore::requestInstall(dir, id, rev);
    DLOG("[NET] theme %s rev %lu san sang, cho cai", id, (unsigned long)rev);
    return true;
}

bool NetworkManager::syncAlarmMusic() {
    if (!SdStore::card()) return true;  // no card: alarms just beep

    // Card remounted (maybe a different one): reload the index, refetch the list.
    uint32_t epoch = SdStore::mountEpoch.load();
    if (epoch != _seenMountEpoch) {
        _seenMountEpoch = epoch;
        MusicStore::load();
        _musicNeedFetch = true;
    }

    char need[ALARM_MUSIC_MAX_TRACKS][24];
    size_t nNeed = AlarmClock::instance().musicInUse(time(nullptr), need, ALARM_MUSIC_MAX_TRACKS);
    bool missing = false;
    for (size_t i = 0; i < nNeed; i++) {
        if (!MusicStore::has(need[i])) missing = true;
    }
    // A normal cycle costs no request.
    if (!_musicNeedFetch && !missing) return true;

    WiFiClientSecure client;
    configureTlsClient(client);
    HTTPClient http;
    snprintf(_url, sizeof(_url), "https://%s/boxes/%s/music.json", FIREBASE_HOST, BOX_ID);
    appendAuth('?');
    if (!http.begin(client, _url)) return true;
    http.setTimeout(FIREBASE_TIMEOUT_MS);
    int code = http.GET();
    noteAuthFailure(code, "music");
    if (code != HTTP_CODE_OK) {
        DLOG("[NET] music GET fail: %d", code);
        http.end();
        return true;
    }
    String payload = http.getString();
    http.end();

    JsonDocument doc;
    if (payload != "null" && payload.length() > 2 && deserializeJson(doc, payload)) return true;
    JsonObject lib = doc.as<JsonObject>();

    // Delete tracks removed from the library (only after a successful list GET).
    char keep[ALARM_MUSIC_MAX_TRACKS][24];
    size_t nKeep = 0;
    for (JsonPair kv : lib) {
        if (nKeep >= ALARM_MUSIC_MAX_TRACKS) break;
        if (strlen(kv.key().c_str()) >= sizeof(keep[0])) continue;
        strncpy(keep[nKeep], kv.key().c_str(), sizeof(keep[0]) - 1);
        keep[nKeep][sizeof(keep[0]) - 1] = '\0';
        nKeep++;
    }
    MusicStore::pruneExcept(keep, nKeep);
    _musicNeedFetch = false;

    uint8_t done = 0;
    for (size_t i = 0; i < nNeed; i++) {
        JsonObject m = lib[need[i]];
        if (m.isNull()) continue;  // deleted track: the alarm beeps
        uint32_t rev = m["rev"] | 0u;
        uint32_t size = m["size"] | 0u;
        uint32_t crc = m["crc32"] | 0u;
        const char* sp = m["storage_path"] | "";
        if (MusicStore::has(need[i], rev)) continue;
        if (size == 0 || size > ALARM_MUSIC_MAX_BYTES || !sp[0]) continue;
        if (done >= ALARM_MUSIC_PER_SYNC) {
            _musicNeedFetch = true;  // continue next cycle
            break;
        }
        char path[48];
        MusicStore::pathFor(need[i], path, sizeof(path));
        DlResult r = downloadFile(sp, path, size, crc);
        if (r == DlResult::ABORTED) {
            _musicNeedFetch = true;
            return false;
        }
        if (r == DlResult::OK) {
            MusicStore::put(need[i], rev, size, crc);
            DLOG("[NET] nhac %s OK", need[i]);
        } else {
            _musicNeedFetch = true;
        }
        done++;
    }
    return true;
}

void NetworkManager::pushLogTail() {
    static constexpr size_t MAX = 2200;
    char* tail = (char*)malloc(MAX);
    if (!tail) return;
    if (!SdLog::takeTail(tail, MAX)) {
        free(tail);
        return;
    }
    JsonDocument doc;
    doc["log_tail"] = (const char*)tail;
    doc["log_at"] = (uint32_t)time(nullptr);
    String body;
    serializeJson(doc, body);  // escapes ", \ and newlines
    free(tail);

    WiFiClientSecure client;
    configureTlsClient(client);
    HTTPClient http;
    snprintf(_url, sizeof(_url), "https://%s/boxes/%s/status.json", FIREBASE_HOST, BOX_ID);
    appendAuth('?');
    if (!http.begin(client, _url)) return;
    http.setTimeout(FIREBASE_TIMEOUT_MS);
    http.addHeader("Content-Type", "application/json");
    int code = http.PATCH(body);
    noteAuthFailure(code, "log");
    http.end();
}

// Downloads voice_url/bg_music_url and appends it to the slot just written.
// true = no URL (nothing to do) or fully downloaded; false = failed/short/stalled.
bool NetworkManager::downloadVoiceSegment(const String& rawVoiceUrl, WiFiClientSecure& client,
                                           IStorageProvider* storage, const char* writeSlotId) {
    if (rawVoiceUrl.length() == 0) return true;

    String voiceUrl = resolveMediaUrl(rawVoiceUrl);

    DLOG("[NET] voice/bg_music found, downloading...");
    HTTPClient httpAudio;
    bool ok = false;
    if (httpAudio.begin(client, voiceUrl.c_str())) {
        httpAudio.setTimeout(30000);
        // Without this header Storage returns 403 (storage.rules).
        addStorageAuthHeader(httpAudio);
        int aCode = httpAudio.GET();
        noteAuthFailure(aCode, "voice");
        if (aCode == HTTP_CODE_OK) {
            int aLen = httpAudio.getSize();
            WiFiClient* aStream = httpAudio.getStreamPtr();

            if (storage->openForAppend(writeSlotId)) {
                uint8_t audcHeader[10];
                memcpy(audcHeader, "AUDC", 4);
                uint16_t sr      = (uint16_t)AUDIO_SAMPLE_RATE;
                uint32_t pcmSize = (aLen > 0) ? (uint32_t)aLen : 0;
                memcpy(audcHeader + 4, &sr,      2);
                memcpy(audcHeader + 6, &pcmSize, 4);
                // A short header write means silent playback (no "AUDC" magic): log it.
                bool aWriteError =
                    storage->writeChunk(audcHeader, sizeof(audcHeader)) < sizeof(audcHeader);
                if (aWriteError) {
                    DLOG("[NET] Audio hdr write SHORT");
                }

                // Stream PCM into the slot. 2048B buffer: same reason as the video loop.
                uint8_t abuf[2048];
                int     aTotalRead = 0;
                uint32_t aLastProgressMs = millis();
                while (!aWriteError && httpAudio.connected() && (aLen > 0 || aLen == -1)) {
                    size_t av = aStream->available();
                    if (av) {
                        size_t tr = (av < sizeof(abuf)) ? av : sizeof(abuf);
                        // read(), NOT readBytes() (see the video download loop).
                        int c = aStream->read(abuf, tr);
                        if (c > 0) {
                            size_t aw = storage->writeChunk(abuf, c);
                            if (aw < (size_t)c) {
                                DLOG("[NET] audio write SHORT %u/%d @ %d",
                                     (unsigned)aw, c, aTotalRead);
                                aWriteError = true;
                                break;
                            }
                            aTotalRead += c;
                            if (aLen > 0) aLen -= c;
                            aLastProgressMs = millis();
                            // Hard cap
                            if (aTotalRead > (int)MAX_MEDIA_BYTES) {
                                DLOG("[NET] audio dl ABORT: over cap %d", aTotalRead);
                                aWriteError = true;
                                break;
                            }
                        }
                    }
                    if (millis() - aLastProgressMs > DOWNLOAD_STALL_TIMEOUT_MS) {
                        DLOG("[NET] audio dl STALL %d bytes", aTotalRead);
                        // A stall is a failure: the audio may be the message's only content.
                        aWriteError = true;
                        break;
                    }
                    // Yield only when there is no data (see the video download loop).
                    if (av == 0) {
                        delay(1);
                    }
                }
                // Records audioSize; without it AudioPlayer can't find the audio.
                storage->closeAppend();
                DLOG("[NET] Audio DL %s: %d bytes",
                     aWriteError ? "SHORT" : "OK", aTotalRead);
                ok = !aWriteError;
            } else {
                DLOG("[NET] Audio append FAIL (openForAppend)");
            }
        } else {
            DLOG("[NET] Audio DL fail: %d", aCode);
            if (aCode < 0) logTlsError(client, "audio");
        }
        httpAudio.end();
    }
    return ok;
}

// A message's "timestamp" (number or numeric string); 0 when missing.
static uint64_t messageTimestamp(JsonObjectConst msg) {
    JsonVariantConst v = msg["timestamp"];
    if (v.isNull()) return 0;
    if (v.is<uint64_t>()) return v.as<uint64_t>();
    if (v.is<double>()) return (uint64_t)v.as<double>();
    if (v.is<const char*>()) return strtoull(v.as<const char*>(), nullptr, 10);
    return v.as<uint64_t>();
}

// Same, but a message without a timestamp gets NTP time, else millis().
static uint64_t messageTimestampOrNow(JsonObjectConst msg) {
    uint64_t ts = messageTimestamp(msg);
    if (ts == 0) {
        time_t nowSec = time(nullptr);
        ts = (nowSec > MIN_VALID_EPOCH) ? ((uint64_t)nowSec * 1000ULL) : (uint64_t)millis();
    }
    return ts;
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
    configureTlsClient(client);
    HTTPClient http;

    uint64_t nextTs = (lastTs > 0) ? (lastTs + 1) : 0;
    if (nextTs > 0) {
        snprintf(_url, sizeof(_url),
                 "https://%s/messages/%s.json?orderBy=%%22timestamp%%22&startAt=%llu",
                 FIREBASE_HOST, BOX_ID, (unsigned long long)nextTs);
        appendAuth('&');
    } else {
        snprintf(_url, sizeof(_url),
                 "https://%s/messages/%s.json",
                 FIREBASE_HOST, BOX_ID);
        appendAuth('?');
    }

    if (!http.begin(client, _url)) {
        DLOG("[NET] HTTP msg init fail");
        return false;
    }
    http.setTimeout(FIREBASE_TIMEOUT_MS);

    int httpCode = http.GET();
    noteAuthFailure(httpCode, "msg");
    if (httpCode < 0) {
        // Connect failed: a plain retry is useless on a dead link or stale DNS, so
        // force a re-association first.
        DLOG("[NET] msg GET %d, heap=%u -> re-assoc", httpCode, (unsigned)ESP.getFreeHeap());
        logTlsError(client, "msg");
        _forceReassociate = true;
        if (ensureConnected(12000)) {
            httpCode = http.GET();
            DLOG("[NET] HTTP msg retry: %d", httpCode);
        }
    }

    if (httpCode != HTTP_CODE_OK) {
        DLOG("[NET] FB GET fail: %d", httpCode);
        http.end();
        return false;
    }

    WiFiClient* stream = http.getStreamPtr();
    if (!stream) {
        DLOG("[NET] HTTP stream NULL");
        http.end();
        return false;
    }

    // Parse from the stream: no large temporary String
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, *stream);
    http.end();

    if (err) {
        DLOG("[NET] JSON parse err: %s", err.c_str());
        return false;
    }

    if (doc.isNull()) {
        return true;
    }

    // Firebase returns an object or an array, depending on the keys
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

    // Oldest first
    std::sort(msgList.begin(), msgList.end(), [](const JsonObject& a, const JsonObject& b) {
        return messageTimestamp(a) < messageTimestamp(b);
    });

    uint8_t unreadInMem = storage ? storage->getUnreadCount() : 0;
    uint32_t newCloudMsg = 0;
    for (JsonObject msg : msgList) {
        if (messageTimestampOrNow(msg) > lastTs) newCloudMsg++;
    }
    _numOfNewMsg = unreadInMem + newCloudMsg;
    DLOG("[NET] msg: cloud=%d mem=%d new=%d", newCloudMsg, unreadInMem, _numOfNewMsg);

    if (newCloudMsg > 0) {
        _isDownloadingMedia = true;
    }

    if (msgList.empty()) {
        DLOG("[NET] no msgs in payload");
        _hasPendingMessages = false;
        _isDownloadingMedia = false;
        return true;
    }

    uint64_t successfullyProcessedMaxTs = lastTs;
    bool downloadedAnyMedia = false;
    _hasPendingMessages = false;

    // Modem sleep off while downloading (it drops TCP window updates). Re-enabled
    // after the loop, which is only ever left by `break`, never `return`.
    WiFi.setSleep(false);

    for (JsonObject msg : msgList) {
        uint64_t ts = messageTimestampOrNow(msg);
        if (ts <= lastTs) {
            continue;
        }

        uint32_t maxDisplayTime = 60;
        if (msg["max_display_time"].is<uint32_t>()) {
            maxDisplayTime = msg["max_display_time"].as<uint32_t>();
        } else if (msg["max_display_time"].is<const char*>()) {
            maxDisplayTime = atoi(msg["max_display_time"].as<const char*>());
            if (maxDisplayTime == 0) maxDisplayTime = 60;
        }

        // Accept every key naming variant
        String rawMediaUrl = "";
        const char* candidateKeys[] = {
            "bin_url", "binUrl", 
            "video_url", "videoUrl", 
            "image_url", "imageUrl", 
            "media_url", "mediaUrl", 
            "url"
        };
        for (const char* k : candidateKeys) {
            JsonVariantConst mediaValue = msg[k];
            if (!mediaValue.isNull()) {
                String val = mediaValue.as<String>();
                val.trim();
                if (val.length() > 0 && val != "null") {
                    rawMediaUrl = val;
                    break;
                }
            }
        }

        // Attached audio: voice and background music share one path
        String rawVoiceUrl = "";
        const char* voiceKeys[] = { "voice_url", "voiceUrl", "audio_url", "audioUrl", "bg_music_url", "bgMusicUrl" };
        for (const char* k : voiceKeys) {
            JsonVariantConst v = msg[k];
            if (!v.isNull()) {
                String val = v.as<String>();
                val.trim();
                if (val.length() > 0 && val != "null") {
                    rawVoiceUrl = val;
                    break;
                }
            }
        }

        // Caption: go by the field itself, not by "type"
        String rawText = "";
        {
            JsonVariantConst v = msg["text"];
            if (!v.isNull()) {
                String val = v.as<String>();
                val.trim();
                if (val.length() > 0 && val != "null") rawText = val;
            }
        }

        bool messageSuccess = false;

        if (rawMediaUrl.length() > 0) {
            String fullUrl = resolveMediaUrl(rawMediaUrl);

            _isDownloadingMedia = true;
            DLOG("[NET] Downloading media...");
            
            if (http.begin(client, fullUrl.c_str())) {
                http.setTimeout(30000);
                // Without this header Storage returns 403 (storage.rules).
                addStorageAuthHeader(http);
                int code = http.GET();
                if (code < 0) logTlsError(client, "media");
                noteAuthFailure(code, "media");
                if (code == HTTP_CODE_OK) {
                    int len = http.getSize();
                    int initialLen = len;
                    int totalRead = 0;
                    // Longest-lived TLS session (spans the audio download): watch this heap figure.
                    DLOG("[NET] GET OK len=%d heap=%u", len, (unsigned)ESP.getFreeHeap());
                    char writeSlotId[16] = "";
                    if (!storage->getNextWriteSlotIdentifier(writeSlotId, sizeof(writeSlotId))) {
                        DLOG("[NET] skip dl: FULL");
                        http.end();
                        _isDownloadingMedia = false;
                        _hasPendingMessages = true;
                        break;
                    }

                    WiFiClient* stream = http.getStreamPtr();
                    if (storage->openForWrite(writeSlotId)) {
                        DLOG("[NET] writing slot %s", writeSlotId);
                        // 2048B: a 256B buffer capped the download at ~20-25KB/s.
                        uint8_t buffer[2048];
                        bool writeError = false;
                        // len == -1 (chunked) never ends the loop by itself: bound it by
                        // time since the last byte.
                        uint32_t lastProgressMs = millis();
                        int lastLoggedRead = 0;
                        while (http.connected() && (len > 0 || len == -1)) {
                            // An alarm started ringing: yield the card to the music
                            // (mandatory case, MEMORY.md §28). Counts as a failed download.
                            if (isPlaybackActive()) {
                                DLOG("[NET] dl dung: bao thuc dang keu");
                                writeError = true;
                                break;
                            }
                            size_t sizeAvail = stream->available();
                            if (sizeAvail) {
                                size_t toRead = (sizeAvail < sizeof(buffer)) ? sizeAvail : sizeof(buffer);
                                // read(), NOT readBytes(): WiFiClientSecure falls back to
                                // Stream::readBytes(), which reads byte by byte (2 mbedTLS calls
                                // each, ~20-25KB/s ceiling) and can busy-spin for 30s in timedRead().
                                int c = stream->read(buffer, toRead);
                                if (c > 0) {
                                    size_t written = storage->writeChunk(buffer, c);
                                    if (written < (size_t)c) {
                                        writeError = true;
                                        break;
                                    }
                                    totalRead += c;
                                    if (len > 0) len -= c;
                                    lastProgressMs = millis();
                                    // Hard cap
                                    if (totalRead > (int)MAX_MEDIA_BYTES) {
                                        DLOG("[NET] dl ABORT: over cap %d", totalRead);
                                        writeError = true;
                                        break;
                                    }
                                    if (totalRead - lastLoggedRead >= 16384) {
                                        lastLoggedRead = totalRead;
                                        DLOG("[NET] dl %d/%d", totalRead, initialLen);
                                    }
                                }
                            }
                            if (millis() - lastProgressMs > DOWNLOAD_STALL_TIMEOUT_MS) {
                                DLOG("[NET] dl STALL %d/%d", totalRead, initialLen);
                                // A stall is a failure even when the length is unknown.
                                writeError = true;
                                break;
                            }
                            // Yield ONLY when there is no data: an unconditional delay caps
                            // the speed and causes stalls.
                            if (sizeAvail == 0) {
                                delay(1);
                            }
                        }
                        // Connection dropped midway: logged apart from a short download.
                        if (!writeError && !http.connected() &&
                            (initialLen > 0 && totalRead < initialLen)) {
                            DLOG("[NET] conn DROPPED @ %d/%d", totalRead, initialLen);
                        }
                        // Commit only a complete download: closeWrite() on a truncated one would
                        // publish it as a valid unread message ("Bad jpegSize" a few seconds in).
                        // discardWrite() leaves the slot table alone, so the next sync retries this slot.
                        bool downloadComplete = !writeError && (initialLen <= 0 || totalRead >= initialLen);
                        if (downloadComplete) {
                            storage->closeWrite(maxDisplayTime);
                        } else {
                            storage->discardWrite();
                        }

                        if (downloadComplete) {
                            DLOG("[NET] DL OK slot %s", writeSlotId);

                            // End the video session BEFORE the audio one: both share one
                            // WiFiClientSecure. The later http.end() is a no-op.
                            http.end();

                            // Audio is secondary here: a failed download doesn't cancel the message.
                            downloadVoiceSegment(rawVoiceUrl, client, storage, writeSlotId);
                            if (rawVoiceUrl.length() == 0) {
                                DLOG("[NET] No voice_url in msg");
                            }
                            if (rawText.length() > 0) {
                                storage->setItemText(writeSlotId, rawText.c_str());
                            }

                            messageSuccess = true;
                            downloadedAnyMedia = true;
                            if (_onDownloadComplete) {
                                _onDownloadComplete();
                            }
                        } else {
                            DLOG("[NET] DL err: %d/%d (discarded)", totalRead, initialLen);
                        }
                    } else {
                        DLOG("[NET] DL err: open slot %s", writeSlotId);
                    }
                } else {
                    DLOG("[NET] DL HTTP err: %d", code);
                }
                http.end();
            }
            _isDownloadingMedia = false;
        } else if (rawVoiceUrl.length() > 0 || rawText.length() > 0) {
            // Audio/text only, no image: openForWrite()/closeWrite() create an "empty" slot
            // (dataSize=4 sentinel) that MediaPlayer shows as a black screen.
            char writeSlotId[16] = "";
            if (!storage->getNextWriteSlotIdentifier(writeSlotId, sizeof(writeSlotId))) {
                DLOG("[NET] skip dl: FULL");
                _hasPendingMessages = true;
                break;
            }

            _isDownloadingMedia = true;
            if (storage->openForWrite(writeSlotId)) {
                storage->closeWrite(maxDisplayTime);
                bool audioOk = downloadVoiceSegment(rawVoiceUrl, client, storage, writeSlotId);
                if (audioOk) {
                    if (rawText.length() > 0) {
                        storage->setItemText(writeSlotId, rawText.c_str());
                    }
                    DLOG("[NET] DL OK slot %s (static, no image)", writeSlotId);
                    messageSuccess = true;
                    downloadedAnyMedia = true;
                    if (_onDownloadComplete) {
                        _onDownloadComplete();
                    }
                } else {
                    // Audio is the main content here: a failed download cancels the message.
                    DLOG("[NET] DL err: voice/bg_music failed (discarded)");
                    storage->discardWrite();
                }
            } else {
                DLOG("[NET] DL err: open slot %s (static)", writeSlotId);
            }
            _isDownloadingMedia = false;
        } else {
            messageSuccess = true;
        }


        // Advance the timestamp only for a fully processed message
        if (messageSuccess) {
            if (ts > successfullyProcessedMaxTs) {
                successfullyProcessedMaxTs = ts;
            }
        } else {
            DLOG("[NET] ts fail");
            break; // never skip ahead of a failed message
        }
    }

    WiFi.setSleep(true);

    if (successfullyProcessedMaxTs > lastTs) {
        if (cfg.init(NVS_NAMESPACE)) {
            cfg.saveLastDownloadTimestamp(successfullyProcessedMaxTs);
            cfg.end();
            DLOG("[NET] TS updated: %llu", (unsigned long long)successfullyProcessedMaxTs);
        }
    }

    return downloadedAnyMedia;
}
