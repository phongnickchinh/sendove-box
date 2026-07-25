#include "FirebaseClient.h"
#include <ArduinoJson.h>
#include "SDCardManager.h"

void FirebaseClient::init(const char* firebaseHost, const char* boxId) {
    _firebaseHost = firebaseHost;
    _boxId = boxId;
    Serial.printf("[Firebase] Initialized with host: %s, boxId: %s\n", _firebaseHost.c_str(), _boxId.c_str());
}

FirebaseMessageResult FirebaseClient::checkMessages() {
    FirebaseMessageResult result;

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println(F("[Firebase] WiFi not connected"));
        return result;
    }

    HTTPClient http;
    String url = String("https://") + _firebaseHost + "/boxes/" + _boxId + ".json";

    http.begin(url);
    http.setTimeout(10000);
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();

        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, payload);

        if (!err) {
            JsonObject messaging = doc["messaging"];
            result.hasNewMessage = messaging["has_new_msg"] | false;
            result.videoUrl = messaging["video_url"] | "";
            result.voiceUrl = messaging["voice_url"] | "";

            JsonObject config = doc["config"];
            result.alarmTime = config["alarm_time"] | "07:00";
            result.timezone = config["timezone"] | 7;
            result.isAlarmActive = config["alarm_active"] | false;

            if (!config["wifi_ssid"].isNull()) {
                result.newWifiSsid = config["wifi_ssid"] | "";
                result.newWifiPass = config["wifi_pass"] | "";
            }
        } else {
            Serial.printf("[Firebase] JSON parse err: %s\n", err.c_str());
        }
    } else {
        Serial.printf("[Firebase] GET failed, code: %d\n", httpCode);
    }

    http.end();
    return result;
}

bool FirebaseClient::downloadFileToSD(const char* url, const char* sdPath, SDCardManager* sdCard) {
    if (WiFi.status() != WL_CONNECTED || sdCard == nullptr) {
        return false;
    }

    HTTPClient http;
    http.begin(url);
    http.setTimeout(15000);

    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("[Firebase] Download failed, code: %d\n", httpCode);
        http.end();
        return false;
    }

    int totalLen = http.getSize();
    WiFiClient* stream = http.getStreamPtr();

    if (!sdCard->openFileForWrite(sdPath)) {
        Serial.printf("[Firebase] Cannot open SD file: %s\n", sdPath);
        http.end();
        return false;
    }

    uint8_t buffer[512];
    int written = 0;

    while (http.connected() && (written < totalLen || totalLen == -1)) {
        size_t size = stream->available();
        if (size > 0) {
            int readBytes = stream->readBytes(buffer, min(size, sizeof(buffer)));
            size_t bytesAppended = sdCard->appendChunk(buffer, readBytes);
            written += bytesAppended;
        }
        delay(1);
    }

    sdCard->closeWriteFile();
    http.end();

    Serial.printf("[Firebase] Downloaded %d bytes to %s\n", written, sdPath);
    return (written > 0);
}

bool FirebaseClient::updateBoxStatus(uint8_t batteryPercent) {
    if (WiFi.status() != WL_CONNECTED) return false;

    HTTPClient http;
    String url = String("https://") + _firebaseHost + "/boxes/" + _boxId + "/status.json";

    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    char body[128];
    snprintf(body, sizeof(body), "{\"battery\":%d,\"last_online\":%lud,\"status\":\"online\"}",
             batteryPercent, (unsigned long)time(nullptr));

    int httpCode = http.PATCH(body);
    http.end();

    if (httpCode == HTTP_CODE_OK) {
        Serial.printf("[Firebase] Status updated: battery=%d%%\n", batteryPercent);
        return true;
    }

    Serial.printf("[Firebase] Status update failed: %d\n", httpCode);
    return false;
}

bool FirebaseClient::markMessageRead() {
    if (WiFi.status() != WL_CONNECTED) return false;

    HTTPClient http;
    String url = String("https://") + _firebaseHost + "/boxes/" + _boxId + "/messaging/has_new_msg.json";

    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    int httpCode = http.PUT("false");
    http.end();

    if (httpCode == HTTP_CODE_OK) {
        Serial.println(F("[Firebase] Message marked as read"));
        return true;
    }

    return false;
}
