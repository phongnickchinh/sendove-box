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

// Minimum valid Unix time (2020-09-13). Below it the RTC has never been set —
// mbedTLS would reject certificates with BADCERT_FUTURE.
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

    // Get notified when SNTP sync succeeds
    sntp_set_time_sync_notification_cb(onNtpSyncCallback);

    // Timezone + SNTP servers via the Arduino API
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
    // Captive portal running: the "full WiFi restart" branch below switches to
    // WIFI_STA, tearing down the SoftAP serving the user -> the AP comes up then dies.
    if (isProvisioningActive()) {
        DLOG("[NET] ensureConnected skip: provisioning AP");
        return false;
    }

    // Do NOT trust WiFi.status() alone. After light sleep it VERY OFTEN still says
    // WL_CONNECTED although the association is dead on the AP side: the CPU slept
    // for 5 minutes, so the driver never processed beacon-loss/deauth events and
    // the status keeps its old value. Trusting it -> reconnect skipped -> every
    // http.GET() afterwards returns -1 ("WiFi connected but HTTP GET gives -1").
    // Worse: after a timer wake the box is only awake 2s, too short for the driver
    // to notice the lost beacons (~6s+) and auto-reconnect -> stuck for hundreds of
    // cycles. So right after waking ALWAYS re-associate instead of trusting old state.
    // Also check the IP: WL_CONNECTED only means associated + authenticated, not
    // that DHCP has handed out an IP; HTTP without an IP also returns -1.
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

    // WiFi.reconnect() is not enough: it relies on the very driver state that is
    // wrong. Drop the old association and begin() cleanly.
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

    // Synced within the last 60 seconds: skip
    if (_isTimeSynced && (millis() - _lastTimeSync < 60000)) {
        return true;
    }

    _isNtpSyncing = true;

    // Reset the callback flag and restart SNTP
    s_ntpSyncDone = false;
    configTzTime(TIMEZONE_ENV, NTP_SERVER_1, NTP_SERVER_2, NTP_SERVER_3);

    // Wait for the LWIP SNTP daemon to call onNtpSyncCallback()
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
    // Non-blocking read from internal ESP32 RTC (timeout = 0)
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

// Paths operating systems request to probe for internet access. Answering 302
// here makes the device report "sign-in required" and open the portal itself.
//   Android : /generate_204, /gen_204
//   Apple   : /hotspot-detect.html, /library/test/success.html
//   Windows : /connecttest.txt, /ncsi.txt, /redirect, /fwlink
// Do NOT return the content Windows expects (Microsoft NCSI...) on these paths:
// Windows would conclude the network has real internet.
static const char* const CAPTIVE_PROBE_PATHS[] = {
    "/generate_204", "/gen_204",
    "/hotspot-detect.html", "/library/test/success.html",
    "/connecttest.txt", "/ncsi.txt", "/redirect", "/fwlink"
};

void NetworkManager::startProvisioningAP(const char* apSsid, const char* apPassword) {
    // Mark provisioning as active FIRST. isProvisioningActive() relies on
    // _captiveServer being non-null; allocating it further down would leave a
    // window during AP setup where ensureConnected() thinks provisioning is off
    // and drags Wi-Fi back to STA mode.
    if (_captiveServer == nullptr) _captiveServer = new WebServer(80);
    _provisioningDone = false;

    // Without disabling auto-reconnect the ESP Wi-Fi layer keeps retrying the old
    // credentials and pulls the chip out of AP mode within seconds.
    WiFi.setAutoReconnect(false);
    WiFi.disconnect(true, true);
    delay(100);

    // AP_STA rather than AP: Wi-Fi scanning needs a live STA interface. Set the mode
    // AFTER the disconnect() above so STA comes up idle and doesn't connect by itself.
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(apSsid, apPassword);

    // Every DNS query resolves to the box's IP -> any domain opens the portal.
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
    // Unknown paths get the portal page directly (200). A 302 here would bounce
    // the portal page's own sub-requests around.
    _captiveServer->onNotFound([this]() { handleCaptiveRoot(); });

    _captiveServer->begin();

    // Scan once up front so the list is ready when the user opens the page.
    WiFi.scanNetworks(true, false);
}

void NetworkManager::handleCaptiveProbe() {
    if (!_captiveServer) return;
    String target = "http://" + WiFi.softAPIP().toString() + "/";
    _captiveServer->sendHeader("Location", target, true);
    _captiveServer->sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    _captiveServer->send(302, "text/plain", "");
}

// SSIDs may contain quotes and backslashes -> escape before embedding in JSON
// instead of concatenating raw.
static String jsonEscape(const String& s) {
    String out;
    out.reserve(s.length() + 8);
    for (size_t i = 0; i < s.length(); i++) {
        // unsigned: SSIDs are UTF-8; as signed char a byte > 127 goes negative
        // and gets wrongly stripped by the control-character branch below.
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

    // Deleting the results resets scanComplete() to -2, so the next poll starts a
    // fresh scan — the intended behaviour for the "rescan" button.
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

// ---------------------------------------------------------------------------
// Alarms on the portal. Runs in the NetworkController task (handleClient), so it
// only touches AlarmClock (which has its own mutex), never SPI/the display.
// ---------------------------------------------------------------------------

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
    // Accept the phone's time only while the box has no NTP time. NTP is more
    // trustworthy — and no arbitrary web page on the AP gets to move the clock back.
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
    // Lets the standby screen show real time instead of 00:00 (getTimeString reads
    // this flag). syncNtpTime() still runs once online and corrects it.
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

// ============================================================================
// Firebase REST API Integration (Wakeup Lifecycle Sync)
// ============================================================================

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

// Every WiFiClientSecure to Firebase must go through here. Calling setInsecure()
// at each call site would disable certificate checks entirely: anyone on the
// network path could read/alter traffic and grab FIREBASE_AUTH_SECRET.
//
// setCACert(), NOT setCACertBundle(): in Arduino core 2.0.17,
// arduino_esp_crt_bundle_attach() returns early with log_e("Failed to attach
// bundle") unless arduino_esp_crt_bundle_set() was called first — the Arduino
// wrapper does NOT embed a default bundle; you'd have to generate the blob with
// gen_crt_bundle.py. setCACert with an explicit PEM is the reliable path.
//
// Do NOT set setHandshakeTimeout() here (default 120s). Tightening it adds no
// security, and on a weak Wi-Fi link (associate + DHCP already take 3-8s, see
// MEMORY.md "Fix vong 4") it creates a failure that looks like a certificate
// error but isn't.
#ifndef FIREBASE_TLS_VERIFY
// Fail closed: if the flag goes missing (renamed, config.h not included), `#if`
// on an undefined macro silently evaluates to 0 -> falls back to setInsecure()
// unnoticed. Fail at compile time instead of silently allowing MITM.
#error "FIREBASE_TLS_VERIFY chua duoc dinh nghia (xem include/config.h)"
#endif

static void configureTlsClient(WiFiClientSecure& client) {
#if FIREBASE_TLS_VERIFY
    client.setCACert(FIREBASE_ROOT_CA);
#else
    client.setInsecure();
#endif
}

// The real mbedTLS error, read from the client after HTTPClient returned only a
// generic -1. Tells "invalid certificate" apart from "can't reach the server".
static void logTlsError(WiFiClientSecure& client, const char* where) {
    char buf[100] = "";
    int err = client.lastError(buf, sizeof(buf));
    if (err != 0) {
        DLOG("[NET] tls %s: %s", where, buf);
    }
}

// Appends the auth parameter to the END of _url: `?auth=<idToken>` in idToken
// mode, `?auth=<Database Secret>` in legacy mode. `sep` is the separator that
// fits the URL: '?' when it has no query yet, '&' when it already has parameters.
//
// Why not a header: measured with the box's own valid idToken (945 bytes,
// localId matching BOX_ID) on /boxes/<BOX_ID>/status.json:
//     Authorization: Bearer <idToken>    -> 401 "Unauthorized request."
//     Authorization: Firebase <idToken>  -> 401
//     ?auth=<idToken>                    -> 200
// RTDB accepts `Bearer` only for a service account's OAuth2 access token, not a
// Firebase idToken. See MEMORY.md §17. Don't change this back.
void NetworkManager::appendAuth(char sep) {
    size_t len = strlen(_url);
    if (len >= sizeof(_url)) return;
#if FIREBASE_USE_IDTOKEN
    if (_idToken[0] == '\0') return;
    int n = snprintf(_url + len, sizeof(_url) - len, "%cauth=%s", sep, _idToken);
#else
    int n = snprintf(_url + len, sizeof(_url) - len, "%cauth=%s", sep, FIREBASE_AUTH_SECRET);
#endif
    // A truncated token yields a 401 that looks EXACTLY like a rule rejection.
    // That ambiguity already caused one wrong conclusion (MEMORY.md §11). Be loud.
    if (n < 0 || (size_t)n >= sizeof(_url) - len) {
        DLOG("[NET] auth query BI CAT CUT (url %u)", (unsigned)strlen(_url));
    }
}

// How long without a new byte before the stream counts as dead.
// http.setTimeout(30000) only covers a single read, not the whole loop.
static const uint32_t DOWNLOAD_STALL_TIMEOUT_MS = 30000;

// The WakeSync task is PERMANENT: created once, then sleeps waiting for a notify.
//
// Creating and deleting a 12KB-stack task per sync cycle means allocating and
// freeing a contiguous 12KB block every few dozen seconds, right when mbedTLS
// needs a contiguous ~16KB block — the surest way to fragment the heap, i.e. the
// `SSL - Memory allocation` symptom in MEMORY.md §21. The stack is allocated once.
void NetworkManager::triggerWakeupSync(uint8_t batteryPercent, bool isCharging, IStorageProvider* storage) {
    if (isPlaybackActive()) {
        DLOG("[NET] sync skip: video playing");
        return;
    }

    // Two tasks of different priority (UIController=5, MediaPlayer=3) both call
    // this. With the read and write of _isSyncing as separate steps both could
    // slip through and start 2 syncs overwriting the same flash slot.
    bool claimed = false;
    portENTER_CRITICAL(&s_syncMux);
    if (!_isSyncing) {
        _isSyncing = true;
        claimed = true;
    }
    portEXIT_CRITICAL(&s_syncMux);
    if (!claimed) return;

    // Parameters travel via member variables, not an allocated pointer: only one
    // sync runs at a time (the _isSyncing flag above guarantees it).
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
        return;  // a freshly created task runs its first pass immediately, no notify needed
    }

    xTaskNotifyGive(_syncTask);
}

void NetworkManager::wakeupSyncTaskWorker(void* param) {
    NetworkManager* self = static_cast<NetworkManager*>(param);
    for (;;) {
        if (self != nullptr) {
            self->syncWakeup(self->_syncBattery, self->_syncCharging, self->_syncStorage);
        }
        // Wait for the next round. A notify arriving before this point makes
        // ulTaskNotifyTake return immediately (the count is kept), so none is lost.
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    }
}

bool NetworkManager::syncWakeup(uint8_t batteryPercent, bool isCharging, IStorageProvider* storage) {
    _isSyncing = true;

    // Keep the previous cycle's diagnostics (most useful when it failed or went to sleep early)
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

    // 0. Card absent (not mounted at boot, or the probe just reported it gone): try
    // to remount. Safe here because nothing is playing (checked above) and this
    // task is the only one opening download files. Once remounted, the later
    // steps (theme, music, messages) sync by themselves.
    if (SdStore::state() == SdStore::State::ABSENT) {
        SdStore::tryRemount();
    }

    // 1. Reconnect Wi-Fi. 12s rather than 5s: after light sleep this is a brand-new
    // associate + 4-way handshake + DHCP (see ensureConnected), 3-8s in practice.
    // Cutting at 5s aborts right before it completes.
    // No battery wasted: Task_UIController can't sleep while isSyncing().
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

    // Each TLS session to Firebase needs ~35-45KB of heap for the mbedTLS handshake
    // (in/out content buffers default to 16KB each way — NOT adjustable:
    // setBufferSizes() is an ESP8266 API that WiFiClientSecure on ESP32 lacks).
    // If http.GET() = -1 shows up while Wi-Fi is clearly alive, this is the first
    // number to look at to tell OOM from a link error.
    // maxblk = largest contiguous block: tells fragmentation from out-of-RAM (MEMORY.md §21).
    DLOG("[NET] sync start heap=%u maxblk=%u", (unsigned)ESP.getFreeHeap(),
         (unsigned)ESP.getMaxAllocHeap());

    // 2. Sync NTP time first so later timestamps are accurate
    syncNtpTime(5000);

    // Time gate — REQUIRED alongside setCACert(), not optional. With setInsecure()
    // a broken NTP still works. With VERIFY_REQUIRED, time(nullptr) ≈ 0 on a cold
    // boot -> mbedTLS returns BADCERT_FUTURE -> EVERY handshake fails. Dropping
    // this step turns "flaky NTP" into "cloud gone entirely".
    if (time(nullptr) < MIN_VALID_EPOCH) {
        // Clear the flag: syncNtpTime() short-circuits for 60s while _isTimeSynced
        // is set, so a plain retry would return true without requesting NTP again.
        _isTimeSynced = false;
        DLOG("[NET] time invalid -> NTP retry 15s");
        syncNtpTime(15000);
    }
    if (time(nullptr) < MIN_VALID_EPOCH) {
        // A DISTINCT marker, not to be confused with http.GET() = -1: no connection
        // has been opened yet. This line means an NTP failure, not TLS/network.
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

    // 2b. Get/renew the idToken BEFORE any Firebase call. After the time gate
    // because expiry is compared against time(nullptr) — with a wrong RTC a fresh
    // token would count as expired at once. Tokens live 1 hour, so most sync
    // cycles just reuse the one in RAM at no request cost.
    if (!ensureIdToken()) {
        DLOG("[NET] sync abort: khong lay duoc idToken");
        strncpy(_diagStep, "token_fail", sizeof(_diagStep) - 1);
        strncpy(_diagErr, "idtoken_fail", sizeof(_diagErr) - 1);
        _isSyncing = false;
        return false;
    }
    strncpy(_diagStep, "token_ok", sizeof(_diagStep) - 1);

    // 3. Check flags (alarms, OTA, pairing) FIRST so status carries the latest alarm data
    checkFirebaseFlags();
    vTaskDelay(pdMS_TO_TICKS(100));

    if (isPlaybackActive()) {
        strncpy(_diagErr, "playback_active", sizeof(_diagErr) - 1);
        _isSyncing = false;
        return false;
    }

    // 4. Update status (heartbeat + diagnostic telemetry pushed to the cloud)
    updateFirebaseStatus(batteryPercent, isCharging);
    // Log tail, ONLY on a new error (or first time after boot) -> usually costs nothing.
    pushLogTail();
    vTaskDelay(pdMS_TO_TICKS(100));

    if (isPlaybackActive()) {
        strncpy(_diagErr, "playback_active", sizeof(_diagErr) - 1);
        _isSyncing = false;
        return false;
    }

    // 4a. Theme (small ~150KB bundle, downloaded only when the flag is set / rev differs from the one in flash).
    if (!syncTheme(_themeFlag) || isPlaybackActive()) {
        strncpy(_diagErr, "playback_active", sizeof(_diagErr) - 1);
        _isSyncing = false;
        return false;
    }

    // 4b. Alarm music — BEFORE messages: alarms have a deadline, messages don't.
    if (!syncAlarmMusic() || isPlaybackActive()) {
        strncpy(_diagErr, "playback_active", sizeof(_diagErr) - 1);
        _isSyncing = false;
        return false;
    }

    // 5. Check and download new messages.
    //
    // The "slots full" gate belongs HERE, not around the whole cycle. Gating the
    // whole cycle makes a full box go completely silent: no heartbeat, no flag
    // reads, no alarm sync, no OTA or pairing flag. The intent is only "don't
    // waste a download when full" — which applies to THIS step alone.
    if (storage != nullptr) {
        if (storage->isFull()) {
            // _numOfNewMsg is ONLY assigned in checkAndDownloadNewMessages(), i.e.
            // BEHIND this gate. After a reset while full it is 0 with no way to
            // restore it -> the box says "No new messages" on touch and "slots
            // full" on sync, stuck forever because slots are only freed by reading.
            // Raise the floor every sync cycle.
            uint8_t unread = storage->getUnreadCount();
            if (_numOfNewMsg < unread) _numOfNewMsg = unread;
            // Log the unread count too: it tells "really full" (unread > 0) from
            // "SD card not mounted" (full with unread == 0: when !_mounted,
            // SDStorageProvider::isFull() returns true and getUnreadCount()
            // returns 0). The write cursor is clamped by writeIndexSafe(), so it
            // can't be the cause (see MEMORY.md §26).
            DLOG("[NET] msg skip: het slot, unread=%u", (unsigned)unread);
        } else {
            checkAndDownloadNewMessages(storage);
        }
    }

    strncpy(_diagStep, "sync_done", sizeof(_diagStep) - 1);
    _isSyncing = false;
    return true;
}

// ============================================================================
// Firebase Auth — the box's own idToken instead of the admin Database Secret
// ============================================================================
// Both endpoints below chain to GTS Root R4 (measured on identitytoolkit and
// securetoken) — already in firebase_root_ca.h, no extra certificate to embed.

// Firebase Storage is the OPPOSITE of RTDB: it DOES accept a header, with scheme
// "Firebase <idToken>" (not "Bearer"). Different service, different convention.
//
// NOT VERIFIED on a real device: Storage is currently open, so a garbage token
// also returns 200 and "accepted" can't be told from "not needed". Measurable
// only once storage.rules is deployed. This is the same trap that led to the
// wrong conclusion about RTDB headers (MEMORY.md §11) — don't repeat that reasoning.
//
// A String instead of a ~1.4KB stack buffer: TASK_STACK_NETWORK is only 6144 and
// this sits deep in the call chain. addHeader() takes const String&, so a
// temporary String exists anyway — declaring it explicitly costs nothing.
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
        // Dead token or rule rejection. Zero the expiry so the next sync cycle gets a
        // new token. No retry here: a token lives 1 hour and a sync cycle a few
        // seconds, so 401 almost always means a RULE rejection, not expiry —
        // retrying right away just costs another TLS handshake for the same 401.
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
    // 60s margin: a token with under a minute left counts as expired, so it can't
    // expire in the middle of a sync cycle.
    if (!force && _idToken[0] != '\0' && time(nullptr) < _idTokenExpiry - 60) {
        return true;
    }

    // A refresh token doesn't expire with time -> prefer it, so the password
    // isn't sent over the wire every time.
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
        // Refresh token broken (revoked / password changed) -> discard it and sign in from scratch.
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
// Reads idToken/refreshToken/expiresIn from the response into the caller's buffers.
// An ArduinoJson filter allocates ONLY the 3 fields needed — the
// signInWithPassword response also carries email/localId/kind..., and allocating
// it all wastes heap right before the next TLS handshake needs ~45KB.
// Takes a String, NOT a Stream (measured, see MEMORY.md §18): both identitytoolkit
// and securetoken answer "Transfer-Encoding: chunked" with no Content-Length.
// http.getStreamPtr() yields the RAW stream still carrying the hex chunk-size
// lines, e.g. "4a1\r\n{...}". ArduinoJson reads "4a1" -> parses "4" as a NUMBER,
// finishes SUCCESSFULLY, then doc["idToken"] = null -> "missing idToken" with no
// JSON error at all. Only http.getString() decodes chunked encoding.
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
        // Log the head of the response: without it "missing idToken" says nothing.
        // The first 60 chars only hold the "kind"/"error" part, never a token.
        DLOG("[NET] auth: thieu idToken; body=%s", body.substring(0, 60).c_str());
        return false;
    }
    // Firebase returns expiresIn as a STRING of seconds ("3600"), not a number.
    long ttl = (expires != nullptr) ? atol(expires) : 3600;
    if (ttl <= 0) ttl = 3600;

    // Store the RAW JWT (no "Bearer " prefix): RTDB doesn't accept the header —
    // the token goes into the `?auth=` query (see MEMORY.md §17).
    int n = snprintf(outToken, tokenLen, "%s", idTok);
    if (n < 0 || (size_t)n >= tokenLen) {
        // A truncated token = every later request 401s for no visible reason. Fail instead.
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

// The two auth endpoints differ only in URL, content type and field naming
// (camelCase vs snake_case); parseAuthResponse() handles both.
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
    // getString() (NOT getStreamPtr): the response is chunked — see
    // parseAuthResponse. end() right after reading to release the connection early.
    String resp = http.getString();
    http.end();

    // snakeCase = true: the securetoken endpoint uses id_token/refresh_token/expires_in
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
    // getString() (NOT getStreamPtr): the response is chunked — see
    // parseAuthResponse. end() right after reading to release the connection early.
    String resp = http.getString();
    http.end();

    // snakeCase = false: the identitytoolkit endpoint uses idToken/refreshToken/expiresIn
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

    // 640: a payload with full diag is ~480 bytes. On overflow snprintf truncates
    // SILENTLY -> broken JSON -> the PATCH is rejected and the heartbeat dies
    // (checked right below).
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
        // This is the FIRST TLS session of every sync cycle -> the cheapest place
        // to learn whether the handshake passes with setCACert() on.
        DLOG("[NET] status PATCH %d, heap=%u", httpCode, (unsigned)ESP.getFreeHeap());
        logTlsError(client, "status");
    } else {
        // DEBUG_SCREEN: only verifies Phase A (handshake passes, heap left after
        // parsing the 2 roots). Remove once settled.
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

    // The alarm flag is `a_flag` — the name the backend writes
    // (firebase-alarm.repository.ts); see MEMORY.md §20.
    // There are no OTA flags: nothing writes them (no backend, web or rules), and
    // remote-triggered OTA would keep the box awake 10 minutes for something that
    // may never come. OTA is its own mode, entered by the user with the touch-hold
    // sequence 3s, 3s, 6s (STATE_OTA in main.cpp).
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
    // Alarms changed (maybe a different track) or the music library changed -> refetch the music list.
    if (alarmFlag || musicFlag) _musicNeedFetch = true;

    // Reset flags BEFORE downloading: an edit from the web during the download
    // re-raises the flag and is caught next cycle. Resetting afterwards would wipe
    // that edit's flag. PATCH only when a flag is set — one per sync cycle would be
    // a wasted TLS handshake. Lowering config_flag here is safe even if the fetch
    // fails: _settingsNeedFetch keeps the retry.
    resetFlags(alarmFlag, configFlag, false, musicFlag);

    if (configFlag || _settingsNeedFetch) {
        _settingsNeedFetch = !syncFirebaseSettings();
    }

    // Two-way alarm sync (full rules in AlarmClock.h):
    //  - the box has unpushed edits -> PUT the whole list, ignore a_flag (box wins)
    //  - otherwise download when a_flag is set, or on the first sync after boot
    //    (NVS may be empty/stale, e.g. after flashing firmware that resized AlarmItem)
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
        // On a failed fetch keep _alarmsNeedFetch so the next cycle retries although the flag was reset.
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

// Box -> cloud: PUT replaces ALL of boxes/<id>/config/alarm_list. The rules let
// the box write exactly this branch (database.rules.json). a_flag is not raised:
// the box itself just wrote, so raising it would only make the next cycle
// download what was just pushed.
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
        // The box pushes the WHOLE list (box wins), so the music fields must be sent too,
        // or the PUT wipes the music/volume choice the recipient made on the web
        // (mandatory case, MEMORY.md §28).
        if (alarms[i].musicId[0]) a["music_id"] = alarms[i].musicId;
        a["volume"] = alarms[i].volume;
        a["ramp"] = alarms[i].ramp;
        // The web doesn't read created_at; written to match the backend schema (BaseModel).
        a["created_at"] = nowMs;
        a["updated_at"] = nowMs;
    }
    String body;
    serializeJson(doc, body);  // {} when empty -> RTDB deletes the node, i.e. "delete all"

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

// User settings. A key-range query orderBy="$key" from "config_rev" to
// "playback_volume": alphabetically exactly the 4 keys config_rev, display_brightness,
// led_state, playback_volume. One request, without pulling the Wi-Fi password
// (wifi_config), the alarm list (alarm_list) or the theme. orderBy="$key" needs no .indexOn.
//
// Do NOT use `?shallow=true`: it returns `true` for EVERY child key instead of numeric
// values -> is<int>() fails -> the box silently skips them and never receives
// brightness / volume.
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
        // is<int>() rejects odd values (string, null) -> -1 = keep the old value.
        if (doc["display_brightness"].is<int>()) bl = doc["display_brightness"].as<int>();
        if (doc["playback_volume"].is<int>()) vol = doc["playback_volume"].as<int>();
        rev = doc["config_rev"] | 0u;
    }
    // Nothing to change -> no NVS write (every boot passes through here).
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

    // "null" = the web deleted every alarm. Don't return early: the box would keep
    // the old list in NVS and still ring deleted alarms.
    if (payload != "null" && payload.length() > 2) {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, payload);
        if (err) return false;

        JsonObject obj = doc.as<JsonObject>();
        for (JsonPair kv : obj) {
            if (count >= MAX_ALARMS) break;
            JsonObject alarmObj = kv.value().as<JsonObject>();
            const char* tStr = alarmObj["time"] | "";
            // An id longer than the buffer would be truncated -> pushing it back
            // creates a different key. Skip and log instead of silently duplicating
            // the alarm in the cloud.
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

NetworkManager::DlResult NetworkManager::downloadFile(const char* storagePath, const char* dstPath,
                                                     uint32_t size, uint32_t crc) {
    SDCardManager* card = SdStore::card();
    if (!card || !storagePath || !dstPath || size == 0) return DlResult::FAILED;

    // Already complete from an earlier run (crc was checked at rename) -> skip. Happens when
    // a theme bundle is reinstalled, or a track already on the card is picked again.
    if (card->getFileSize(dstPath) == (int32_t)size) return DlResult::OK;

    char part[96];
    snprintf(part, sizeof(part), "%s.part", dstPath);

    int32_t have = card->getFileSize(part);
    if (have < 0) have = 0;
    if ((uint32_t)have > size) {  // .part of a different, longer version -> discard
        card->deleteFile(part);
        have = 0;
    }

    if ((uint32_t)have < size) {
        // Storage path -> download URL: every '/' must be encoded as %2F (like voice_url).
        String url = "https://firebasestorage.googleapis.com/v0/b/iot-app-839a2.firebasestorage.app/o/";
        for (const char* p = storagePath; *p; p++) {
            if (*p == '/') url += "%2F";
            else url += *p;
        }
        url += "?alt=media";

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
            append = false;  // server ignored Range -> download from the start
            have = 0;
        } else {
            DLOG("[NET] file GET %d: %s", code, dstPath);
            http.end();
            // 416 = .part already complete/mismatched -> discard, restart next time.
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
            // An alarm (with music) started ringing midway: stop, keep .part to resume later.
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
            // read(), NOT readBytes(): WiFiClientSecure doesn't override readBytes(), so it
            // falls back to Stream::readBytes() reading BYTE BY BYTE (see the message download loop below).
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
    // Task_MediaPlayer still has to install the bundle just downloaded: don't download again, wait.
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
    // A theme saved by an old web build has no theme_id/rev/assets: the box can't tell
    // what to download. The recipient saving the theme once more on the web fixes it.
    if (!id[0] || rev == 0 || strlen(id) > 20) {
        DLOG("[NET] theme cu, can luu lai tren web");
        _themeNeedFetch = false;
        if (themeFlag) resetFlags(false, false, true, false);
        return true;
    }

    char dir[48];
    snprintf(dir, sizeof(dir), "/theme/%s_r%lu", id, (unsigned long)rev);

    if (ThemeStore::valid() && ThemeStore::rev() == rev && strcmp(ThemeStore::themeId(), id) == 0) {
        // The copy in flash is current: only now lower the flag, and clean old bundles off the card.
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

    // Download each asset into the bundle directory. Size + crc32 are checked in downloadFile().
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
            return true;  // _themeNeedFetch stays set -> retried next cycle
        }
    }
    // layout.json = the theme verbatim from the cloud (widgets + asset names). Written AFTER
    // the assets: a bundle is only "complete" once it has layout.json (required by ThemeStore).
    char path[80];
    snprintf(path, sizeof(path), "%s/layout.json", dir);
    if (!SdStore::writeAtomic(path, (const uint8_t*)payload.c_str(), payload.length())) return true;

    ThemeStore::requestInstall(dir, id, rev);
    DLOG("[NET] theme %s rev %lu san sang, cho cai", id, (unsigned long)rev);
    return true;
}

bool NetworkManager::syncAlarmMusic() {
    if (!SdStore::card()) return true;  // no card: alarms just beep, nothing to do

    // Card just remounted: the index on a new card may differ entirely -> reload it, refetch the list.
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
    // A normal cycle (no flag, all tracks present) costs NO request.
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

    // Tracks removed from the library -> delete them from the card. Only after a successful list GET.
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
        if (m.isNull()) continue;  // alarm points at a deleted track: it beeps
        uint32_t rev = m["rev"] | 0u;
        uint32_t size = m["size"] | 0u;
        uint32_t crc = m["crc32"] | 0u;
        const char* sp = m["storage_path"] | "";
        if (MusicStore::has(need[i], rev)) continue;
        if (size == 0 || size > ALARM_MUSIC_MAX_BYTES || !sp[0]) continue;
        if (done >= ALARM_MUSIC_PER_SYNC) {
            _musicNeedFetch = true;  // tracks left to download -> continue next cycle
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
    serializeJson(doc, body);  // ArduinoJson escapes ", \ and newlines itself
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

// Downloads voice_url/bg_music_url and appends it to the slot just written (right
// after the image/video, or at offset 4 for an empty slot — see
// checkAndDownloadNewMessages()). Shared by the image/video path (audio is
// secondary, a failed download doesn't cancel the message) and the image-less
// "still message" path (audio may be the main content).
// Returns true when there is NO voice URL (nothing to download, not an error) or
// the download completed; false when a URL exists but it failed/fell short/stalled.
bool NetworkManager::downloadVoiceSegment(const String& rawVoiceUrl, WiFiClientSecure& client,
                                           IStorageProvider* storage, const char* writeSlotId) {
    if (rawVoiceUrl.length() == 0) return true;

    String voiceUrl = rawVoiceUrl;
    if (!voiceUrl.startsWith("http")) {
        if (voiceUrl.startsWith("gs://")) {
            int si = voiceUrl.indexOf('/', 5);
            if (si > 0) voiceUrl = voiceUrl.substring(si + 1);
        }
        if (voiceUrl.startsWith("/")) voiceUrl.remove(0, 1);
        voiceUrl.replace("/", "%2F");
        voiceUrl = "https://firebasestorage.googleapis.com/v0/b/iot-app-839a2.firebasestorage.app/o/" + voiceUrl + "?alt=media";
    }

    DLOG("[NET] voice/bg_music found, downloading...");
    HTTPClient httpAudio;
    bool ok = false;
    if (httpAudio.begin(client, voiceUrl.c_str())) {
        httpAudio.setTimeout(30000);
        // Required once storage.rules is locked down: without this header Storage returns 403.
        addStorageAuthHeader(httpAudio);
        int aCode = httpAudio.GET();
        noteAuthFailure(aCode, "voice");
        if (aCode == HTTP_CODE_OK) {
            int aLen = httpAudio.getSize();
            WiFiClient* aStream = httpAudio.getStreamPtr();

            // openForAppend continues writing without erasing the sectors
            // that already hold the video
            if (storage->openForAppend(writeSlotId)) {
                // Write the AUDC header (10 bytes)
                uint8_t audcHeader[10];
                memcpy(audcHeader, "AUDC", 4);
                uint16_t sr      = (uint16_t)AUDIO_SAMPLE_RATE;
                uint32_t pcmSize = (aLen > 0) ? (uint32_t)aLen : 0;
                memcpy(audcHeader + 4, &sr,      2);
                memcpy(audcHeader + 6, &pcmSize, 4);
                // With a short header write AudioPlayer won't match the
                // "AUDC" magic -> the video plays silently. Log it anyway,
                // or storage errors on the audio branch are invisible.
                bool aWriteError =
                    storage->writeChunk(audcHeader, sizeof(audcHeader)) < sizeof(audcHeader);
                if (aWriteError) {
                    DLOG("[NET] Audio hdr write SHORT");
                }

                // Stream PCM data into the slot (no closeWrite: metadata is unchanged).
                // 2048B rather than 256B — same reason as the video loop: a small
                // buffer + unconditional delay(1) caps the download speed and stalls easily.
                uint8_t abuf[2048];
                int     aTotalRead = 0;
                uint32_t aLastProgressMs = millis();
                while (!aWriteError && httpAudio.connected() && (aLen > 0 || aLen == -1)) {
                    size_t av = aStream->available();
                    if (av) {
                        size_t tr = (av < sizeof(abuf)) ? av : sizeof(abuf);
                        // read(), not readBytes() — full explanation at the video
                        // download loop in checkAndDownloadNewMessages().
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
                            // Hard cap — sets aWriteError (not writeError) so the
                            // half-written slot is rejected by this loop's own check.
                            if (aTotalRead > (int)MAX_MEDIA_BYTES) {
                                DLOG("[NET] audio dl ABORT: over cap %d", aTotalRead);
                                aWriteError = true;
                                break;
                            }
                        }
                    }
                    // Same infinite-hang risk as the video loop.
                    if (millis() - aLastProgressMs > DOWNLOAD_STALL_TIMEOUT_MS) {
                        DLOG("[NET] audio dl STALL %d bytes", aTotalRead);
                        // Must set aWriteError here, or the log says "OK" for a short
                        // download. For an image-less still message the audio may be
                        // the ONLY content, so it has to be reported accurately.
                        aWriteError = true;
                        break;
                    }
                    // Yield the CPU only when there is REALLY no data (see the video
                    // download loop in checkAndDownloadNewMessages()).
                    if (av == 0) {
                        delay(1);
                    }
                }
                // Commit the append session: writes audioSize to the slot
                // table. Without it the audio sits in storage but
                // AudioPlayer doesn't know where it is or how long ->
                // the box is silent.
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
        // auth goes LAST: orderBy/startAt already took the '?', so the auth parameter
        // is joined with '&'. True in both modes — Database Secret and idToken.
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
        // -1 = HTTPC_ERROR_CONNECTION_REFUSED: TCP/TLS connect failed. A plain retry
        // on the same client is nearly useless when the cause is a dead Wi-Fi link
        // or stale DNS — FORCE a re-association first, which also fetches a fresh
        // DNS server from DHCP.
        DLOG("[NET] msg GET %d, heap=%u -> re-assoc", httpCode, (unsigned)ESP.getFreeHeap());
        // "Invalid certificate" vs "can't reach the server": both are -1 at the
        // HTTPClient layer; only lastError tells them apart.
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

    // Parse JSON straight from the stream (no large temporary String fragmenting the heap)
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

    // Collect messages from a JsonObject or JsonArray (Firebase returns either, depending on the keys)
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

    // Keep msgList sorted by ascending timestamp (oldest -> newest)
    std::sort(msgList.begin(), msgList.end(), [](const JsonObject& a, const JsonObject& b) {
        uint64_t tsA = 0, tsB = 0;
        JsonVariantConst vA = a["timestamp"];
        if (!vA.isNull()) {
            if (vA.is<uint64_t>()) tsA = vA.as<uint64_t>();
            else if (vA.is<double>()) tsA = (uint64_t)vA.as<double>();
            else if (vA.is<const char*>()) tsA = strtoull(vA.as<const char*>(), nullptr, 10);
            else tsA = vA.as<uint64_t>();
        }
        JsonVariantConst vB = b["timestamp"];
        if (!vB.isNull()) {
            if (vB.is<uint64_t>()) tsB = vB.as<uint64_t>();
            else if (vB.is<double>()) tsB = (uint64_t)vB.as<double>();
            else if (vB.is<const char*>()) tsB = strtoull(vB.as<const char*>(), nullptr, 10);
            else tsB = vB.as<uint64_t>();
        }
        return tsA < tsB;
    });

    uint8_t unreadInMem = storage ? storage->getUnreadCount() : 0;
    uint32_t newCloudMsg = 0;
    for (JsonObject msg : msgList) {
        uint64_t ts = 0;
        JsonVariantConst tsVar = msg["timestamp"];
        if (!tsVar.isNull()) {
            if (tsVar.is<uint64_t>()) ts = tsVar.as<uint64_t>();
            else if (tsVar.is<double>()) ts = (uint64_t)tsVar.as<double>();
            else if (tsVar.is<const char*>()) ts = strtoull(tsVar.as<const char*>(), nullptr, 10);
            else ts = tsVar.as<uint64_t>();
        }
        if (ts == 0) {
            time_t nowSec = time(nullptr);
            ts = (nowSec > 1600000000) ? ((uint64_t)nowSec * 1000ULL) : (uint64_t)millis();
        }
        if (ts > lastTs) newCloudMsg++;
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

    // Keep the radio fully on for the whole download — the default modem sleep
    // (WIFI_PS_MIN_MODEM) adds latency/drops TCP window updates, contributing to
    // downloads breaking off at high speed. Power saving is re-enabled right after
    // the loop (every exit from the loop below is a `break`, never a `return`, so
    // that line is always reached).
    WiFi.setSleep(false);

    for (JsonObject msg : msgList) {
        uint64_t ts = 0;
        JsonVariantConst tsVar = msg["timestamp"];
        if (!tsVar.isNull()) {
            if (tsVar.is<uint64_t>()) {
                ts = tsVar.as<uint64_t>();
            } else if (tsVar.is<double>()) {
                ts = (uint64_t)tsVar.as<double>();
            } else if (tsVar.is<const char*>()) {
                ts = strtoull(tsVar.as<const char*>(), nullptr, 10);
            } else {
                ts = tsVar.as<uint64_t>();
            }
        }

        // No timestamp from Firebase (ts == 0): fall back to NTP time, or the millis() counter
        if (ts == 0) {
            time_t nowSec = time(nullptr);
            ts = (nowSec > 1600000000) ? ((uint64_t)nowSec * 1000ULL) : (uint64_t)millis();
        }

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

        // Accept every key naming variant (snake_case, camelCase...)
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

        // Voice URL (attached audio) — bg_music_url shares voice_url's download/append
        // path (both PCM/WAV, differing only in UX role: speech vs background music).
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

        // Caption text — check that the field EXISTS rather than rely on "type"
        // (known minor web bug: the text field isn't cleared when switching modes,
        // so a message of another type may still carry old text; reading the
        // actual field is the most accurate).
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
            String fullUrl = rawMediaUrl;
            
            // Convert a relative path or gs:// URL to a Firebase Storage HTTP download URL
            if (!fullUrl.startsWith("http")) {
                if (fullUrl.startsWith("gs://")) {
                    int slashIdx = fullUrl.indexOf('/', 5);
                    if (slashIdx > 0) fullUrl = fullUrl.substring(slashIdx + 1);
                }
                if (fullUrl.startsWith("/")) fullUrl.remove(0, 1);
                fullUrl.replace("/", "%2F"); // encode / as %2F
                fullUrl = "https://firebasestorage.googleapis.com/v0/b/iot-app-839a2.firebasestorage.app/o/" + fullUrl + "?alt=media";
            }

            _isDownloadingMedia = true;
            DLOG("[NET] Downloading media...");
            
            if (http.begin(client, fullUrl.c_str())) {
                http.setTimeout(30000);
                // Required once storage.rules is locked down: without this header Storage returns 403.
                addStorageAuthHeader(http);
                int code = http.GET();
                if (code < 0) logTlsError(client, "media");
                noteAuthFailure(code, "media");
                if (code == HTTP_CODE_OK) {
                    int len = http.getSize();
                    int initialLen = len;
                    int totalRead = 0;
                    // The heap figure most worth watching with setCACert() on: this
                    // TLS session lives longest (it spans the nested audio download)
                    // and has a history of OOM.
                    DLOG("[NET] GET OK len=%d heap=%u", len, (unsigned)ESP.getFreeHeap());
                    char writeSlotId[16] = "";
                    if (!storage->getNextWriteSlotIdentifier(writeSlotId, sizeof(writeSlotId))) {
                        DLOG("[NET] skip dl: FULL");
                        http.end();
                        _isDownloadingMedia = false;
                        _hasPendingMessages = true;
                        break; // stop when storage is full
                    }

                    WiFiClient* stream = http.getStreamPtr();
                    if (storage->openForWrite(writeSlotId)) {
                        DLOG("[NET] writing slot %s", writeSlotId);
                        // 2048B rather than 256B: a small buffer + unconditional delay(1)
                        // per iteration caps the download at ~20-25KB/s (256B drained
                        // per 10ms tick). Still rolling streaming: RAM use is constant
                        // whatever the file size.
                        uint8_t buffer[2048];
                        bool writeError = false;
                        // With len == -1 (chunked, no Content-Length) the loop condition
                        // never turns false by itself: it exits only when the server
                        // closes the connection. A half-open stream sending nothing
                        // would hang here forever. Bound it by time since real progress.
                        uint32_t lastProgressMs = millis();
                        int lastLoggedRead = 0;
                        while (http.connected() && (len > 0 || len == -1)) {
                            // An alarm (with music) started ringing: yield the card to the
                            // music stream (mandatory case, MEMORY.md §28). Treated as a
                            // failed download -> discardWrite();
                            // the message isn't marked downloaded, so the next cycle retries.
                            if (isPlaybackActive()) {
                                DLOG("[NET] dl dung: bao thuc dang keu");
                                writeError = true;
                                break;
                            }
                            size_t sizeAvail = stream->available();
                            if (sizeAvail) {
                                size_t toRead = (sizeAvail < sizeof(buffer)) ? sizeAvail : sizeof(buffer);
                                // read(), NOT readBytes(): WiFiClientSecure doesn't override
                                // readBytes(), so it falls back to Stream::readBytes() reading ONE
                                // BYTE AT A TIME (Stream.cpp:41), each byte calling available() ->
                                // mbedtls_ssl_read() twice. A 2048B chunk = ~4096 mbedTLS calls; a
                                // 2.2MB file = over 4 MILLION calls -> a ~20-25KB/s ceiling. Worse:
                                // when the mbedTLS buffer runs dry mid-chunk, timedRead() busy-spins
                                // for up to 30 SECONDS (Stream.cpp:31, _timeout = 30s) without
                                // yielding the CPU or draining the socket. read(buf,len) costs one
                                // available() + one mbedtls_ssl_read for the whole block.
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
                                    // Hard cap: stop now instead of downloading for minutes and failing elsewhere.
                                    if (totalRead > (int)MAX_MEDIA_BYTES) {
                                        DLOG("[NET] dl ABORT: over cap %d", totalRead);
                                        writeError = true;
                                        break;
                                    }
                                    // Log sparsely (every 16KB) so the loop's pace isn't affected.
                                    if (totalRead - lastLoggedRead >= 16384) {
                                        lastLoggedRead = totalRead;
                                        DLOG("[NET] dl %d/%d", totalRead, initialLen);
                                    }
                                }
                            }
                            if (millis() - lastProgressMs > DOWNLOAD_STALL_TIMEOUT_MS) {
                                DLOG("[NET] dl STALL %d/%d", totalRead, initialLen);
                                // writeError = true so the half-written slot is rejected by
                                // the check below. Without it a partial download with
                                // initialLen <= 0 would slip through -> a garbage slot.
                                writeError = true;
                                break;
                            }
                            // Yield the CPU only when there is REALLY no data — yielding
                            // unconditionally every iteration is the main cause of capped
                            // download speed and of stalls on a flaky network.
                            if (sizeAvail == 0) {
                                delay(1);
                            }
                        }
                        // Leaving the loop on !http.connected() (not a stall, not all bytes)
                        // = the connection dropped midway. Logged separately: the "DL err"
                        // line alone can't tell "connection dropped" from "short download".
                        if (!writeError && !http.connected() &&
                            (initialLen > 0 && totalRead < initialLen)) {
                            DLOG("[NET] conn DROPPED @ %d/%d", totalRead, initialLen);
                        }
                        // Settle the outcome BEFORE deciding to commit or discard. closeWrite()
                        // writes the slot table + sets the unread bit; calling it unconditionally
                        // would turn a mid-download stall/timeout into a "valid unread message"
                        // with truncated data -> it plays the first seconds (the 20-byte header
                        // is there) then fails with Bad jpegSize on reaching the unwritten area.
                        // On a real error -> discardWrite(): the slot table/unread bitmask stay
                        // untouched and _writeSlotIndex is kept, so the next sync retries this
                        // slot instead of burning a new one per failure.
                        bool downloadComplete = !writeError && (initialLen <= 0 || totalRead >= initialLen);
                        if (downloadComplete) {
                            storage->closeWrite(maxDisplayTime);
                        } else {
                            storage->discardWrite();
                        }

                        if (downloadComplete) {
                            DLOG("[NET] DL OK slot %s", writeSlotId);

                            // Close the video HTTP session BEFORE opening the audio one —
                            // downloadVoiceSegment() shares the same WiFiClientSecure, and a
                            // session not yet .end()ed can make the new one handshake in the
                            // wrong state. The second http.end() at the end of the block is a
                            // safe no-op.
                            http.end();

                            // Voice/bg_music is secondary to the image/video: a failed
                            // download is only logged, the message is not cancelled.
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
            // A still message with NO image/video: audio (voice/background music)
            // and/or text only. openForWrite()/closeWrite() are still needed to
            // create a valid "empty" slot (the dataSize=4 sentinel — matches how
            // NandStorageProvider::openForAppend() computes the next audio offset).
            // MediaPlayer recognises the sentinel and shows a black screen instead
            // of trying to decode a JPEG that doesn't exist.
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
                    // Here audio is the MAIN CONTENT (no image) — a failed download must
                    // cancel the whole message, unlike the image/video branch above.
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


        // Advance the timestamp ONLY when this message was fully processed
        if (messageSuccess) {
            if (ts > successfullyProcessedMaxTs) {
                successfullyProcessedMaxTs = ts;
            }
        } else {
            DLOG("[NET] ts fail");
            break; // stop here so message order is never skipped
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
