#include "ConfigManager.h"
#include "DisplayDriver.h"
#include "LayoutEngine.h"
#include "MediaPlayer.h"
#include "NandStorage.h"
#include "NetworkManager.h"
#include "OtaHandler.h"
#include "PowerManager.h"
#include "UIController.h"
#include "config.h"
#include <Arduino.h>
#include <SPI.h>

// ============================================================================
// SENDLOVE BOX — Main Firmware (Phase 1)
// ============================================================================
// Kiến trúc FreeRTOS Event-Driven:
//   - Task_MediaPlayer: Decode + render video/ảnh từ NAND
//   - Task_UIController: Đọc touch sensor + gửi event chuyển slot
//   - Task_NetworkController: Phục vụ WebServer / Captive Portal
// ============================================================================
enum class SystemEvent : uint8_t { NONE, TOUCH_NEXT_SLOT, TOUCH_TOGGLE_MODE };

struct AppContext {
  DisplayDriver display;
  NandStorage nand;
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
  int8_t currentSlot = -1;
  uint32_t lastClockRender = 0;

  currentSlot = appCtx.nand.findFirstValidSlot();
  if (currentSlot >= 0 && currentAppState == AppState::STATE_VIDEO)
    appCtx.player.playSlot(currentSlot);

  for (;;) {
    SystemEvent event = SystemEvent::NONE;
    while (xQueueReceive(eventQueue, &event, 0) == pdTRUE) {
      if (event == SystemEvent::TOUCH_TOGGLE_MODE) {
        if (currentAppState == AppState::STATE_STANDBY) {
          currentAppState = AppState::STATE_VIDEO;
          appCtx.display.clear();
          if (currentSlot < 0)
            currentSlot = appCtx.nand.findFirstValidSlot();
          if (currentSlot >= 0)
            appCtx.player.playSlot(currentSlot);
        } else {
          int8_t next = appCtx.nand.findNextValidSlot(currentSlot);
          if (next >= 0) {
            currentSlot = next;
            appCtx.player.playSlot(currentSlot);
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
        (now - lastUserActivity >= activeSleepTimeoutMs)) {
      appCtx.powerManager.enterLightSleep(SLEEP_TIMER_US, &appCtx.display);

      delay(50);
      appCtx.network.ensureConnected();
      appCtx.network.triggerNtpSync();

      esp_sleep_wakeup_cause_t wakeupCause = esp_sleep_get_wakeup_cause();
      if (wakeupCause == ESP_SLEEP_WAKEUP_TIMER) {
        // Woke up by 5-minute timer (display remains off)
        // Allow 2 seconds for background NTP sync to run, then immediately re-enter sleep
        lastUserActivity = millis();
        activeSleepTimeoutMs = 2000;
      } else {
        // Woke up by Touch sensor interaction
        lastUserActivity = millis();
        activeSleepTimeoutMs = INACTIVITY_SLEEP_TIMEOUT_MS;
        currentAppState = AppState::STATE_STANDBY;
        forceStandbyRedraw = true;
      }
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

  if (!appCtx.nand.init(spiMutex)) {
    uint8_t errData[4] = {0};
    appCtx.nand.readRaw(0, errData, 4);
    char errMsg[64];
    sprintf(errMsg, "NAND Err!\n%02X %02X %02X %02X", errData[0], errData[1],
            errData[2], errData[3]);
    appCtx.display.showMessage(errMsg);
    while (1) {
      delay(100);
    }
  }

  appCtx.player.init(&appCtx.nand, &appCtx.display);
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