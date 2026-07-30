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
#include "UIController.h"
#include "config.h"
#include <Arduino.h>
#include <SPI.h>

// ============================================================================
// SENDLOVE BOX — Main Firmware (Phase 3A: Storage Abstraction Layer)
// ============================================================================
// Kiến trúc FreeRTOS Event-Driven:
//   - Task_MediaPlayer: Decode + render video/ảnh từ IStorageProvider (NAND / SD)
//   - Task_UIController: Đọc touch sensor + gửi event chuyển slot/item
//   - Task_NetworkController: Phục vụ WebServer / Captive Portal
// ============================================================================
enum class SystemEvent : uint8_t { NONE, TOUCH_NEXT_SLOT, TOUCH_TOGGLE_MODE };

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

enum class AppState { STATE_STANDBY, STATE_VIDEO };

AppState currentAppState = AppState::STATE_STANDBY;

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
  char currentId[32] = "";
  uint32_t lastClockRender = 0;

  if (appCtx.storage && appCtx.storage->getFirstValidIdentifier(currentId, sizeof(currentId))) {
    if (currentAppState == AppState::STATE_VIDEO) {
      appCtx.player.playItem(currentId);
    }
  }

  for (;;) {
    SystemEvent event = SystemEvent::NONE;
    while (xQueueReceive(eventQueue, &event, 0) == pdTRUE) {
      if (event == SystemEvent::TOUCH_TOGGLE_MODE) {
        if (currentAppState == AppState::STATE_STANDBY) {
          currentAppState = AppState::STATE_VIDEO;
          appCtx.display.clear();
          
          char unreadId[32] = "";
          if (appCtx.storage && appCtx.storage->getNextUnreadIdentifier(unreadId, sizeof(unreadId))) {
            strncpy(currentId, unreadId, sizeof(currentId) - 1);
            appCtx.player.playItem(currentId);
            appCtx.storage->markAsRead(currentId);
          } else {
            if (currentId[0] == '\0' && appCtx.storage) {
              appCtx.storage->getFirstValidIdentifier(currentId, sizeof(currentId));
            }
            if (currentId[0] != '\0') {
              appCtx.player.playItem(currentId);
            }
          }
        } else {
          char unreadId[32] = "";
          if (appCtx.storage && appCtx.storage->getNextUnreadIdentifier(unreadId, sizeof(unreadId))) {
            strncpy(currentId, unreadId, sizeof(currentId) - 1);
            appCtx.player.playItem(currentId);
            appCtx.storage->markAsRead(currentId);
          } else {
            char nextId[32] = "";
            if (appCtx.storage && appCtx.storage->getNextValidIdentifier(currentId, nextId, sizeof(nextId))) {
              strncpy(currentId, nextId, sizeof(currentId) - 1);
              appCtx.player.playItem(currentId);
            }
          }
        }
      }
    }

    if (currentAppState == AppState::STATE_VIDEO) {
      if (!appCtx.otaHandler.isUpdating()) {
        appCtx.player.update();
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
    }

    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

void Task_UIController(void *pvParameters) {
  uint32_t activeSleepTimeoutMs = INACTIVITY_SLEEP_TIMEOUT_MS;

  for (;;) {
    if (appCtx.ui.isTouched()) {
      SystemEvent event = SystemEvent::TOUCH_TOGGLE_MODE;
      xQueueSend(eventQueue, &event, 0);
      lastUserActivity = millis();
      activeSleepTimeoutMs = INACTIVITY_SLEEP_TIMEOUT_MS;
    }

    uint32_t now = millis();
    if (!appCtx.otaHandler.isUpdating() && !appCtx.network.isProvisioningActive() &&
        !appCtx.network.isFirebaseSyncing() &&
        (now - lastUserActivity >= activeSleepTimeoutMs)) {
      
      time_t nowSec = time(nullptr);
      uint32_t secToAlarm = appCtx.configManager.getSecondsToNextAlarm(nowSec);
      uint64_t sleepTimeUs = SLEEP_TIMER_US;
      if (secToAlarm != 0xFFFFFFFF && secToAlarm > 0) {
        uint64_t alarmUs = (uint64_t)secToAlarm * 1000000ULL;
        if (alarmUs < sleepTimeUs) {
          sleepTimeUs = alarmUs;
        }
      }

      // Dừng MediaPlayer giải phóng SPI/RAM và chuyển về Standby trước khi ngủ
      appCtx.player.stop();
      currentAppState = AppState::STATE_STANDBY;
      forceStandbyRedraw = true;

      appCtx.powerManager.enterLightSleep(sleepTimeUs, &appCtx.display);

      delay(50);
      esp_sleep_wakeup_cause_t wakeupCause = esp_sleep_get_wakeup_cause();
      if (wakeupCause != ESP_SLEEP_WAKEUP_TIMER) {
        // Touch Wakeup: Bật màn hình mượt mà và Render Standby UI NGAY LẬP TỨC (< 50ms!)
        appCtx.display.turnOn();
        currentAppState = AppState::STATE_STANDBY;
        forceStandbyRedraw = true;
        lastUserActivity = millis();
        activeSleepTimeoutMs = INACTIVITY_SLEEP_TIMEOUT_MS;
      } else {
        lastUserActivity = millis();
        activeSleepTimeoutMs = 2000;
      }

      // Chờ 200ms cho UI và SPIBus ổn định hoàn toàn trước khi kích hoạt task đồng bộ ngầm
      vTaskDelay(pdMS_TO_TICKS(200));

      // Thực hiện đồng bộ ngầm non-blocking sau khi thức dậy (cả Touch và Timer)
      appCtx.network.ensureConnected();
      appCtx.network.triggerNtpSync();
      uint8_t batPercent = appCtx.powerManager.getBatteryPercentage();
      bool isCharging = appCtx.powerManager.isCharging();
      appCtx.network.triggerFirebaseSync(batPercent, isCharging, appCtx.storage);
    }

    if (appCtx.network.isFirebaseSyncing()) {
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
    while (1)
      delay(1000);
  }

  SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI, -1);

  appCtx.display.init(spiMutex);
  appCtx.display.setBacklight(BACKLIGHT_DAY_PERCENT);
  appCtx.display.showMessage("Booting...");

  appCtx.network.init();
  appCtx.configManager.init(NVS_NAMESPACE);

  char wifiSsid[WIFI_SSID_MAX_LEN] = "";
  char wifiPass[WIFI_PASS_MAX_LEN] = "";

  if (!appCtx.configManager.loadWiFi(wifiSsid, wifiPass)) {
    strncpy(wifiSsid, DEFAULT_WIFI_SSID, sizeof(wifiSsid) - 1);
    strncpy(wifiPass, DEFAULT_WIFI_PASSWORD, sizeof(wifiPass) - 1);
  }

  appCtx.network.connectWiFi(wifiSsid, wifiPass);
  appCtx.layoutEngine.loadConfig(defaultLayoutJson);

#if ACTIVE_STORAGE_TYPE == STORAGE_TYPE_SD
  appCtx.storage = new SDStorageProvider();
#else
  appCtx.storage = new NandStorageProvider();
#endif

  if (!appCtx.storage->init(spiMutex)) {
    appCtx.display.showMessage("Storage Err!");
    while (1) {
      delay(100);
    }
  }

  appCtx.player.init(appCtx.storage, &appCtx.display);
  appCtx.ui.init(PIN_TOUCH, &appCtx.display);
  appCtx.powerManager.init((gpio_num_t)PIN_TOUCH);
  lastUserActivity = millis();

  appCtx.ui.showBootScreen();

  if (appCtx.network.isConnected()) {
    appCtx.network.triggerNtpSync();
    appCtx.network.startWebServer(OTA_HOSTNAME);
    if (appCtx.network.getWebServer() != nullptr) {
      appCtx.otaHandler.registerRoutes(*appCtx.network.getWebServer());
    }

    // Kích hoạt Firebase Sync ngầm ngay khi vừa nạp code/khởi động xong
    uint8_t batPercent = appCtx.powerManager.getBatteryPercentage();
    bool isCharging = appCtx.powerManager.isCharging();
    appCtx.network.triggerFirebaseSync(batPercent, isCharging, appCtx.storage);
  } else {
    appCtx.display.showMessage("Setup Wi-Fi:\nSendloveBox-Setup");
    appCtx.network.startProvisioningAP("SendloveBox-Setup");
  }

  xTaskCreate(Task_MediaPlayer, "MediaPlayer", TASK_STACK_MEDIA_PLAYER, nullptr,
              TASK_PRIORITY_MEDIA_PLAYER, nullptr);
  xTaskCreate(Task_UIController, "UIController", TASK_STACK_UI_CONTROLLER,
              nullptr, TASK_PRIORITY_UI_CONTROLLER, nullptr);
  xTaskCreate(Task_NetworkController, "NetworkController", TASK_STACK_NETWORK,
              nullptr, TASK_PRIORITY_NETWORK, nullptr);
}

void loop() { vTaskDelay(pdMS_TO_TICKS(500)); }