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

// ============================================================================
// Global Objects
// ============================================================================
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

// --- Shared Resources ---
static SemaphoreHandle_t spiMutex = nullptr;
static QueueHandle_t     eventQueue = nullptr;

enum class AppState {
    STATE_STANDBY,
    STATE_VIDEO
};

AppState currentAppState = AppState::STATE_STANDBY;

const char* defaultLayoutJson = R"({
  "theme_name": "Cyberpunk Clock",
  "background": "",
  "widgets": [
    { "type": "clock_time", "format": "HH:MM", "x": 120, "y": 90, "align": "center", "font": "Orbitron_32", "color": "#00FFFF" },
    { "type": "clock_date", "format": "WEEKDAY, DD/MM/YYYY", "x": 120, "y": 150, "align": "center", "font": "Roboto_14", "color": "#FFFFFF" },
    { "type": "wifi_icon", "x": 10, "y": 10, "color": "#00FF00" },
    { "type": "battery_icon", "x": 200, "y": 10, "color": "#00FF00" }
  ]
})";

// ============================================================================
// Task: Media Player & Standby Renderer
// ============================================================================
// Chạy trên Core 0 hoặc 1 (ESP32-C3 chỉ có 1 core)
// Ưu tiên cao để giải mã JPEG không bị giật, HOẶC render màn hình chờ
// ============================================================================
void Task_MediaPlayer(void* pvParameters) {
    Serial.println(F("[Task] MediaPlayer/Renderer started"));

    int8_t currentSlot = -1;
    uint32_t lastClockRender = 0;

    // Tìm slot hợp lệ đầu tiên
    currentSlot = nand.findFirstValidSlot();
    if (currentSlot >= 0 && currentAppState == AppState::STATE_VIDEO) {
        player.playSlot(currentSlot);
    }

    for (;;) {
        // Xử lý TẤT CẢ các event trong queue trước khi decode frame mới
        SystemEvent event = SystemEvent::NONE;
        while (xQueueReceive(eventQueue, &event, 0) == pdTRUE) {
            if (event == SystemEvent::TOUCH_TOGGLE_MODE) {
                if (currentAppState == AppState::STATE_STANDBY) {
                    currentAppState = AppState::STATE_VIDEO;
                    Serial.println(F("[Task] Switched to VIDEO mode"));
                    display.clear();
                    if (currentSlot < 0) currentSlot = nand.findFirstValidSlot();
                    if (currentSlot >= 0) player.playSlot(currentSlot);
                } else {
                    // Đang ở Video -> Chuyển qua video kế tiếp HOẶC về Standby
                    // Tạm thời cứ chạm là chuyển bài. Để thoát ra standby cần 1 event khác (vd: long press).
                    // Ở đây mô phỏng: nếu chạm, chuyển video tiếp.
                    int8_t next = nand.findNextValidSlot(currentSlot);
                    if (next >= 0) { 
                        currentSlot = next;
                        player.playSlot(currentSlot);
                        Serial.printf("[Task] Switched to slot %d\n", currentSlot);
                    }
                }
            }
        }

        if (currentAppState == AppState::STATE_VIDEO) {
            // Tạm dừng decode khi OTA đang chạy (nhường CPU + tránh xung đột SPI)
            if (!otaHandler.isUpdating()) {
                player.update();
            } else {
                vTaskDelay(pdMS_TO_TICKS(100)); // Nhường CPU cho OTA
            }
        } else if (currentAppState == AppState::STATE_STANDBY) {
            // Cập nhật giờ/ngày mỗi giây HOẶC khi vừa tỉnh dậy từ Light Sleep
            uint32_t now = millis();
            if (now - lastClockRender >= 1000 || forceStandbyRedraw) {
                bool fullRedraw = (lastClockRender == 0) || forceStandbyRedraw;
                layoutEngine.renderStandbyScreen(&display, &network, fullRedraw);
                lastClockRender = now;
                forceStandbyRedraw = false;
            }
        }

        // [QUAN TRỌNG] Yield để Task Priority thấp hơn có thời gian chạy.
        vTaskDelay(pdMS_TO_TICKS(1)); 
    }
}

// ============================================================================
// Task: UI Controller
// ============================================================================
void Task_UIController(void* pvParameters) {
    Serial.println(F("[Task] UIController started"));

    uint32_t activeSleepTimeoutMs = INACTIVITY_SLEEP_TIMEOUT_MS;

    for (;;) {
        // Kiểm tra touch
        if (ui.isTouched()) {
            SystemEvent event = SystemEvent::TOUCH_TOGGLE_MODE;
            xQueueSend(eventQueue, &event, 0);
            lastUserActivity = millis(); // Cập nhật thời gian tương tác gần nhất
            activeSleepTimeoutMs = INACTIVITY_SLEEP_TIMEOUT_MS;
            Serial.println(F("[UI] Touch detected"));
        }

        // Kiểm tra tự động ngủ khi không chạm trong thời gian chỉ định (Inactivity Timeout)
        uint32_t now = millis();
        if (!otaHandler.isUpdating() && (now - lastUserActivity >= activeSleepTimeoutMs)) {
            Serial.printf("[Power] Inactivity timeout (%u ms) -> Entering Light Sleep...\n", activeSleepTimeoutMs);
            powerManager.enterLightSleep(SLEEP_TIMER_US, &display);

            // === SAU KHI TỈNH DẬY TỪ LIGHT SLEEP ===
            delay(50); // Chờ USB CDC re-enumerate trên ESP32-C3

            // Ép Wi-Fi khôi phục phát sóng RF ngắt kết nối ngay khi tỉnh dậy
            network.ensureConnected();

            lastUserActivity = millis();
            esp_sleep_wakeup_cause_t wakeupCause = esp_sleep_get_wakeup_cause();
            if (wakeupCause == ESP_SLEEP_WAKEUP_TIMER) {
                // Thức dậy do Timer ngầm: Dành 30 giây cho Wi-Fi reconnect & nhận OTA/Firebase ngầm
                activeSleepTimeoutMs = INACTIVITY_SLEEP_TIMEOUT_MS*2;
            } else {
                activeSleepTimeoutMs = INACTIVITY_SLEEP_TIMEOUT_MS;
            }

            currentAppState = AppState::STATE_STANDBY; // Luôn trở về màn hình chờ khi tỉnh dậy
            forceStandbyRedraw = true;                 // Yêu cầu vẽ lại toàn bộ Standby UI ngay lập tức
            if (Serial) {
                Serial.println(F("[Power] Restored to STANDBY mode after wakeup."));
            }
        }

        // Cập nhật network dưới nền (NTP, WiFi status)
        network.update();

        ui.updateLED();

        vTaskDelay(pdMS_TO_TICKS(10)); // Poll mỗi 10ms
    }
}

// ============================================================================
// Setup
// ============================================================================
void setup() {
    Serial.setRxBufferSize(4096); 
    Serial.begin(115200);
    delay(2000); // Chờ USB CDC enum trên PC

    Serial.println(F("\n========================================"));
    Serial.println(F("  SENDLOVE BOX — Phase 2"));
    Serial.println(F("  Standby UI & Network Time"));
    Serial.println(F("========================================\n"));

    // --- Tạo shared resources ---
    spiMutex = xSemaphoreCreateMutex();
    eventQueue = xQueueCreate(8, sizeof(SystemEvent));

    if (spiMutex == nullptr || eventQueue == nullptr) {
        Serial.println(F("[FATAL] Failed to create mutex or queue"));
        while (1) delay(1000);
    }

    // --- Khởi tạo Hardware SPI2 ---
    SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI, -1);

    // --- Khởi tạo modules ---
    Serial.println(F("[Init] Display..."));
    display.init(spiMutex);
    display.setBacklight(BACKLIGHT_DAY_PERCENT);
    display.showMessage("Booting...");

    Serial.println(F("[Init] Network & Time..."));
    network.init();

    Serial.println(F("[Init] Layout Engine..."));
    layoutEngine.loadConfig(defaultLayoutJson);

    Serial.println(F("[Init] NAND Storage..."));
    if (!nand.init(spiMutex)) {
        uint8_t errData[4] = {0};
        nand.readRaw(0, errData, 4);
        char errMsg[64];
        sprintf(errMsg, "NAND Err!\n%02X %02X %02X %02X", errData[0], errData[1], errData[2], errData[3]);
        display.showMessage(errMsg);
        while (1) { delay(100); }
    }

    Serial.println(F("[Init] Media Player..."));
    player.init(&nand, &display);

    Serial.println(F("[Init] UI Controller..."));
    ui.init(PIN_TOUCH, &display);

    Serial.println(F("[Init] Power Manager..."));
    powerManager.init((gpio_num_t)PIN_TOUCH);
    // TODO: Khi gắn pin thực tế qua mạch chia điện áp ADC, hãy gọi:
    // powerManager.initBattery(PIN_BATTERY_ADC);
    lastUserActivity = millis();

    // --- Boot screen ---
    ui.showBootScreen();

    // --- OTA Server (luôn bật khi có WiFi, tương lai sẽ trigger từ web client) ---
    // Chờ WiFi kết nối (tối đa WIFI_CONNECT_TIMEOUT_MS)
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
            Serial.printf("[Init] OTA ready at http://%s.local (v%s)\n", OTA_HOSTNAME, FW_VERSION);
        } else {
            Serial.println(F("[Init] WiFi not connected, OTA server skipped."));
        }
    }

    // --- Tạo FreeRTOS Tasks ---
    xTaskCreate(
        Task_MediaPlayer,
        "MediaPlayer",
        TASK_STACK_MEDIA_PLAYER,
        nullptr,
        TASK_PRIORITY_MEDIA_PLAYER,
        nullptr
    );

    xTaskCreate(
        Task_UIController,
        "UIController",
        TASK_STACK_UI_CONTROLLER,
        nullptr,
        TASK_PRIORITY_UI_CONTROLLER,
        nullptr
    );

    Serial.println(F("[Init] All tasks created. System running.\n"));
}

// ============================================================================
// Loop — Lắng nghe Serial command phục vụ TEST chế độ ngủ
// ============================================================================
void loop() {
    if (Serial.available() > 0) {
        String input = Serial.readStringUntil('\n');
        input.trim();

        if (input.equalsIgnoreCase("sleep_light") || input.equalsIgnoreCase("sleep")) {
            Serial.println(F("[TEST] Kích hoạt Light-Sleep ngay lập tức..."));
            powerManager.enterLightSleep(SLEEP_TIMER_US, &display);
            delay(50);
            lastUserActivity = millis();
            currentAppState = AppState::STATE_STANDBY;
            forceStandbyRedraw = true;
            if (Serial) {
                Serial.println(F("[Power] Restored to STANDBY mode after wakeup."));
            }
        } else if (input.equalsIgnoreCase("sleep_deep")) {
            Serial.println(F("[TEST] Kích hoạt Deep-Sleep ngay lập tức..."));
            powerManager.enterDeepSleep(SLEEP_TIMER_US, &display);
        }
    }

    vTaskDelay(pdMS_TO_TICKS(500)); // Yield
}