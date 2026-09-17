#include "ConfigManager.h"
#include "DisplayDriver.h"
#include "IStorageProvider.h"
#include "NandStorageProvider.h"
#include "SDStorageProvider.h"
#include "LayoutEngine.h"
#include "MediaPlayer.h"
#include "NetworkManager.h"
#include "OtaHandler.h"
#include "PowerManager.h"
#include "ScreenLogger.h"
#include "UIController.h"
#include "config.h"
#include <Arduino.h>
#include <SPI.h>
#include <atomic>

// ============================================================================
// SENDLOVE BOX — Main Firmware (Phase 3A: Storage Abstraction Layer)
// ============================================================================
// Kiến trúc FreeRTOS Event-Driven:
//   - Task_MediaPlayer: Decode + render video/ảnh từ IStorageProvider (NAND / SD)
//   - Task_UIController: Đọc touch sensor + gửi event chuyển slot/item
//   - Task_NetworkController: Phục vụ WebServer / Captive Portal
// ============================================================================
enum class SystemEvent : uint8_t { NONE, TOUCH_SHORT, TOUCH_LONG, TIMEOUT_AUTO_NEXT };

struct AppContext {
  DisplayDriver display;
  IStorageProvider* storage = nullptr;
  MediaPlayer player;
  UIController ui;
  NetworkManager network;
  LayoutEngine layoutEngine;
  OtaHandler otaHandler;
  PowerManager powerManager;
  ConfigManager configManager;
};

static AppContext appCtx;

static uint32_t lastUserActivity = 0;
static volatile bool forceStandbyRedraw = false;

static SemaphoreHandle_t spiMutex = nullptr;
static QueueHandle_t eventQueue = nullptr;

// Serial Monitor đã được thay thế hoàn toàn bằng ScreenLogger on-screen overlay.
// Không dng Serial.begin() để tránh block chip khi không có USB CDC.

enum class AppState { STATE_STANDBY, STATE_VIDEO };

// Đọc/ghi từ 3 task (MediaPlayer, UIController, vòng lặp chính) nên phải atomic.
// 18 chỗ dùng đều là so sánh/gán trực tiếp (đã grep), không chỗ nào bind qua `auto`,
// nên operator T() / operator= ngầm phủ hết, không cần sửa chỗ nào khác.
//
// KHÔNG lock-free: ESP32-C3 là RV32IMC, thiếu extension 'A' cho atomic sub-word.
// Link được là nhờ ESP-IDF cấp sẵn bản emulation (đã verify bằng nm:
// __atomic_load_1/store_1/exchange_1 đều là 'T' trong sdk/esp32c3/lib/libnewlib.a).
// Emulation chạy bằng cách tắt ngắt — rẻ, nhưng đừng gọi từ ISR. Hiện không chỗ nào
// gọi từ ISR: main.cpp không có IRAM_ATTR nào, chỗ duy nhất trông giống callback
// (setPlaybackActiveCallback) chạy trong task context.
std::atomic<AppState> currentAppState{AppState::STATE_STANDBY};

const char *defaultLayoutJson = R"({
  "theme_name": "Default Card Theme",
  "background": "bg_defaut",
  "widgets": [
    { "type": "clock_date", "format": "WEEKDAY, DD.MM", "x": 9, "y": 9, "w": 140, "h": 16, "align": "left", "font": "ChakraPetch_16", "color": "#B83D3D" },
    { "type": "clock_time", "format": "HH:MM", "x": 50, "y": 30, "w": 160, "h": 45, "align": "center", "font": "ChakraPetch_48", "color": "#000000" },
    { "type": "battery_icon", "x": 154, "y": 10, "w": 75, "h": 16 }
  ]
})";

void Task_MediaPlayer(void *pvParameters) {
  DLOG("[PLAY] task started");
  char currentId[32] = "";
  uint32_t lastClockRender = 0;
  uint32_t playStartTime = 0;

  if (appCtx.storage && appCtx.storage->getFirstValidIdentifier(currentId, sizeof(currentId))) {
    if (currentAppState == AppState::STATE_VIDEO) {
      appCtx.player.playItem(currentId);
      playStartTime = millis();
    }
  }

  auto drawToast = [](const char* msg) {
      appCtx.layoutEngine.renderStandbyScreen(&appCtx.display, &appCtx.network, true);
      if (appCtx.display.acquireSPI()) {
          appCtx.display.getTFT()->fillRect(0, 200, 240, 40, TFT_BLACK);
          appCtx.display.getTFT()->setTextColor(TFT_WHITE);
          appCtx.display.getTFT()->setTextDatum(lgfx::middle_center);
          appCtx.display.getTFT()->setTextSize(1);
          appCtx.display.getTFT()->drawString(msg, 120, 220);
          appCtx.display.releaseSPI();
      }
  };

  for (;;) {
    SystemEvent event = SystemEvent::NONE;
    while (xQueueReceive(eventQueue, &event, 0) == pdTRUE) {
      // event loop — không log tại đây để tránh spam màn hình
      
      if (event == SystemEvent::TOUCH_SHORT) {
        if (currentAppState == AppState::STATE_STANDBY) {
           if (appCtx.network.isDownloadingMedia()) {
               appCtx.player.stop();
               drawToast("Downloading...");
           } else {
               char unreadId[32] = "";
               if (appCtx.network.getNumOfNewMsg() > 0) {
                  if (appCtx.storage && appCtx.storage->getNextUnreadIdentifier(unreadId, sizeof(unreadId))) {
                      currentAppState = AppState::STATE_VIDEO;
                      appCtx.display.clear();
                      strncpy(currentId, unreadId, sizeof(currentId) - 1);
                      if (appCtx.player.playItem(currentId)) {
                          playStartTime = millis();
                      } else {
                          DLOG("[PLAY] FAIL -> STANDBY");
                          currentAppState = AppState::STATE_STANDBY;
                          forceStandbyRedraw = true;
                      }
                  } else {
                      DLOG("[PLAY] no local unread, downloading...");
                      appCtx.player.stop();
                      drawToast("Downloading...");
                      uint8_t batPercent = appCtx.powerManager.getBatteryPercentage();
                      bool isCharging = appCtx.powerManager.isCharging();
                      appCtx.network.triggerFirebaseSync(batPercent, isCharging, appCtx.storage);
                  }
               } else {
                  appCtx.display.turnOn();
                  drawToast("No new messages");
                  vTaskDelay(1000);
                  forceStandbyRedraw = true;
               }
           }
        } else if (currentAppState == AppState::STATE_VIDEO) {
           if (event == SystemEvent::TOUCH_SHORT) {
               appCtx.storage->markAsRead(currentId);
               appCtx.network.decrementNewMsgCount();
           }

           char unreadId[32] = "";
           if (appCtx.network.getNumOfNewMsg() > 0) {
               if (appCtx.storage && appCtx.storage->getNextUnreadIdentifier(unreadId, sizeof(unreadId))) {
                   strncpy(currentId, unreadId, sizeof(currentId) - 1);
                   if (appCtx.player.playItem(currentId)) {
                       playStartTime = millis();
                   } else {
                       DLOG("[PLAY] FAIL -> STANDBY");
                       currentAppState = AppState::STATE_STANDBY;
                       forceStandbyRedraw = true;
                   }
               } else {
                   appCtx.player.stop();
                   currentAppState = AppState::STATE_STANDBY;
                   drawToast("Downloading...");
                   uint8_t batPercent = appCtx.powerManager.getBatteryPercentage();
                   bool isCharging = appCtx.powerManager.isCharging();
                   appCtx.network.triggerFirebaseSync(batPercent, isCharging, appCtx.storage);
               }
           } else {
               appCtx.player.stop();
               if (appCtx.display.acquireSPI()) {
                   appCtx.display.getTFT()->fillRect(0, 200, 240, 40, TFT_BLACK);
                   appCtx.display.getTFT()->setTextColor(TFT_WHITE);
                   appCtx.display.getTFT()->setTextDatum(lgfx::middle_center);
                   appCtx.display.getTFT()->setTextSize(1);
                   appCtx.display.getTFT()->drawString("Reached newest msg", 120, 220);
                   appCtx.display.releaseSPI();
               }
               vTaskDelay(1500);
               currentAppState = AppState::STATE_STANDBY;
               forceStandbyRedraw = true;
           }
        }
      } else if (event == SystemEvent::TOUCH_LONG || event == SystemEvent::TIMEOUT_AUTO_NEXT) {
        if (currentAppState == AppState::STATE_VIDEO) {
           appCtx.player.stop();
           currentAppState = AppState::STATE_STANDBY;
           forceStandbyRedraw = true;
           lastUserActivity = millis();
        }
      }
    }

    if (currentAppState == AppState::STATE_VIDEO) {
      if (!appCtx.otaHandler.isUpdating()) {
        appCtx.player.update();
        
        if (playStartTime > 0 && appCtx.storage) {
            StorageItemInfo info = appCtx.storage->getItemInfo(currentId);
            uint32_t maxDispMs = (info.maxDisplayTime > 0 ? info.maxDisplayTime : 60) * 1000;
            if (millis() - playStartTime >= maxDispMs) {
                playStartTime = 0; 
                SystemEvent timeoutEv = SystemEvent::TIMEOUT_AUTO_NEXT;
                xQueueSend(eventQueue, &timeoutEv, 0);
            }
        }
      } else {
        vTaskDelay(pdMS_TO_TICKS(100));
      }
    } else if (currentAppState == AppState::STATE_STANDBY) {
      uint32_t now = millis();
      if (now - lastClockRender >= 1000 || forceStandbyRedraw) {
        bool fullRedraw = (lastClockRender == 0) || forceStandbyRedraw;
        appCtx.layoutEngine.renderStandbyScreen(&appCtx.display, &appCtx.network, fullRedraw);
        lastClockRender = now;
        forceStandbyRedraw = false;
      }
      vTaskDelay(pdMS_TO_TICKS(10));
    }
  }
}

void Task_UIController(void *pvParameters) {
  DLOG("[UI] task started");
  uint32_t activeSleepTimeoutMs = INACTIVITY_SLEEP_TIMEOUT_MS;

  for (;;) {
    TouchEvent tEvent = appCtx.ui.getTouchEvent();
    if (tEvent != TouchEvent::NONE) {
      if (appCtx.network.isDownloadingMedia()) {
          // Bỏ qua touch khi đang download — không log để tránh spam
      } else {
          SystemEvent event = (tEvent == TouchEvent::LONG_PRESS) ? SystemEvent::TOUCH_LONG : SystemEvent::TOUCH_SHORT;
          xQueueSend(eventQueue, &event, 0);
          lastUserActivity = millis();
          activeSleepTimeoutMs = INACTIVITY_SLEEP_TIMEOUT_MS;
      }
    }

    uint32_t now = millis();
    static uint32_t lastIntervalSyncMs = millis();

    // Trong luc sync chay ngam, day moc thoi gian theo -> chu ky 10s duoc tinh
    // tu luc sync KET THUC, thay vi tu luc bat dau (tai xong 30s roi sync lai ngay).
    if (appCtx.network.isSyncing()) {
        lastIntervalSyncMs = now;
    }

    // Periodic check if device is kept awake in Standby UI (every 10s)
    //
    // KHÔNG còn điều kiện `!isStorageFull` ở đây: đầy slot chỉ có nghĩa là khỏi
    // tải tin, không có nghĩa là ngừng heartbeat / đọc cờ / đồng bộ báo thức.
    // Cổng đó đã chuyển xuống đúng bước tải tin trong syncWakeup(). Đánh đổi đã
    // biết: box đầy slot giờ vẫn sync mỗi 10s nên tốn pin hơn trước.
    if (now - lastIntervalSyncMs >= 10000 && !appCtx.network.isSyncing() && currentAppState == AppState::STATE_STANDBY) {
        lastIntervalSyncMs = now;
        uint8_t batPercent = appCtx.powerManager.getBatteryPercentage();
        bool isCharging = appCtx.powerManager.isCharging();
        appCtx.network.triggerFirebaseSync(batPercent, isCharging, appCtx.storage);
    }

    if (currentAppState != AppState::STATE_VIDEO &&
        !appCtx.otaHandler.isUpdating() && !appCtx.network.isProvisioningActive() &&
        !appCtx.network.isSyncing() &&
        (now - lastUserActivity >= activeSleepTimeoutMs)) {
      DLOG("[SLP] timeout -> sleeping");
      
      time_t nowSec = time(nullptr);
      uint32_t secToAlarm = appCtx.configManager.getSecondsToNextAlarm(nowSec);
      uint64_t sleepTimeUs = SLEEP_TIMER_US;
      if (secToAlarm != 0xFFFFFFFF && secToAlarm > 0) {
        uint64_t alarmUs = (uint64_t)secToAlarm * 1000000ULL;
        if (alarmUs < sleepTimeUs) {
          sleepTimeUs = alarmUs;
        }
      }

      DLOG("[SLP] sleep %llus", (unsigned long long)(sleepTimeUs / 1000000ULL));

      // Dừng MediaPlayer giải phóng SPI/RAM và chuyển về Standby trước khi ngủ
      appCtx.player.stop();
      currentAppState = AppState::STATE_STANDBY;
      // Không set forceStandbyRedraw ở đây để tránh race condition với Task_MediaPlayer
      
      appCtx.powerManager.enterLightSleep(sleepTimeUs, &appCtx.display);

      // Vừa tỉnh dậy: đánh dấu để lần ensureConnected() kế tiếp ÉP tái lập
      // association. WiFi.status() sau light sleep thường vẫn báo WL_CONNECTED
      // dù association đã chết -> nếu tin nó thì mọi http.GET() đều trả -1.
      appCtx.network.notifyWakeFromSleep();

      // Nháy đèn nền 3 lần NGAY LẬP TỨC sau khi thức dậy.
      // Gọi ở đây (trước delay) để đảm bảo các FreeRTOS task khác chưa resume,
      // SPI bus chưa có xung đột, GPIO an toàn để toggle.
      appCtx.display.wakeupFlash();

      delay(200);
      esp_sleep_wakeup_cause_t wakeupCause = esp_sleep_get_wakeup_cause();
      const char* causeStr = (wakeupCause == ESP_SLEEP_WAKEUP_TIMER) ? "timer" : "touch";
      DLOG("[SLP] wakeup=%s", causeStr);

      if (wakeupCause != ESP_SLEEP_WAKEUP_TIMER) {
        // Touch Wakeup: Re-init màn hình và render Standby UI.
        currentAppState = AppState::STATE_STANDBY;
        appCtx.display.turnOn();
        // currentAppState = AppState::STATE_STANDBY;
        forceStandbyRedraw = true;
        lastUserActivity = millis();
        activeSleepTimeoutMs = INACTIVITY_SLEEP_TIMEOUT_MS;
      } else {
        lastUserActivity = millis();
        activeSleepTimeoutMs = 2000;

        // Nháy đèn xanh dương (GPIO 8 - Bản SuperMini, trùng chân NAND CS) để báo hiệu wakeup ngầm.
        // Đây là chỉ báo timer-wake DUY NHẤT còn lại: wakeupFlash() (DisplayDriver.cpp:171)
        // giờ chỉ gọi gpio_hold_dis(), tên hàm đã lỗi thời, không nháy gì cả. User chốt giữ đèn.
        //
        // PHẢI giữ spiMutex suốt đoạn nháy. Comment cũ ghi "bus SPI hoàn toàn rảnh" là SAI:
        // Task_MediaPlayer có thể đang đẩy pixel lên SCK/MOSI, mà GPIO 8 chính là CS của
        // W25Q128. Ghim CS xuống LOW 30ms trong lúc có xung clock -> NAND chốt nhầm opcode.
        // Giữ mutex triệt tiêu đúng cơ chế đó (CS LOW mà không có clock là vô hại).
        // Chi phí: giữ mutex 160ms, cộng tối đa 1000ms chờ (timeout mặc định của
        // acquireSPI) trong trường hợp xấu. Lúc vừa thức thì SPI thường rảnh.
        if (appCtx.display.acquireSPI()) {
          pinMode(8, OUTPUT);
          digitalWrite(8, LOW);  // Đèn sáng (Active LOW) / NAND CS ghim xuống
          delay(30);
          digitalWrite(8, HIGH); // Đèn tắt / NAND CS nhả ra
          delay(70);
          digitalWrite(8, LOW);
          delay(30);
          digitalWrite(8, HIGH);
          appCtx.display.releaseSPI();
        } else {
          // Bắt buộc phải log: đèn này là chỉ báo timer-wake duy nhất, nên "không
          // nháy mà không nói gì" sẽ bị hiểu nhầm là B1 làm hỏng đèn.
          DLOG("[WAKE] blink skip: spi busy");
        }
      }

      // Chờ 200ms cho UI và SPIBus ổn định hoàn toàn trước khi kích hoạt task đồng bộ ngầm
      vTaskDelay(pdMS_TO_TICKS(200));

      // Thực hiện đồng bộ ngầm non-blocking sau khi thức dậy.
      // Luôn check tin mới + tải đầy đủ vào slot trước khi cho phát — không còn
      // nhánh "có tin local sẵn thì hoãn sync" (dễ bỏ sót tin mới trên Cloud).
      // Luôn sync, kể cả khi đầy slot. Trước 2026-09-05 chỗ này bỏ qua toàn bộ
      // chu kỳ khi đầy ("post-wakeup sync skip: FULL"), làm box mất báo thức và
      // OTA cho tới khi có slot trống. Cổng "đầy" giờ nằm trong syncWakeup(),
      // chỉ chặn đúng bước tải tin.
      // syncWakeup() da bao gom ensureConnected() + syncNtpTime().
      uint8_t batPercent = appCtx.powerManager.getBatteryPercentage();
      bool isCharging = appCtx.powerManager.isCharging();
      appCtx.network.triggerFirebaseSync(batPercent, isCharging, appCtx.storage);
      lastIntervalSyncMs = millis();
    }

    if (appCtx.network.isDownloadingMedia()) {
      lastUserActivity = millis();
    }

    appCtx.ui.updateLED();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void Task_NetworkController(void *pvParameters) {
  for (;;) {
    appCtx.network.update();
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void setup() {
  spiMutex = xSemaphoreCreateMutex();
  eventQueue = xQueueCreate(8, sizeof(SystemEvent));

  if (spiMutex == nullptr || eventQueue == nullptr) {
    // Không có display/logger ở thời điểm này, chỉ blink đèn nền để báo lỗi.
    while (1) delay(1000);
  }

  SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI, -1);

  appCtx.display.init(spiMutex);
  appCtx.display.setBacklight(BACKLIGHT_DAY_PERCENT);
  appCtx.display.showMessage("Booting...");

  // Khởi tạo ScreenLogger sau khi display đã sẵn sàng
  ScreenLogger::init(&appCtx.display);
  DLOG("[BOOT] cpu=%uMHz heap=%u", ESP.getCpuFreqMHz(), ESP.getFreeHeap());
  DLOG("[BOOT] wakeup=%d", (int)esp_sleep_get_wakeup_cause());
  // reset=1 POWERON, 3 SW, 4 INT_WDT, 5 TASK_WDT, 6 WDT, 9 BROWNOUT, 12 PANIC.
  // Nếu dòng này lặp lại đều đặn trong log ⇒ box đang reset vòng lặp, không phải lỗi audio.
  DLOG("[BOOT] reset=%d", (int)esp_reset_reason());

  appCtx.network.init();
  appCtx.configManager.init(NVS_NAMESPACE);

  char wifiSsid[WIFI_SSID_MAX_LEN] = "";
  char wifiPass[WIFI_PASS_MAX_LEN] = "";

  bool credsFromNvs = appCtx.configManager.loadWiFi(wifiSsid, wifiPass);
  if (!credsFromNvs) {
    strncpy(wifiSsid, DEFAULT_WIFI_SSID, sizeof(wifiSsid) - 1);
    strncpy(wifiPass, DEFAULT_WIFI_PASSWORD, sizeof(wifiPass) - 1);
  }

  // Log rõ nguồn credentials: trước đây chỉ in SSID nên không phân biệt được
  // "đang dùng creds mặc định" với "đang dùng creds cũ còn sót trong NVS".
  DLOG("[BOOT] WiFi: %s (%s)", wifiSsid, credsFromNvs ? "NVS" : "default");

  if (appCtx.network.connectWiFi(wifiSsid, wifiPass) != WiFiConnectResult::CONNECTED && credsFromNvs) {
    // Creds trong NVS (lưu từ lần provisioning trước) có thể đã cũ/sai — ví dụ
    // đổi mật khẩu router. Trước đây hễ NVS có BẤT KỲ SSID nào thì creds mặc định
    // trong config.h không bao giờ được thử tới, nên box đi thẳng vào AP mode dù
    // creds mặc định vẫn dùng được. Thử nốt trước khi bỏ cuộc.
    DLOG("[BOOT] NVS creds fail -> try default");
    strncpy(wifiSsid, DEFAULT_WIFI_SSID, sizeof(wifiSsid) - 1);
    strncpy(wifiPass, DEFAULT_WIFI_PASSWORD, sizeof(wifiPass) - 1);
    appCtx.network.connectWiFi(wifiSsid, wifiPass);
  }
  appCtx.layoutEngine.loadConfig(defaultLayoutJson);

#if ACTIVE_STORAGE_TYPE == STORAGE_TYPE_SD
  DLOG("[BOOT] storage: SD");
  appCtx.storage = new SDStorageProvider();
#else
  DLOG("[BOOT] storage: NAND");
  appCtx.storage = new NandStorageProvider();
#endif

  if (!appCtx.storage->init(spiMutex)) {
    DLOG("[BOOT] ERR: storage init FAIL");
    appCtx.display.showMessage("Storage Err!");
    while (1) {
      delay(100);
    }
  }

#if ERASE_NOR_ON_BOOT == 1
  DLOG("[BOOT] erasing NOR flash...");
  appCtx.display.showMessage("Erasing NOR...");
  appCtx.storage->formatStorage();
  delay(1000);
#endif

  appCtx.player.init(appCtx.storage, &appCtx.display);
  
  // Phát beep test loa khi khởi động
  appCtx.player.testAudioBeep();
  
  appCtx.ui.init(PIN_TOUCH, &appCtx.display);
  appCtx.powerManager.init((gpio_num_t)PIN_TOUCH);
  lastUserActivity = millis();
  appCtx.ui.showBootScreen();

  if (appCtx.network.isConnected()) {
    DLOG("[BOOT] WiFi OK -> NTP+Firebase");
    appCtx.network.startWebServer(OTA_HOSTNAME);
    if (appCtx.network.getWebServer() != nullptr) {
      appCtx.otaHandler.registerRoutes(*appCtx.network.getWebServer());
    }

    appCtx.network.setOnDownloadComplete([]() {
      forceStandbyRedraw = true;
    });

    appCtx.network.setPlaybackActiveCallback([]() {
      return (currentAppState == AppState::STATE_VIDEO);
    });

    // Kích hoạt Firebase Sync ngầm ngay khi vừa nạp code/khởi động xong
    uint8_t batPercent = appCtx.powerManager.getBatteryPercentage();
    bool isCharging = appCtx.powerManager.isCharging();
    appCtx.network.triggerFirebaseSync(batPercent, isCharging, appCtx.storage);
  } else {
    DLOG("[BOOT] no WiFi -> AP mode");
    appCtx.display.showMessage("Setup Wi-Fi:\nSendloveBox-Setup");
    appCtx.network.startProvisioningAP("SendloveBox-Setup");
  }

  xTaskCreate(Task_MediaPlayer, "MediaPlayer", TASK_STACK_MEDIA_PLAYER, nullptr,
              TASK_PRIORITY_MEDIA_PLAYER, nullptr);
  xTaskCreate(Task_UIController, "UIController", TASK_STACK_UI_CONTROLLER,
              nullptr, TASK_PRIORITY_UI_CONTROLLER, nullptr);
  xTaskCreate(Task_NetworkController, "NetworkController", TASK_STACK_NETWORK,
              nullptr, TASK_PRIORITY_NETWORK, nullptr);

  DLOG("[BOOT] setup complete");
}

void loop() { vTaskDelay(pdMS_TO_TICKS(500)); }