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

// Moc Unix hop le toi thieu (2020-09-13). Duoi moc nay nghia la RTC chua tung
// duoc set — mbedTLS se tu choi chung chi voi BADCERT_FUTURE.
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

    // Đăng ký callback chính thức nhận thông báo khi SNTP sync thành công
    sntp_set_time_sync_notification_cb(onNtpSyncCallback);

    // Cấu hình múi giờ + SNTP servers bằng Arduino API đã chứng minh hoạt động
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
    // Dang chay captive portal: nhanh "full WiFi restart" ben duoi chuyen sang
    // WIFI_STA, pha huy SoftAP dang phuc vu nguoi dung -> AP bat len roi tat.
    if (isProvisioningActive()) {
        DLOG("[NET] ensureConnected skip: provisioning AP");
        return false;
    }

    // KHONG duoc tin mot minh WiFi.status(). Sau Light Sleep no RAT HAY van bao
    // WL_CONNECTED du association da chet o phia AP: CPU ngu suot 5 phut nen driver
    // khong he xu ly duoc beacon-loss/deauth event, bien trang thai giu nguyen gia
    // tri cu. Tin vao no -> bo qua reconnect -> moi http.GET() sau do tra ve -1
    // (dung trieu chung user bao 2026-09-03: "wifi da connect nhung get http van
    // ra -1"). Te hon: sau timer wake box chi thuc 2s, chua du de driver tu phat
    // hien mat beacon (~6s+) roi auto-reconnect -> ket vinh vien qua hang tram chu ky.
    // Vi vay: vua ngu day thi LUON tai lap association, khong tin trang thai cu.
    // Ngoai ra kiem them IP: WL_CONNECTED chi nghia la da associate + auth xong,
    // chua chac da xin duoc IP tu DHCP; gui HTTP khi chua co IP cung ra -1.
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

    // WiFi.reconnect() khong du: no dua tren chinh trang thai driver dang sai.
    // Phai dut diem association cu roi begin() lai sach se.
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

    // Nếu đã sync gần đây (trong 60 giây qua), bỏ qua
    if (_isTimeSynced && (millis() - _lastTimeSync < 60000)) {
        return true;
    }

    _isNtpSyncing = true;

    // Reset cờ callback và kích hoạt lại SNTP qua Arduino API chính thức
    s_ntpSyncDone = false;
    configTzTime(TIMEZONE_ENV, NTP_SERVER_1, NTP_SERVER_2, NTP_SERVER_3);

    // Chờ callback onNtpSyncCallback() được gọi bởi LWIP SNTP daemon
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

// triggerNtpSync() + ntpTaskWorker() đã xoá 2026-09-18: không có lời gọi nào
// (chỉ khai báo trong header), NTP thật sự chạy bằng syncNtpTime() gọi thẳng
// trong syncWakeup(). Giữ lại chỉ tạo ảo giác có một đường NTP nền thứ hai.

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

    const char* days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

    snprintf(buffer, maxLen, "%s, %02d.%02d",
             days[timeinfo.tm_wday],
             timeinfo.tm_mday,
             timeinfo.tm_mon + 1);
}

int NetworkManager::getWifiRSSI() const {
    if (WiFi.status() != WL_CONNECTED) return -100;
    return WiFi.RSSI();
}

// Cac duong dan he dieu hanh goi de do "co internet khong". Tra 302 o day thi
// may bao "can dang nhap" va tu bung trang portal len.
//   Android : /generate_204, /gen_204
//   Apple   : /hotspot-detect.html, /library/test/success.html
//   Windows : /connecttest.txt, /ncsi.txt, /redirect, /fwlink
// Luu y: KHONG duoc tra dung noi dung Windows cho (Microsoft NCSI...) o cac
// duong dan nay, vi lam vay Windows se ket luan la mang co internet that.
static const char* const CAPTIVE_PROBE_PATHS[] = {
    "/generate_204", "/gen_204",
    "/hotspot-detect.html", "/library/test/success.html",
    "/connecttest.txt", "/ncsi.txt", "/redirect", "/fwlink"
};

void NetworkManager::startProvisioningAP(const char* apSsid, const char* apPassword) {
    // Danh dau dang provisioning NGAY tu dau. isProvisioningActive() dua vao
    // _captiveServer khac null; neu de viec cap phat xuong duoi thi trong luc
    // dung AP van con mot khe cua so ma ensureConnected() tuong la khong
    // provisioning va di keo Wi-Fi ve che do STA.
    if (_captiveServer == nullptr) _captiveServer = new WebServer(80);
    _provisioningDone = false;

    // Khong tat auto-reconnect thi lop Wi-Fi cua ESP van tu thu ket noi lai bang
    // credential cu va keo chip ra khoi che do AP chi sau vai giay.
    WiFi.setAutoReconnect(false);
    WiFi.disconnect(true, true);
    delay(100);

    // AP_STA chu khong phai AP: quet Wi-Fi can giao dien STA song. Dat mode SAU
    // disconnect() o tren de STA len o trang thai roi, khong tu di ket noi.
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(apSsid, apPassword);

    // Moi truy van DNS deu tra ve IP cua box -> go ten mien nao cung ra portal.
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
    // Giu nguyen cach cu: duong dan la tra thang trang portal (200). Doi sang
    // 302 o day se lam chinh cac request con cua trang portal bi day di lung tung.
    _captiveServer->onNotFound([this]() { handleCaptiveRoot(); });

    _captiveServer->begin();

    // Quet truoc mot lan de den luc nguoi dung mo trang thi danh sach da co san.
    WiFi.scanNetworks(true, false);
}

void NetworkManager::handleCaptiveProbe() {
    if (!_captiveServer) return;
    String target = "http://" + WiFi.softAPIP().toString() + "/";
    _captiveServer->sendHeader("Location", target, true);
    _captiveServer->sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    _captiveServer->send(302, "text/plain", "");
}

// SSID duoc phep chua dau nhay va dau gach nguoc -> phai escape truoc khi nhet
// vao JSON, khong noi chuoi tho.
static String jsonEscape(const String& s) {
    String out;
    out.reserve(s.length() + 8);
    for (size_t i = 0; i < s.length(); i++) {
        // unsigned: SSID la UTF-8, byte > 127 khi de char co dau se thanh am
        // va bi cat nham o nhanh ky tu dieu khien ben duoi.
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
    if (n == WIFI_SCAN_FAILED) {          // -2: chua chay lan nao (hoac vua xoa ket qua)
        WiFi.scanNetworks(true, false);
        _captiveServer->send(200, "application/json", "{\"status\":\"scanning\"}");
        return;
    }
    if (n == WIFI_SCAN_RUNNING) {         // -1: dang quet
        _captiveServer->send(200, "application/json", "{\"status\":\"scanning\"}");
        return;
    }

    String json = "{\"status\":\"done\",\"nets\":[";
    bool first = true;
    for (int i = 0; i < n; i++) {
        String ssid = WiFi.SSID(i);
        if (ssid.length() == 0) continue;  // mang an, khong bam chon duoc
        if (!first) json += ',';
        first = false;
        json += "{\"ssid\":\"" + jsonEscape(ssid) + "\",\"rssi\":" + String(WiFi.RSSI(i))
              + ",\"lock\":" + String(WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? 0 : 1) + "}";
    }
    json += "]}";

    // Xoa ket qua -> scanComplete() ve lai -2, nen lan poll sau se quet moi.
    // Do la hanh vi mong muon cho nut "Quet lai".
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
// Báo thức trên portal. Chạy trong task NetworkController (handleClient), nên
// chỉ đụng AlarmClock (có mutex riêng), không đụng SPI/màn hình.
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
    // Chỉ nhận giờ điện thoại khi hộp chưa có giờ NTP. Đã có NTP thì NTP đáng tin
    // hơn — và không cho một trang web bất kỳ trên AP kéo lùi đồng hồ.
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
    // Cho màn hình chờ hiện giờ thật thay vì 00:00 (getTimeString đọc cờ này).
    // syncNtpTime() khi lên mạng vẫn chạy và chỉnh lại cho chính xác.
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

// Bao ve buoc gianh quyen co _isSyncing giua cac task khac do uu tien.
static portMUX_TYPE s_syncMux = portMUX_INITIALIZER_UNLOCKED;

// MIN_VALID_EPOCH: da chuyen len dau file (cac handler bao thuc cua portal dung truoc).

// Moi WiFiClientSecure toi Firebase deu phai di qua day. Truoc kia moi cho tu
// goi setInsecure() -> tat hoan toan viec kiem tra chung chi, ai dung giua mang
// cung doc/sua duoc noi dung va lay duoc FIREBASE_AUTH_SECRET.
//
// Dung setCACert() chu KHONG phai setCACertBundle(): da doc source Arduino core
// 2.0.17, arduino_esp_crt_bundle_attach() return som voi log_e("Failed to attach
// bundle") neu chua goi arduino_esp_crt_bundle_set() truoc — wrapper Arduino
// KHONG nhung san bundle mac dinh, muon dung phai tu sinh blob bang
// gen_crt_bundle.py. setCACert voi PEM tuong minh la duong chac chan.
//
// KHONG dat setHandshakeTimeout() o day (mac dinh 120s). Siet ngan lai khong
// giup gi cho bao mat, ma link Wi-Fi yeu (associate + DHCP da ton 3-8s, xem
// MEMORY.md Fix vong 4) se de ra mot kieu fail trong giong loi cert nhung
// khong phai.
#ifndef FIREBASE_TLS_VERIFY
// Fail-closed: neu co bi mat (doi ten, thieu include config.h) thi `#if` cua mot
// macro chua dinh nghia am tham thanh 0 -> tu dong quay ve setInsecure() ma
// khong ai biet. Bat loi luc bien dich thay vi im lang ho MITM.
#error "FIREBASE_TLS_VERIFY chua duoc dinh nghia (xem include/config.h)"
#endif

static void configureTlsClient(WiFiClientSecure& client) {
#if FIREBASE_TLS_VERIFY
    client.setCACert(FIREBASE_ROOT_CA);
#else
    client.setInsecure();
#endif
}

// Loi mbedTLS that su, doc tu client sau khi HTTPClient chi tra ve -1 chung
// chung. Phan biet duoc "chung chi khong hop le" voi "khong noi duoc toi server".
static void logTlsError(WiFiClientSecure& client, const char* where) {
    char buf[100] = "";
    int err = client.lastError(buf, sizeof(buf));
    if (err != 0) {
        DLOG("[NET] tls %s: %s", where, buf);
    }
}

// Chuoi xac thuc noi vao cuoi URL.
// - Che do idToken: RONG. Token di qua header Authorization: Bearer. Bat buoc
//   phai lam vay vi idToken JWT dai ~900-1100 byte, nhet vao `?auth=` se tran
//   cac buffer url[256]/url[384] dang dung.  (Da kiem chung 2026-09-03: RTDB
//   REST co parse header nay — token rac tra ve 401 "Unauthorized request.",
//   khac han 200 khi khong gui header.)
// - Che do cu: `?auth=<Database Secret>` nhu truoc.
// `sep` la ky tu ngan cach dung cho URL do: '?' neu chua co query nao, '&' neu
// da co san tham so khac.
// Noi tham so auth vao CUOI _url.
//
// Vi sao khong dung header: da do that 2026-09-05 bang idToken hop le cua chinh
// box (945 byte, localId khop BOX_ID) tren /boxes/<BOX_ID>/status.json:
//     Authorization: Bearer <idToken>    -> 401 "Unauthorized request."
//     Authorization: Firebase <idToken>  -> 401
//     ?auth=<idToken>                    -> 200
// RTDB chi nhan `Bearer` cho OAuth2 access token cua service account, khong phai
// Firebase idToken. Xem MEMORY.md muc 17. Dung sua nguoc lai.
void NetworkManager::appendAuth(char sep) {
    size_t len = strlen(_url);
    if (len >= sizeof(_url)) return;
#if FIREBASE_USE_IDTOKEN
    if (_idToken[0] == '\0') return;
    int n = snprintf(_url + len, sizeof(_url) - len, "%cauth=%s", sep, _idToken);
#else
    int n = snprintf(_url + len, sizeof(_url) - len, "%cauth=%s", sep, FIREBASE_AUTH_SECRET);
#endif
    // Cat cut token cho ra 401 TRONG Y HET voi 401 do rule tu choi. Chinh su
    // nhap nhang kieu nay da lam muc 11 ket luan sai mot vong. Phai keu len.
    if (n < 0 || (size_t)n >= sizeof(_url) - len) {
        DLOG("[NET] auth query BI CAT CUT (url %u)", (unsigned)strlen(_url));
    }
}

// Bao lau khong nhan them byte nao thi coi la stream chet. http.setTimeout(30000)
// chi ap cho mot lan doc, khong chot duoc ca vong lap.
static const uint32_t DOWNLOAD_STALL_TIMEOUT_MS = 30000;

// Task WakeSync THƯỜNG TRÚ, tạo một lần rồi ngủ chờ notify.
//
// Trước 2026-09-18 mỗi chu kỳ sync tạo một task mới stack 12KB rồi xoá. Xin và
// trả một khối 12KB liền mạch cứ vài chục giây, ngay sát lúc mbedTLS cần khối
// ~16KB liền mạch, là cách chắc chắn nhất để làm vụn heap — đúng triệu chứng
// `SSL - Memory allocation` ở MEMORY.md §21. Stack giờ cấp đúng một lần.
void NetworkManager::triggerWakeupSync(uint8_t batteryPercent, bool isCharging, IStorageProvider* storage) {
    if (isPlaybackActive()) {
        DLOG("[NET] sync skip: video playing");
        return;
    }

    // Hai task khac do uu tien (UIController=5, MediaPlayer=3) cung goi ham nay.
    // Doc roi ghi _isSyncing thanh hai lenh rieng thi ca hai deu co the lot qua
    // va tao 2 lan sync ghi de len cung mot slot flash.
    bool claimed = false;
    portENTER_CRITICAL(&s_syncMux);
    if (!_isSyncing) {
        _isSyncing = true;
        claimed = true;
    }
    portEXIT_CRITICAL(&s_syncMux);
    if (!claimed) return;

    // Tham số đi qua biến thành viên, không qua con trỏ cấp phát: chỉ có một lượt
    // sync chạy tại một thời điểm (cờ _isSyncing ở trên đã chốt điều đó).
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
        return;  // task vừa tạo chạy ngay lượt đầu, không cần notify
    }

    xTaskNotifyGive(_syncTask);
}

void NetworkManager::wakeupSyncTaskWorker(void* param) {
    NetworkManager* self = static_cast<NetworkManager*>(param);
    for (;;) {
        if (self != nullptr) {
            self->syncWakeup(self->_syncBattery, self->_syncCharging, self->_syncStorage);
        }
        // Chờ lượt sau. Notify tới trước khi vào đây thì ulTaskNotifyTake trả về
        // ngay (đếm được giữ lại), nên không mất lượt nào.
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    }
}

bool NetworkManager::syncWakeup(uint8_t batteryPercent, bool isCharging, IStorageProvider* storage) {
    _isSyncing = true;

    // Lưu chẩn đoán của chu kỳ trước đó (đặc biệt hữu ích khi chu kỳ trước fail hoặc ngủ vội)
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

    // 1. Tái kết nối Wi-Fi. 12s chứ không phải 5s: sau Light Sleep đây là một lần
    // associate + 4-way handshake + xin IP DHCP hoàn toàn mới (xem ensureConnected),
    // thực tế tốn 3-8s. Cắt ở 5s là bỏ dở giữa chừng đúng lúc sắp xong.
    // Không sợ tốn pin oan: Task_UIController bị khoá không cho ngủ khi isSyncing().
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

    // Mỗi phiên TLS tới Firebase cần ~35-45KB heap cho mbedTLS handshake (in/out
    // content buffer mặc định 16KB mỗi chiều — KHÔNG chỉnh được: setBufferSizes()
    // là API của ESP8266, WiFiClientSecure trên ESP32 không có, đã kiểm chứng
    // 2026-09-03). Nếu về sau còn gặp http.GET() = -1 mà Wi-Fi rõ ràng vẫn sống,
    // đây là con số cần nhìn đầu tiên để phân biệt OOM với lỗi đường truyền.
    // maxblk = khoi lien lon nhat: phan biet phan manh voi het RAM (MEMORY.md §21).
    DLOG("[NET] sync start heap=%u maxblk=%u", (unsigned)ESP.getFreeHeap(),
         (unsigned)ESP.getMaxAllocHeap());

    // 2. Đồng bộ thời gian NTP trước để các mốc timestamp phía sau luôn chính xác
    syncNtpTime(5000);

    // Chốt chặn thời gian — BẮT BUỘC đi kèm setCACert(), không phải tuỳ chọn.
    // Với setInsecure() thì NTP hỏng vẫn chạy được. Với VERIFY_REQUIRED thì
    // time(nullptr) ≈ 0 lúc boot nguội -> mbedTLS trả BADCERT_FUTURE -> MỌI
    // handshake fail. Bỏ bước này là biến "NTP chập chờn" thành "mất hẳn cloud".
    if (time(nullptr) < MIN_VALID_EPOCH) {
        // Phải hạ cờ: syncNtpTime() short-circuit 60s nếu _isTimeSynced đang bật,
        // gọi lại suông sẽ return true ngay mà không hề xin lại gói NTP nào.
        _isTimeSynced = false;
        DLOG("[NET] time invalid -> NTP retry 15s");
        syncNtpTime(15000);
    }
    if (time(nullptr) < MIN_VALID_EPOCH) {
        // Marker RIÊNG, không lẫn với http.GET() = -1: ở đây chưa hề mở kết nối
        // nào cả. Thấy dòng này nghĩa là lỗi NTP, không phải lỗi TLS/mạng.
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

    // 2b. Lấy/gia hạn idToken TRƯỚC mọi lời gọi Firebase. Đặt sau chốt chặn thời
    // gian vì hạn token so bằng time(nullptr) — RTC sai thì token vừa lấy về đã
    // bị coi là hết hạn ngay. Token sống 1 giờ nên hầu hết chu kỳ sync chỉ đọc
    // lại biến trong RAM, không tốn request nào.
    if (!ensureIdToken()) {
        DLOG("[NET] sync abort: khong lay duoc idToken");
        strncpy(_diagStep, "token_fail", sizeof(_diagStep) - 1);
        strncpy(_diagErr, "idtoken_fail", sizeof(_diagErr) - 1);
        _isSyncing = false;
        return false;
    }
    strncpy(_diagStep, "token_ok", sizeof(_diagStep) - 1);

    // 3. Check Flags (Alarms, OTA, Pairing) TRƯỚC để status có dữ liệu alarm mới nhất
    checkFirebaseFlags();
    vTaskDelay(pdMS_TO_TICKS(100));

    if (isPlaybackActive()) {
        strncpy(_diagErr, "playback_active", sizeof(_diagErr) - 1);
        _isSyncing = false;
        return false;
    }

    // 4. Update Status (Heartbeat + Diag Telemetry đẩy lên cloud)
    updateFirebaseStatus(batteryPercent, isCharging);
    vTaskDelay(pdMS_TO_TICKS(100));

    if (isPlaybackActive()) {
        strncpy(_diagErr, "playback_active", sizeof(_diagErr) - 1);
        _isSyncing = false;
        return false;
    }

    // 5. Check and download new messages.
    //
    // Cong "het slot" nam O DAY chu khong o ngoai cung. Truoc 2026-09-05 no nam
    // tren ca chu ky (main.cpp: `&& !isStorageFull` cho sync 10s, va nhanh
    // "post-wakeup sync skip: FULL"), nen khi day slot thi box im hoan toan:
    // mat heartbeat, mat doc co, mat dong bo bao thuc, mat OTA va pairing flag.
    // Y dinh ban dau chi la "day roi thi khoi tai tin cho phi" — dung, nhung chi
    // ap cho RIENG buoc nay.
    if (storage != nullptr) {
        if (storage->isFull()) {
            // _numOfNewMsg CHI duoc gan trong checkAndDownloadNewMessages(), tuc
            // la NAM SAU cong nay. Sau mot lan reset trong lúc dang day slot, no
            // ve 0 va khong duong nao dat lai duoc -> hop vua bao "No new
            // messages" luc cham, vua bao "het slot" luc sync, ket vinh vien vi
            // slot chi duoc tra lai bang cach doc. Nang san moi chu ky sync.
            uint8_t unread = storage->getUnreadCount();
            if (_numOfNewMsg < unread) _numOfNewMsg = unread;
            // In kèm số slot chưa đọc: phân biệt "đầy thật" (unread > 0) với
            // "con trỏ ghi trỏ nhầm" (unread == 0 mà vẫn báo đầy).
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
// Firebase Auth — idToken riêng của box thay cho Database Secret quyền admin
// ============================================================================
// Cả hai endpoint dưới đây đã đo chain thật (2026-09-03): identitytoolkit và
// securetoken đều về GTS Root R4 — đã có sẵn trong firebase_root_ca.h, không
// phải nhúng thêm chứng chỉ nào.

// Firebase Storage NGUOC voi RTDB: no CO nhan header, va scheme la
// "Firebase <idToken>" (khong phai "Bearer"). Khac dich vu, khac quy uoc.
//
// CHUA VERIFY DUOC tren may that: Storage hien dang mo nen gui token rac cung
// tra 200, khong phan biet duoc "duoc chap nhan" voi "khong can thiet". Chi do
// duoc sau khi deploy storage.rules. Day chinh la cai bay da lam muc 11 ket luan
// sai ve header cua RTDB — dung lap lai kieu suy luan do.
//
// Dung String thay vi buffer stack ~1.4KB: TASK_STACK_NETWORK chi 6144 va cho nay
// da nam sau trong call-chain. addHeader() nhan const String& nen dang nao cung
// sinh String tam — khai bao tuong minh khong ton them gi.
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
        // Token chết hoặc rule từ chối. Hạ hạn để chu kỳ sync sau lấy token mới.
        // Không tự retry ngay tại đây: token sống 1 giờ còn 1 chu kỳ sync chỉ vài
        // giây, nên 401 gần như luôn nghĩa là RULE từ chối chứ không phải hết hạn
        // — retry ngay chỉ tốn thêm một handshake TLS mà vẫn 401.
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
    return true;   // vẫn dùng Database Secret, không cần token
#else
    // Trừ hao 60s: token còn hạn dưới 1 phút thì coi như hết, tránh trường hợp
    // hết hạn ngay giữa chu trình sync.
    if (!force && _idToken[0] != '\0' && time(nullptr) < _idTokenExpiry - 60) {
        return true;
    }

    // Refresh token không hết hạn theo thời gian -> ưu tiên dùng, đỡ phải gửi
    // lại mật khẩu qua đường truyền mỗi lần.
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
        // Refresh hỏng (bị thu hồi / đổi mật khẩu) -> vứt đi, đăng nhập lại từ đầu.
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
// Đọc idToken/refreshToken/expiresIn từ response rồi cất vào RAM + NVS.
// Dùng filter của ArduinoJson để CHỈ cấp phát 3 trường cần thiết — response
// signInWithPassword còn kèm email/localId/kind..., cấp phát trọn gói là phí
// heap đúng lúc sắp cần ~45KB cho handshake TLS kế tiếp.
// Nhan String chu KHONG phai Stream. Ly do (do that 2026-09-05, MEMORY.md muc 18):
// ca identitytoolkit lan securetoken tra "Transfer-Encoding: chunked", khong co
// Content-Length. http.getStreamPtr() cho ra stream THO con nguyen dong kich thuoc
// chunk dang hex, vi du "4a1\r\n{...}". ArduinoJson doc phai "4a1" -> parse "4"
// thanh mot SO, ket thuc THANH CONG, roi doc["idToken"] = null -> bao
// "thieu idToken" ma khong he co loi JSON. Chi http.getString() moi giai ma chunked.
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
        // In dau response: khong co dong nay thi "thieu idToken" khong noi len
        // duoc gi ca — da tung ton mot vong flash vi vay. 60 ky tu dau chi chua
        // phan "kind"/"error", chua toi cho co token.
        DLOG("[NET] auth: thieu idToken; body=%s", body.substring(0, 60).c_str());
        return false;
    }
    // Firebase trả expiresIn dạng CHUỖI giây ("3600"), không phải số.
    long ttl = (expires != nullptr) ? atol(expires) : 3600;
    if (ttl <= 0) ttl = 3600;

    // Luu JWT THO. Truoc day luu "Bearer <jwt>" cho addHeader(), nhung RTDB
    // khong nhan header — token phai di vao query `?auth=` (MEMORY.md muc 17).
    int n = snprintf(outToken, tokenLen, "%s", idTok);
    if (n < 0 || (size_t)n >= tokenLen) {
        // Cắt cụt token = mọi request sau đó 401 mà không rõ lý do. Thà báo hỏng.
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

// Gửi request auth và xử lý response. Gom chung vì 2 endpoint chỉ khác URL,
// content-type và cách đặt tên trường (camelCase vs snake_case).
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
    // getString() (KHONG phai getStreamPtr) vi response la chunked — xem ghi chu
    // o parseAuthResponse. Doc xong roi end() ngay de tra connection som.
    String resp = http.getString();
    http.end();

    // snakeCase = true: endpoint securetoken dùng id_token/refresh_token/expires_in
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
        // 400 kèm PASSWORD_LOGIN_DISABLED = chưa bật Email/Password trong Console.
        // 400 kèm EMAIL_NOT_FOUND / INVALID_PASSWORD = chưa chạy provision script
        // hoặc điền sai config_secrets.h.
        DLOG("[NET] signIn fail: %d", code);
        if (code < 0) logTlsError(client, "signin");
        else DLOG("[NET] signIn: %s", http.getString().substring(0, 80).c_str());
        http.end();
        return false;
    }

    char newRefresh[FIREBASE_REFRESH_TOKEN_MAX_LEN] = "";
    // getString() (KHONG phai getStreamPtr) vi response la chunked — xem ghi chu
    // o parseAuthResponse. Doc xong roi end() ngay de tra connection som.
    String resp = http.getString();
    http.end();

    // snakeCase = false: endpoint identitytoolkit dùng idToken/refreshToken/expiresIn
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

    char payload[384];
    uint32_t now = (uint32_t)time(nullptr);
    snprintf(payload, sizeof(payload),
             "{\"online\":true,\"battery\":%d,\"is_charging\":%s,\"last_seen\":%u,"
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

    int httpCode = http.PATCH((uint8_t*)payload, strlen(payload));
    noteAuthFailure(httpCode, "status");
    if (httpCode < 0) {
        // Day la phien TLS DAU TIEN cua moi chu trinh sync -> cung la cho re
        // nhat de biet handshake co qua duoc khong sau khi bat setCACert().
        DLOG("[NET] status PATCH %d, heap=%u", httpCode, (unsigned)ESP.getFreeHeap());
        logTlsError(client, "status");
    } else {
        // DEBUG_SCREEN: dong nay chi de xac minh Phase A (handshake qua duoc,
        // heap con lai bao nhieu sau khi parse 2 root). Go bo sau khi da chot.
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

    // Cờ báo thức tên là `a_flag` — đúng tên backend ghi (firebase-alarm.repository.ts).
    // Trước 2026-09-18 firmware đọc `sync_alarms_flag`, một cờ không ai ghi, nên
    // báo thức đặt trên web không bao giờ về tới hộp (nợ đã ghi ở MEMORY.md §20).
    bool alarmFlag = false;
    bool emergencyOta = false;
    bool normalOta = false;
    if (payload != "null" && payload.length() > 2) {
        JsonDocument doc;
        if (deserializeJson(doc, payload)) {
            strncpy(_diagStep, "flags_json_err", sizeof(_diagStep) - 1);
            strncpy(_diagErr, "json_fail", sizeof(_diagErr) - 1);
            return false;
        }
        alarmFlag = doc["a_flag"] | false;
        emergencyOta = doc["emergency_ota"] | false;
        normalOta = doc["normal_ota"] | false;
    }
    _lastAFlag = alarmFlag;

    // Hai cờ OTA trước 2026-09-18 chỉ được đọc rồi reset, không kích hoạt gì —
    // nhìn code tưởng hộp cập nhật được từ xa. Giờ chúng mở cửa sổ OTA thật:
    // main.cpp bật web server + mDNS trong OTA_WINDOW_MS rồi tắt. Web server
    // không còn chạy suốt đời máy chỉ để chờ một việc hiếm khi làm.
    if (emergencyOta || normalOta) {
        _otaRequested = true;
        DLOG("[NET] flags: OTA requested");
    }

    // Reset cờ TRƯỚC khi tải danh sách báo thức: web sửa tiếp trong lúc đang tải
    // sẽ bật lại a_flag và được bắt ở chu kỳ sau. Reset sau khi tải thì lần sửa đó
    // bị xoá mất cờ. Chỉ PATCH khi có cờ bật — trước đây PATCH mỗi chu kỳ 10s,
    // tốn một lần bắt tay TLS vô ích.
    if (alarmFlag || emergencyOta || normalOta) {
        WiFiClientSecure patchClient;
        configureTlsClient(patchClient);
        HTTPClient patchHttp;

        // Dung lai _url an toan: GET flags o tren da http.end() xong truoc khi toi day.
        snprintf(_url, sizeof(_url), "https://%s/boxes/%s/flags.json",
                 FIREBASE_HOST, BOX_ID);
        appendAuth('?');

        if (patchHttp.begin(patchClient, _url)) {
            patchHttp.setTimeout(FIREBASE_TIMEOUT_MS);
            patchHttp.addHeader("Content-Type", "application/json");
            int pc = patchHttp.PATCH("{\"a_flag\":false,\"emergency_ota\":false,\"normal_ota\":false}");
            noteAuthFailure(pc, "flags reset");
            patchHttp.end();
        }
    }

    // Đồng bộ báo thức hai chiều (luật đầy đủ ở AlarmClock.h):
    //  - hộp có sửa đổi chưa đẩy -> PUT cả danh sách lên, bỏ qua a_flag (hộp thắng)
    //  - không thì tải về khi a_flag bật, hoặc lần sync đầu tiên sau khi boot
    //    (NVS có thể rỗng/cũ, vd. vừa nạp firmware đổi kích thước AlarmItem)
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
        // Tải hỏng thì giữ _alarmsNeedFetch để chu kỳ sau thử lại dù cờ đã reset.
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

// Hộp -> cloud: PUT thay TOÀN BỘ boxes/<id>/config/alarm_list. Rule cho phép box
// ghi đúng nhánh này (database.rules.json). Không bật a_flag: chính hộp là bên
// vừa ghi, bật lên chỉ khiến chu kỳ sau tải lại đúng thứ vừa đẩy.
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
        // Web không đọc created_at; ghi cho khớp schema backend (BaseModel).
        a["created_at"] = nowMs;
        a["updated_at"] = nowMs;
    }
    String body;
    serializeJson(doc, body);  // {} khi rỗng -> RTDB xoá nút, đúng ý "xoá hết"

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

    // "null" = web đã xoá hết báo thức. Trước đây return sớm ở đây nên hộp giữ
    // nguyên danh sách cũ trong NVS và vẫn kêu các báo thức đã xoá.
    if (payload != "null" && payload.length() > 2) {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, payload);
        if (err) return false;

        JsonObject obj = doc.as<JsonObject>();
        for (JsonPair kv : obj) {
            if (count >= MAX_ALARMS) break;
            JsonObject alarmObj = kv.value().as<JsonObject>();
            const char* tStr = alarmObj["time"] | "";
            // id dài hơn buffer sẽ bị cắt -> lần đẩy ngược lên tạo key khác. Bỏ qua
            // và kêu lên thay vì âm thầm nhân đôi báo thức trên cloud.
            if (strlen(kv.key().c_str()) >= sizeof(alarms[count].id) ||
                !AlarmClock::isValidTime(tStr)) {
                DLOG("[NET] alarm bo qua: %s", kv.key().c_str());
                continue;
            }
            strncpy(alarms[count].id, kv.key().c_str(), sizeof(alarms[count].id) - 1);
            strncpy(alarms[count].time, tStr, sizeof(alarms[count].time) - 1);
            alarms[count].isEnable = alarmObj["is_enable"] | false;
            alarms[count].repeatable = alarmObj["repeatable"] | false;
            count++;
        }
    }

    AlarmClock::instance().replaceFromCloud(alarms, count);
    return true;
}

// Tải voice_url/bg_music_url và append vào slot vừa ghi (offset ngay sau phần
// ảnh/video, hoặc offset 4 nếu slot rỗng — xem checkAndDownloadNewMessages()).
// Dùng chung cho cả đường ảnh/video (audio là phụ, tải lỗi không huỷ message)
// và đường "tin nhắn tĩnh không ảnh" (audio có thể là nội dung chính).
// Trả về true nếu KHÔNG có voice URL (không có gì để tải, không phải lỗi) hoặc
// tải thành công trọn vẹn; false nếu có URL nhưng tải thất bại/thiếu/stall.
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
        // Bat buoc khi storage.rules da siet: khong co header nay thi Storage tra 403.
        addStorageAuthHeader(httpAudio);
        int aCode = httpAudio.GET();
        noteAuthFailure(aCode, "voice");
        if (aCode == HTTP_CODE_OK) {
            int aLen = httpAudio.getSize();
            WiFiClient* aStream = httpAudio.getStreamPtr();

            // Dùng openForAppend (virtual method trên IStorageProvider)
            // để ghi nối tiếp mà không xóa sector đã có video
            if (storage->openForAppend(writeSlotId)) {
                // Ghi AUDC header (10 bytes)
                uint8_t audcHeader[10];
                memcpy(audcHeader, "AUDC", 4);
                uint16_t sr      = (uint16_t)AUDIO_SAMPLE_RATE;
                uint32_t pcmSize = (aLen > 0) ? (uint32_t)aLen : 0;
                memcpy(audcHeader + 4, &sr,      2);
                memcpy(audcHeader + 6, &pcmSize, 4);
                // Ghi hut header thi AudioPlayer khong khop magic
                // "AUDC" -> phat video im lang. Van phai bao ra log,
                // neu khong loi NAND o nhanh audio hoan toan vo hinh.
                bool aWriteError =
                    storage->writeChunk(audcHeader, sizeof(audcHeader)) < sizeof(audcHeader);
                if (aWriteError) {
                    DLOG("[NET] Audio hdr write SHORT");
                }

                // Stream PCM data vào slot (không gọi closeWrite vì không đổi metadata)
                // 2048B thay vi 256B — cung ly do nhu vong lap video: buffer nho +
                // delay(1) vo dieu kien tung ep tran toc do tai, de dinh STALL.
                uint8_t abuf[2048];
                int     aTotalRead = 0;
                uint32_t aLastProgressMs = millis();
                while (!aWriteError && httpAudio.connected() && (aLen > 0 || aLen == -1)) {
                    size_t av = aStream->available();
                    if (av) {
                        size_t tr = (av < sizeof(abuf)) ? av : sizeof(abuf);
                        // read() chu khong readBytes() — xem giai thich day du o vong
                        // lap tai video trong checkAndDownloadNewMessages().
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
                            // Tran cung — dat aWriteError (khong phai writeError) de
                            // slot dang do bi loai o buoc kiem tra cua chinh vong nay.
                            if (aTotalRead > (int)MAX_MEDIA_BYTES) {
                                DLOG("[NET] audio dl ABORT: over cap %d", aTotalRead);
                                aWriteError = true;
                                break;
                            }
                        }
                    }
                    // Cung dang treo vo han nhu vong lap video.
                    if (millis() - aLastProgressMs > DOWNLOAD_STALL_TIMEOUT_MS) {
                        DLOG("[NET] audio dl STALL %d bytes", aTotalRead);
                        // Fix: truoc day KHONG set aWriteError=true o day -> log bao
                        // nham "OK" du tai cut. Voi tin nhan tinh khong anh, audio co
                        // the la noi dung DUY NHAT nen phai bao chinh xac tai day.
                        aWriteError = true;
                        break;
                    }
                    // Chi nhuong CPU khi THUC SU khong co data (xem giai thich o
                    // vong lap video phia tren).
                    if (av == 0) {
                        delay(1);
                    }
                }
                // Chốt phiên append: ghi audioSize vào bảng
                // slot. Không có bước này thì phần audio nằm
                // trên flash nhưng AudioPlayer không biết nó
                // ở đâu và dài bao nhiêu -> hộp câm.
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
        // auth nam CUOI: orderBy/startAt da chiem '?' nen tham so auth phai noi
        // bang '&'. Dung o ca 2 che do — Database Secret lan idToken.
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
        // -1 = HTTPC_ERROR_CONNECTION_REFUSED: TCP/TLS connect thất bại. Thử lại
        // suông trên cùng client (cách cũ) gần như vô ích khi nguyên nhân là link
        // Wi-Fi đã chết hoặc DNS cũ — phải ÉP tái lập association trước, việc này
        // cũng xin lại DNS server mới từ DHCP.
        DLOG("[NET] msg GET %d, heap=%u -> re-assoc", httpCode, (unsigned)ESP.getFreeHeap());
        // Phan biet "chung chi khong hop le" voi "khong noi duoc toi server":
        // ca hai deu ra -1 o tang HTTPClient, doc lastError moi biet duoc.
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

    // Zero-copy JSON stream parsing (tránh cấp phát chuỗi String tạm lớn gây phân mảnh RAM)
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

    // Đảm bảo msgList luôn được sắp xếp theo timestamp tăng dần (cũ nhất -> mới nhất)
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

    // Giữ RF luôn bật hết công suất trong suốt quá trình tải — Modem Sleep mặc
    // định (WIFI_PS_MIN_MODEM) tăng độ trễ/rớt gói TCP Window Update, góp phần
    // gây download bị ngắt giữa chừng ở tốc độ cao. Bật lại tiết kiệm điện ngay
    // sau vòng lặp (mọi lối thoát khỏi vòng lặp bên dưới đều là `break`, không
    // có `return`, nên luôn chạy tới đây).
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

        // Nếu Firebase không có timestamp (ts == 0), tự động dùng mốc giờ NTP hoặc bộ đếm millis()
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

        // Tìm voice URL (audio đính kèm) — bg_music_url dùng chung 1 cơ chế tải/append
        // với voice_url (cùng là PCM/WAV, chỉ khác vai trò UX: lời thoại vs nhạc nền).
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

        // Tìm caption text — kiểm tra SỰ TỒN TẠI của field, không dựa vào "type"
        // (web có bug nhỏ đã biết: field text không bị xoá khi đổi chế độ, nên
        // 1 message type khác vẫn có thể mang text cũ; đọc field thật là đúng nhất).
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
            DLOG("[NET] Downloading media...");
            
            if (http.begin(client, fullUrl.c_str())) {
                http.setTimeout(30000);
                // Bat buoc khi storage.rules da siet: khong co header nay thi Storage tra 403.
                addStorageAuthHeader(http);
                int code = http.GET();
                if (code < 0) logTlsError(client, "media");
                noteAuthFailure(code, "media");
                if (code == HTTP_CODE_OK) {
                    int len = http.getSize();
                    int initialLen = len;
                    int totalRead = 0;
                    // Heap o day la con so dang nhin nhat sau khi bat setCACert():
                    // phien TLS nay song lau nhat (giu qua ca doan tai audio long
                    // ben trong) va la cho tung co tien su OOM.
                    DLOG("[NET] GET OK len=%d heap=%u", len, (unsigned)ESP.getFreeHeap());
                    char writeSlotId[16] = "";
                    if (!storage->getNextWriteSlotIdentifier(writeSlotId, sizeof(writeSlotId))) {
                        DLOG("[NET] skip dl: FULL");
                        http.end();
                        _isDownloadingMedia = false;
                        _hasPendingMessages = true;
                        break; // Dừng tiến trình khi bộ nhớ đầy
                    }

                    WiFiClient* stream = http.getStreamPtr();
                    if (storage->openForWrite(writeSlotId)) {
                        DLOG("[NET] writing slot %s", writeSlotId);
                        // 2048B thay vi 256B: buffer nho + delay(1) vo dieu kien moi
                        // vong lap tung ep tran toc do tai xuong con ~20-25KB/s (256B
                        // rut can tren 1 tick 10ms). Van la streaming cuon chieu, RAM
                        // tieu thu khong doi bat ke file lon nho.
                        uint8_t buffer[2048];
                        bool writeError = false;
                        // Voi len == -1 (chunked, khong co Content-Length) dieu kien
                        // vong lap khong bao gio tu sai: chi thoat khi server dong
                        // ket noi. Mot stream nua-mo khong gui byte nao se treo o day
                        // vinh vien. Chot lai bang moc thoi gian co tien do that su.
                        uint32_t lastProgressMs = millis();
                        int lastLoggedRead = 0;
                        while (http.connected() && (len > 0 || len == -1)) {
                            size_t sizeAvail = stream->available();
                            if (sizeAvail) {
                                size_t toRead = (sizeAvail < sizeof(buffer)) ? sizeAvail : sizeof(buffer);
                                // read() chu KHONG readBytes(): WiFiClientSecure khong override
                                // readBytes() nen no roi ve Stream::readBytes() doc TUNG BYTE MOT
                                // (Stream.cpp:41), moi byte lai goi available() -> mbedtls_ssl_read()
                                // 2 lan. Chunk 2048B = ~4096 loi goi mbedTLS; ca file 2.2MB = hon 4
                                // TRIEU loi goi -> tran toc do ~20-25KB/s. Te hon nua: khi buffer
                                // mbedTLS can giua chung chunk, timedRead() busy-spin toi 30 GIAY
                                // (Stream.cpp:31, _timeout = 30s) khong nhuong CPU, khong rut socket.
                                // read(buf,len) chi ton 1 available() + 1 mbedtls_ssl_read cho ca khoi.
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
                                    // Tran cung: dung ngay thay vi tai vai phut roi chet cho khac.
                                    if (totalRead > (int)MAX_MEDIA_BYTES) {
                                        DLOG("[NET] dl ABORT: over cap %d", totalRead);
                                        writeError = true;
                                        break;
                                    }
                                    // Log thua tay (moi 16KB) de khong doi nhip vong lap.
                                    if (totalRead - lastLoggedRead >= 16384) {
                                        lastLoggedRead = totalRead;
                                        DLOG("[NET] dl %d/%d", totalRead, initialLen);
                                    }
                                }
                            }
                            if (millis() - lastProgressMs > DOWNLOAD_STALL_TIMEOUT_MS) {
                                DLOG("[NET] dl STALL %d/%d", totalRead, initialLen);
                                // writeError = true de slot dang do bi loai o buoc kiem
                                // tra ben duoi. Khong co dong nay thi mot file tai dang
                                // do voi initialLen <= 0 van lot qua -> slot rac.
                                writeError = true;
                                break;
                            }
                            // Chi nhuong CPU khi THUC SU khong co data — nhuong vo dieu
                            // kien moi vong lap la nguyen nhan chinh khien tai xuong bi
                            // tran toc do va de dinh STALL khi mang chap chon.
                            if (sizeAvail == 0) {
                                delay(1);
                            }
                        }
                        // Thoat vong lap do !http.connected() (khong phai STALL, khong
                        // phai du byte) = ket noi bi dut giua chung. Truoc day khong in
                        // gi ca nen phai suy ra tu dong "DL err" -> khong phan biet duoc
                        // "dut ket noi" voi "tai thieu byte". Log rieng de chan doan.
                        if (!writeError && !http.connected() &&
                            (initialLen > 0 && totalRead < initialLen)) {
                            DLOG("[NET] conn DROPPED @ %d/%d", totalRead, initialLen);
                        }
                        // Chốt kết quả TRƯỚC khi quyết định commit hay huỷ. closeWrite() ghi
                        // slot table + set unread bit; gọi vô điều kiện như code cũ khiến 1 lần
                        // stall/timeout giữa chừng cũng biến slot dở (dữ liệu cụt) thành "tin
                        // hợp lệ chưa đọc" -> phát được vài giây đầu (đủ 20-byte header) rồi lỗi
                        // Bad jpegSize khi chạm vùng chưa ghi (bug xác nhận 2026-09-02, video
                        // 15s/2.2MB bị NAND/mạng stall ~20%). Lỗi thật -> discardWrite(): không
                        // đụng slot table/unread bitmask, giữ nguyên _writeSlotIndex để lần sync
                        // sau retry đúng slot này thay vì đốt thêm 1 slot mới cho mỗi lần fail.
                        bool downloadComplete = !writeError && (initialLen <= 0 || totalRead >= initialLen);
                        if (downloadComplete) {
                            storage->closeWrite(maxDisplayTime);
                        } else {
                            storage->discardWrite();
                        }

                        // Kiểm tra dữ liệu đã tải trọn vẹn 100% chưa
                        if (downloadComplete) {
                            DLOG("[NET] DL OK slot %s", writeSlotId);

                            // Đóng phiên HTTP video TRƯỚC khi mở phiên audio mới —
                            // downloadVoiceSegment() dùng chung 1 WiFiClientSecure client,
                            // để phiên cũ chưa .end() có thể làm phiên mới bắt tay sai trạng
                            // thái. http.end() gọi lại lần nữa ở cuối khối vẫn an toàn (no-op).
                            http.end();

                            // Voice/bg_music là phụ với ảnh/video: tải lỗi chỉ log, không
                            // huỷ cả message (hành vi giữ nguyên như code cũ).
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
            // Tin nhắn tĩnh KHÔNG có ảnh/video: chỉ audio (voice/nhạc nền) và/hoặc text.
            // Vẫn phải openForWrite()/closeWrite() để tạo slot "rỗng" hợp lệ
            // (dataSize=4 sentinel — đã trace/kiểm chứng khớp với cách
            // NandStorageProvider::openForAppend() tính offset audio kế tiếp).
            // MediaPlayer nhận sentinel này để hiện màn đen thay vì cố decode
            // JPEG không tồn tại.
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
                    // Ở đây audio là NỘI DUNG CHÍNH (không có ảnh) — tải lỗi phải huỷ
                    // cả message, khác với nhánh ảnh/video ở trên (audio chỉ là phụ).
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


        // CHỈ CẬP NHẬT TIMESTAMP KHI VÀ CHỈ KHI TASK CỦA TIN NHẮN NÀY ĐÃ HOÀN THÀNH 100%
        if (messageSuccess) {
            if (ts > successfullyProcessedMaxTs) {
                successfullyProcessedMaxTs = ts;
            }
        } else {
            DLOG("[NET] ts fail");
            break; // Ngắt vòng lặp để đảm bảo thứ tự tin nhắn không bị nhảy vọt
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
