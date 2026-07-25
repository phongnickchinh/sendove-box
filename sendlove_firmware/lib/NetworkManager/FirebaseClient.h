#ifndef FIREBASE_CLIENT_H
#define FIREBASE_CLIENT_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "config.h"

// Forward declarations
class ConfigManager;
class SDCardManager;

/// Kết quả kiểm tra tin nhắn từ Firebase Realtime Database
struct FirebaseMessageResult {
    bool hasNewMessage = false;
    String videoUrl;
    String voiceUrl;
    String alarmTime;
    int8_t timezone = 7;
    bool isAlarmActive = false;
    String newWifiSsid;
    String newWifiPass;
};

class FirebaseClient {
public:
    FirebaseClient() = default;

    /// Khởi tạo FirebaseClient với cấu hình host và ID
    void init(const char* firebaseHost = FIREBASE_HOST, const char* boxId = BOX_ID);

    /// Kiểm tra tin nhắn mới và đọc cấu hình từ Firebase
    FirebaseMessageResult checkMessages();

    /// Tải file từ URL và ghi thẳng vào SD card (HTTP Stream Download)
    bool downloadFileToSD(const char* url, const char* sdPath, SDCardManager* sdCard);

    /// Cập nhật trạng thái pin và hộp lên Firebase
    bool updateBoxStatus(uint8_t batteryPercent);

    /// Đánh dấu tin nhắn đã đọc trên Firebase (has_new_msg = false)
    bool markMessageRead();

private:
    String _firebaseHost;
    String _boxId;
};

#endif // FIREBASE_CLIENT_H
