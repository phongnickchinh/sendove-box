#include <Arduino.h>
#include "config.h"

// --- Lib modules ---
#include "PowerManager.h"
#include "ConfigManager.h"
#include "NetworkHandler.h"
#include "MediaPlayer.h"
#include "UIController.h"
#include "SDCardManager.h"
#include "DisplayDriver.h"
#include "BatteryMonitor.h"
#include "TimeManager.h"

// ============================================================================
// SENDLOVE BOX — main.cpp
// ============================================================================
// Kiến trúc Task-based (FreeRTOS):
//   Task 1: Task_PowerManager  (Ưu tiên 4 — Cao nhất)
//   Task 2: Task_NetworkHandler (Ưu tiên 2)
//   Task 3: Task_MediaPlayer   (Ưu tiên 3)
//   Task 4: Task_UI_Controller (Ưu tiên 1 — Thấp nhất)
// ============================================================================

// --- Module instances (Global) ---
static PowerManager   powerMgr;
static ConfigManager  configMgr;
static NetworkHandler networkHandler;
static MediaPlayer    mediaPlayer;
static UIController   uiController;
static SDCardManager  sdCard;
static DisplayDriver  display;
static BatteryMonitor battery;
static TimeManager    timeMgr;

// --- FreeRTOS Synchronization ---
static SemaphoreHandle_t spiMutex      = nullptr;
static EventGroupHandle_t eventGroup   = nullptr;
static TaskHandle_t hTaskPower         = nullptr;
static TaskHandle_t hTaskNetwork       = nullptr;
static TaskHandle_t hTaskMedia         = nullptr;
static TaskHandle_t hTaskUI            = nullptr;

// --- Event Group Bits ---
#define EVT_NETWORK_SYNC     (1 << 0) // Yêu cầu Task_Network bật Wi-Fi và sync
#define EVT_NETWORK_DONE     (1 << 1) // Task_Network hoàn thành
#define EVT_MEDIA_PLAY       (1 << 2) // Yêu cầu Task_Media phát tin nhắn
#define EVT_MEDIA_DONE       (1 << 3) // Task_Media phát xong
#define EVT_TOUCH_DETECTED   (1 << 4) // Phát hiện chạm (từ UI hoặc wakeup)
#define EVT_HAS_NEW_MESSAGE  (1 << 5) // Có tin nhắn mới trên SD

// --- Biến trạng thái runtime (giữ qua Light-sleep) ---
static RTC_DATA_ATTR bool     co_tin_nhan = false;
static RTC_DATA_ATTR char     alarm_time[6] = "07:30"; // "HH:MM"
static RTC_DATA_ATTR bool     is_alarm_active = false;
static RTC_DATA_ATTR int8_t   timezone_offset = 7;
static RTC_DATA_ATTR uint32_t boot_count = 0;

// ============================================================================
// Task 1: Task_PowerManager (Ưu tiên cao nhất)
// ============================================================================
// Quản lý vòng đời: Xác định nguyên nhân thức dậy → phân luồng sự kiện
// → chờ các task hoàn thành → đưa ESP32 vào Light-sleep.
// ============================================================================
void Task_PowerManager(void* pvParameters) {
    Serial.printf("[Task_Power] Started. Boot count: %u\n", boot_count);

    for (;;) {
        WakeupCause cause = powerMgr.getWakeupCause();

        switch (cause) {
            case WakeupCause::POWER_ON: {
                // === Lần khởi động đầu tiên ===
                Serial.println(F("[Task_Power] POWER_ON: First boot"));

                // Kiểm tra Wi-Fi đã cấu hình chưa
                if (!configMgr.hasWiFiConfig()) {
                    // Chưa có → bật SoftAP Captive Portal
                    Serial.println(F("[Task_Power] No Wi-Fi config → Starting provisioning"));
                    uiController.showBootScreen();
                    networkHandler.startProvisioningAP(AP_SSID, AP_PASSWORD);
                    // Sau khi provisioning xong → restart
                    ESP.restart();
                }

                // Đã có Wi-Fi → Sync lần đầu
                xEventGroupSetBits(eventGroup, EVT_NETWORK_SYNC);
                xEventGroupWaitBits(eventGroup, EVT_NETWORK_DONE,
                                    pdTRUE, pdTRUE, pdMS_TO_TICKS(30000));
                break;
            }

            case WakeupCause::TIMER: {
                // === Thức dậy bởi Timer 5 phút → Sync ngầm ===
                Serial.println(F("[Task_Power] TIMER: Background sync"));

                // Kiểm tra alarm
                if (is_alarm_active && timeMgr.isAlarmTriggered(alarm_time)) {
                    Serial.println(F("[Task_Power] ALARM triggered!"));
                    // TODO: Phát hiệu ứng/âm thanh báo thức
                }

                // Sync Firebase
                xEventGroupSetBits(eventGroup, EVT_NETWORK_SYNC);
                xEventGroupWaitBits(eventGroup, EVT_NETWORK_DONE,
                                    pdTRUE, pdTRUE, pdMS_TO_TICKS(30000));
                break;
            }

            case WakeupCause::TOUCH: {
                // === Thức dậy bởi cảm biến chạm ===
                Serial.println(F("[Task_Power] TOUCH: User interaction"));

                if (co_tin_nhan) {
                    // Nhánh A: Có tin nhắn → Phát media
                    Serial.println(F("[Task_Power] Has message → Play media"));
                    xEventGroupSetBits(eventGroup, EVT_MEDIA_PLAY);
                    xEventGroupWaitBits(eventGroup, EVT_MEDIA_DONE,
                                        pdTRUE, pdTRUE, pdMS_TO_TICKS(60000));
                    co_tin_nhan = false;

                    // Báo Firebase đã đọc
                    xEventGroupSetBits(eventGroup, EVT_NETWORK_SYNC);
                    xEventGroupWaitBits(eventGroup, EVT_NETWORK_DONE,
                                        pdTRUE, pdTRUE, pdMS_TO_TICKS(30000));
                } else {
                    // Nhánh B: Không có tin nhắn → Pull-to-refresh
                    Serial.println(F("[Task_Power] No message → Pull-to-refresh"));
                    xEventGroupSetBits(eventGroup, EVT_NETWORK_SYNC);
                    xEventGroupWaitBits(eventGroup, EVT_NETWORK_DONE,
                                        pdTRUE, pdTRUE, pdMS_TO_TICKS(30000));

                    if (co_tin_nhan) {
                        // Phát hiện tin nhắn mới → Phát ngay
                        xEventGroupSetBits(eventGroup, EVT_MEDIA_PLAY);
                        xEventGroupWaitBits(eventGroup, EVT_MEDIA_DONE,
                                            pdTRUE, pdTRUE, pdMS_TO_TICKS(60000));
                        co_tin_nhan = false;
                    } else {
                        // Không có → Hiển thị đồng hồ 5 giây
                        uiController.showClock(CLOCK_DISPLAY_DURATION_MS);
                    }
                }
                break;
            }

            default:
                break;
        }

        // === Chuẩn bị ngủ ===
        // Cập nhật LED: nếu có tin nhắn chưa đọc → breathing, không thì tắt
        if (co_tin_nhan) {
            uiController.startBreathingLED();
        } else {
            uiController.stopBreathingLED();
        }

        // Tắt màn hình
        display.turnOff();

        // Ngắt Wi-Fi
        networkHandler.disconnectWiFi();

        // Cấu hình wakeup sources
        powerMgr.configureTimerWakeup(SLEEP_TIMER_US);
        powerMgr.configureTouchWakeup((gpio_num_t)PIN_TOUCH);

        boot_count++;

        // Đi ngủ (hàm này block cho đến khi thức dậy)
        powerMgr.enterLightSleep();

        // === Thức dậy → Quay lại đầu vòng lặp ===
    }
}

// ============================================================================
// Task 2: Task_NetworkHandler (Ưu tiên trung bình)
// ============================================================================
void Task_NetworkHandler(void* pvParameters) {
    Serial.println(F("[Task_Network] Started. Waiting for events..."));

    for (;;) {
        // Chờ event từ PowerManager
        xEventGroupWaitBits(eventGroup, EVT_NETWORK_SYNC,
                            pdTRUE, pdTRUE, portMAX_DELAY);

        Serial.println(F("[Task_Network] Sync requested"));

        // Kết nối Wi-Fi
        uint8_t retryCount = 0;
        WiFiConnectResult result = WiFiConnectResult::FAILED;

        while (retryCount < WIFI_RETRY_MAX) {
            result = networkHandler.connectFromNVS();
            if (result == WiFiConnectResult::CONNECTED) break;
            retryCount++;
            Serial.printf("[Task_Network] Wi-Fi retry %d/%d\n", retryCount, WIFI_RETRY_MAX);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }

        if (result != WiFiConnectResult::CONNECTED) {
            // Thử backup Wi-Fi
            char ssidBak[WIFI_SSID_MAX_LEN], passBak[WIFI_PASS_MAX_LEN];
            if (configMgr.loadBackupWiFi(ssidBak, passBak)) {
                result = networkHandler.connectWiFi(ssidBak, passBak);
            }

            if (result != WiFiConnectResult::CONNECTED) {
                Serial.println(F("[Task_Network] All Wi-Fi attempts failed"));
                // TODO: Có thể bật SoftAP ở đây nếu cần
                xEventGroupSetBits(eventGroup, EVT_NETWORK_DONE);
                continue;
            }
        }

        // Kiểm tra Firebase
        MessageCheckResult msg = networkHandler.checkFirebase(BOX_ID, FIREBASE_HOST);

        // Cập nhật alarm config
        if (msg.alarmTime.length() > 0) {
            strncpy(alarm_time, msg.alarmTime.c_str(), sizeof(alarm_time) - 1);
            is_alarm_active = msg.isAlarmActive;
            timezone_offset = msg.timezone;
            timeMgr.setTimezone(timezone_offset);
        }

        // Kiểm tra Wi-Fi mới từ Firebase (receiver đổi Wi-Fi)
        if (msg.newWifiSsid.length() > 0) {
            Serial.printf("[Task_Network] New Wi-Fi from Firebase: %s\n",
                          msg.newWifiSsid.c_str());

            // Backup Wi-Fi hiện tại trước khi đổi
            char currentSsid[WIFI_SSID_MAX_LEN], currentPass[WIFI_PASS_MAX_LEN];
            if (configMgr.loadWiFi(currentSsid, currentPass)) {
                configMgr.saveBackupWiFi(currentSsid, currentPass);
            }
            configMgr.saveWiFi(msg.newWifiSsid.c_str(), msg.newWifiPass.c_str());
            // Wi-Fi mới sẽ có hiệu lực ở lần kết nối tiếp theo
        }

        // Tải tin nhắn mới nếu có
        if (msg.hasNewMessage) {
            Serial.println(F("[Task_Network] New message! Downloading..."));

            bool videoOk = networkHandler.downloadFileToSD(
                msg.videoUrl.c_str(), SD_VIDEO_PATH);
            bool audioOk = networkHandler.downloadFileToSD(
                msg.voiceUrl.c_str(), SD_AUDIO_PATH);

            if (videoOk && audioOk) {
                co_tin_nhan = true;
                networkHandler.markMessageRead(BOX_ID, FIREBASE_HOST);
                Serial.println(F("[Task_Network] Download complete, message marked read"));
            } else {
                Serial.println(F("[Task_Network] ERROR: Download failed"));
            }
        }

        // Cập nhật trạng thái hộp
        networkHandler.updateBoxStatus(BOX_ID, FIREBASE_HOST, battery.getPercentage());

        // Báo hiệu hoàn thành
        xEventGroupSetBits(eventGroup, EVT_NETWORK_DONE);
    }
}

// ============================================================================
// Task 3: Task_MediaPlayer (Ưu tiên cao)
// ============================================================================
void Task_MediaPlayer(void* pvParameters) {
    Serial.println(F("[Task_Media] Started. Waiting for events..."));

    for (;;) {
        // Chờ event từ PowerManager
        xEventGroupWaitBits(eventGroup, EVT_MEDIA_PLAY,
                            pdTRUE, pdTRUE, portMAX_DELAY);

        Serial.println(F("[Task_Media] Playing message..."));

        // Tắt LED breathing trước khi phát
        uiController.stopBreathingLED();

        // Phát video + audio đồng bộ
        bool ok = mediaPlayer.playMessage(SD_VIDEO_PATH, SD_AUDIO_PATH);

        if (!ok) {
            Serial.println(F("[Task_Media] ERROR: Playback failed"));
            uiController.showError("Play Error");
            vTaskDelay(pdMS_TO_TICKS(2000));
        }

        // Tắt màn hình sau khi phát xong
        display.turnOff();

        // Báo hiệu hoàn thành
        xEventGroupSetBits(eventGroup, EVT_MEDIA_DONE);
    }
}

// ============================================================================
// Task 4: Task_UI_Controller (Ưu tiên thấp nhất)
// ============================================================================
void Task_UI_Controller(void* pvParameters) {
    Serial.println(F("[Task_UI] Started"));

    for (;;) {
        // Cập nhật hiệu ứng LED (breathing nếu đang active)
        uiController.updateLED();

        // Yield cho các task khác
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ============================================================================
// setup() — Khởi tạo phần cứng và tạo FreeRTOS Tasks
// ============================================================================
void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println(F("\n=== SENDLOVE BOX ==="));

    // --- Tạo SPI Mutex (chia sẻ giữa SD Card và TFT Display) ---
    spiMutex = xSemaphoreCreateMutex();
    if (spiMutex == nullptr) {
        Serial.println(F("FATAL: Cannot create SPI mutex"));
        while (1) delay(1000);
    }

    // --- Tạo Event Group ---
    eventGroup = xEventGroupCreate();
    if (eventGroup == nullptr) {
        Serial.println(F("FATAL: Cannot create Event Group"));
        while (1) delay(1000);
    }

    // --- Khởi tạo các module ---
    configMgr.init(NVS_NAMESPACE);
    battery.init(PIN_BATTERY_ADC, BATTERY_VOLTAGE_DIVIDER_RATIO);
    sdCard.init(PIN_SD_CS, spiMutex);
    display.init(PIN_TFT_BLK, spiMutex);
    powerMgr.init((gpio_num_t)PIN_TOUCH);

    networkHandler.init(&configMgr, &sdCard, &timeMgr);
    mediaPlayer.init(&sdCard, &display, PIN_I2S_BCLK, PIN_I2S_LRC, PIN_I2S_DOUT);
    uiController.init(PIN_LED, PIN_TOUCH, &display, &battery, &timeMgr);

    Serial.println(F("[Setup] All modules initialized"));

    // --- Tạo FreeRTOS Tasks ---
    xTaskCreate(Task_PowerManager,  "PowerMgr",  TASK_STACK_POWER_MANAGER,
                nullptr, TASK_PRIORITY_POWER_MANAGER, &hTaskPower);

    xTaskCreate(Task_NetworkHandler, "Network",   TASK_STACK_NETWORK,
                nullptr, TASK_PRIORITY_NETWORK, &hTaskNetwork);

    xTaskCreate(Task_MediaPlayer,   "Media",      TASK_STACK_MEDIA_PLAYER,
                nullptr, TASK_PRIORITY_MEDIA_PLAYER, &hTaskMedia);

    xTaskCreate(Task_UI_Controller, "UI",         TASK_STACK_UI_CONTROLLER,
                nullptr, TASK_PRIORITY_UI_CONTROLLER, &hTaskUI);

    Serial.println(F("[Setup] All tasks created. FreeRTOS scheduler running."));
}

// ============================================================================
// loop() — Trống (FreeRTOS quản lý)
// ============================================================================
void loop() {
    // Không sử dụng. Toàn bộ logic chạy trong các FreeRTOS Tasks.
    vTaskDelay(portMAX_DELAY);
}