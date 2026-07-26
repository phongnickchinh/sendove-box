#include <Arduino.h>
#include <SPI.h>
#include "config.h"
#include "DisplayDriver.h"
#include "NandStorage.h"
#include "MediaPlayer.h"
#include "UIController.h"
#include "NetworkManager.h"
#include "LayoutEngine.h"
#include "OtaHandler.h"
#include "PowerManager.h"

// ============================================================================
// SENDLOVE BOX — Main Firmware (Phase 1)
// ============================================================================
// Kiến trúc FreeRTOS Event-Driven:
//   - Task_MediaPlayer: Decode + render video/ảnh từ NAND
//   - Task_UIController: Đọc touch sensor + gửi event chuyển slot
//
// Phần cứng Phase 1:
//   - ST7789 240x240 (LovyanGFX, Shared SPI2)
//   - W25Q128 NAND Flash (Shared SPI2)
//   - TTP223 Touch Sensor (GPIO 10)
// ============================================================================
// System Events
// ============================================================================
enum class SystemEvent : uint8_t {
    NONE,
    TOUCH_NEXT_SLOT,
    TOUCH_TOGGLE_MODE
};

DisplayDriver  display;
NandStorage    nand;
MediaPlayer    player;
UIController   ui;
NetworkManager network;
LayoutEngine   layoutEngine;
OtaHandler     otaHandler;
PowerManager   powerManager;

static uint32_t lastUserActivity = 0;
static volatile bool forceStandbyRedraw = false;

static SemaphoreHandle_t spiMutex = nullptr;
static QueueHandle_t     eventQueue = nullptr;

enum class AppState {
    STATE_STANDBY,
    STATE_VIDEO
};

AppState currentAppState = AppState::STATE_STANDBY;

const char* defaultLayoutJson = R"({
  "theme_name": "Pastel Marble Clock",
  "background": "",
  "widgets": [dùng font Chakra Petch 16 semibold
    { "type": "clock_time", "format": "HH:MM", "x": 120, "y": 95, "align": "center", "font": "ChakraPetch_48", "color": "#000000" },
    { "type": "clock_date", "format": "WEEKDAY, DD/MM/YYYY", "x": 120, "y": 150, "align": "center", "font": "ChakraPetch_16", "color": "#000000" },
    { "type": "wifi_icon", "x": 20, "y": 20, "color": "#000000" },
    { "type": "battery_icon", "x": 195, "y": 20, "color": "#000000" }
  ]
})";

void Task_MediaPlayer(void* pvParameters) {
    int8_t currentSlot = -1;
    uint32_t lastClockRender = 0;

    currentSlot = nand.findFirstValidSlot();
    if (currentSlot >= 0 && currentAppState == AppState::STATE_VIDEO) player.playSlot(currentSlot);

    for (;;) {
        SystemEvent event = SystemEvent::NONE;
        while (xQueueReceive(eventQueue, &event, 0) == pdTRUE) {
            if (event == SystemEvent::TOUCH_TOGGLE_MODE) {
                if (currentAppState == AppState::STATE_STANDBY) {
                    currentAppState = AppState::STATE_VIDEO;
                    display.clear();
                    if (currentSlot < 0) currentSlot = nand.findFirstValidSlot();
                    if (currentSlot >= 0) player.playSlot(currentSlot);
                } else {
                    int8_t next = nand.findNextValidSlot(currentSlot);
                    if (next >= 0) { 
                        currentSlot = next;
                        player.playSlot(currentSlot);
                    }
                }
            }
        }

        if (currentAppState == AppState::STATE_VIDEO) {
            if (!otaHandler.isUpdating()) {
                player.update();
            } else {
                vTaskDelay(pdMS_TO_TICKS(100));
            }
        } else if (currentAppState == AppState::STATE_STANDBY) {
            uint32_t now = millis();
            if (now - lastClockRender >= 1000 || forceStandbyRedraw) {
                bool fullRedraw = (lastClockRender == 0) || forceStandbyRedraw;
                layoutEngine.renderStandbyScreen(&display, &network, fullRedraw);
                lastClockRender = now;
                forceStandbyRedraw = false;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1)); 
    }
}

void Task_UIController(void* pvParameters) {
    uint32_t activeSleepTimeoutMs = INACTIVITY_SLEEP_TIMEOUT_MS;

    for (;;) {
        if (ui.isTouched()) {
            SystemEvent event = SystemEvent::TOUCH_TOGGLE_MODE;
            xQueueSend(eventQueue, &event, 0);
            lastUserActivity = millis();
            activeSleepTimeoutMs = INACTIVITY_SLEEP_TIMEOUT_MS;
        }

        uint32_t now = millis();
        if (!otaHandler.isUpdating() && (now - lastUserActivity >= activeSleepTimeoutMs)) {
            powerManager.enterLightSleep(SLEEP_TIMER_US, &display);

            delay(50);
            network.ensureConnected();

            lastUserActivity = millis();
            esp_sleep_wakeup_cause_t wakeupCause = esp_sleep_get_wakeup_cause();
            activeSleepTimeoutMs = (wakeupCause == ESP_SLEEP_WAKEUP_TIMER) ? (INACTIVITY_SLEEP_TIMEOUT_MS * 2) : INACTIVITY_SLEEP_TIMEOUT_MS;

            currentAppState = AppState::STATE_STANDBY;
            forceStandbyRedraw = true;
        }

        network.update();
        ui.updateLED();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void setup() {
    spiMutex = xSemaphoreCreateMutex();
    eventQueue = xQueueCreate(8, sizeof(SystemEvent));

    if (spiMutex == nullptr || eventQueue == nullptr) {
        while (1) delay(1000);
    }

    SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI, -1);

    display.init(spiMutex);
    display.setBacklight(BACKLIGHT_DAY_PERCENT);
    display.showMessage("Booting...");

    network.init();
    layoutEngine.loadConfig(defaultLayoutJson);

    if (!nand.init(spiMutex)) {
        uint8_t errData[4] = {0};
        nand.readRaw(0, errData, 4);
        char errMsg[64];
        sprintf(errMsg, "NAND Err!\n%02X %02X %02X %02X", errData[0], errData[1], errData[2], errData[3]);
        display.showMessage(errMsg);
        while (1) { delay(100); }
    }

    player.init(&nand, &display);
    ui.init(PIN_TOUCH, &display);
    powerManager.init((gpio_num_t)PIN_TOUCH);
    // TODO: Init battery when hardware ADC voltage divider is attached:
    // powerManager.initBattery(PIN_BATTERY_ADC);
    lastUserActivity = millis();

    ui.showBootScreen();

    {
        uint32_t wifiStart = millis();
        while (!network.isConnected() && (millis() - wifiStart < WIFI_CONNECT_TIMEOUT_MS)) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        if (network.isConnected()) {
            network.startWebServer(OTA_HOSTNAME);
            if (network.getWebServer() != nullptr) {
                otaHandler.registerRoutes(*network.getWebServer());
            }
        }
    }

    xTaskCreate(Task_MediaPlayer, "MediaPlayer", TASK_STACK_MEDIA_PLAYER, nullptr, TASK_PRIORITY_MEDIA_PLAYER, nullptr);
    xTaskCreate(Task_UIController, "UIController", TASK_STACK_UI_CONTROLLER, nullptr, TASK_PRIORITY_UI_CONTROLLER, nullptr);
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(500));
}