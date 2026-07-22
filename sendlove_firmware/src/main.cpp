#include <Arduino.h>
#include <SPI.h>
#include "config.h"
#include "DisplayDriver.h"
#include "NandStorage.h"
#include "MediaPlayer.h"
#include "UIController.h"

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

// --- System Event Types ---
enum class SystemEvent : uint8_t {
    NONE,
    TOUCH_NEXT_SLOT,   // Chạm → chuyển slot tiếp theo
};

// --- Shared Resources ---
static SemaphoreHandle_t spiMutex = nullptr;
static QueueHandle_t     eventQueue = nullptr;

// --- Modules ---
static DisplayDriver display;
static NandStorage   nand;
static MediaPlayer   player;
static UIController  ui;

// --- State ---
static int8_t currentSlot = -1;

// ============================================================================
// Task: Media Player
// ============================================================================
// Ưu tiên cao — decode JPEG + render lên display liên tục.
// Nhận lệnh chuyển slot qua eventQueue.
// ============================================================================
void Task_MediaPlayer(void* pvParameters) {
    Serial.println(F("[Task] MediaPlayer started"));

    // Tìm và phát slot hợp lệ đầu tiên
    currentSlot = nand.findFirstValidSlot();
    if (currentSlot >= 0) {
        player.playSlot(currentSlot);
    } else {
        display.showMessage("NAND Empty");
        Serial.println(F("[Task] No valid slots found in NAND"));
    }

    for (;;) {
        // Kiểm tra event từ UI Controller
        SystemEvent event = SystemEvent::NONE;
        if (xQueueReceive(eventQueue, &event, 0) == pdTRUE) {
            if (event == SystemEvent::TOUCH_NEXT_SLOT) {
                int8_t next = nand.findNextValidSlot(currentSlot);
                if (next >= 0 && next != currentSlot) {
                    currentSlot = next;
                    player.playSlot(currentSlot);
                    Serial.printf("[Task] Switched to slot %d\n", currentSlot);
                }
            }
        }

        // Update playback (decode 1 frame hoặc idle)
        player.update();
    }
}

// ============================================================================
// Task: UI Controller
// ============================================================================
// Ưu tiên thấp — đọc touch sensor, debounce, gửi event.
// ============================================================================
void Task_UIController(void* pvParameters) {
    Serial.println(F("[Task] UIController started"));

    for (;;) {
        // Kiểm tra touch
        if (ui.isTouched()) {
            SystemEvent event = SystemEvent::TOUCH_NEXT_SLOT;
            xQueueSend(eventQueue, &event, 0);
            Serial.println(F("[UI] Touch detected → NEXT_SLOT event"));
        }

        // LED update (Phase 2)
        ui.updateLED();

        vTaskDelay(pdMS_TO_TICKS(10)); // Poll mỗi 10ms
    }
}

// ============================================================================
// Setup
// ============================================================================
void setup() {
    Serial.setRxBufferSize(4096); // Cực kỳ quan trọng để chống rớt gói tin USB CDC (như project tessst)
    Serial.begin(115200);
    delay(2000); // Chờ USB CDC enum trên PC

    Serial.println(F("\n========================================"));
    Serial.println(F("  SENDLOVE BOX — Phase 1 Firmware"));
    Serial.println(F("  ST7789 + NAND + Touch"));
    Serial.println(F("========================================\n"));

    // --- Tạo shared resources ---
    spiMutex = xSemaphoreCreateMutex();
    eventQueue = xQueueCreate(8, sizeof(SystemEvent));

    if (spiMutex == nullptr || eventQueue == nullptr) {
        Serial.println(F("[FATAL] Failed to create mutex or queue"));
        while (1) delay(1000);
    }

    // --- Khởi tạo Hardware SPI2 ---
    // LovyanGFX sẽ tự init SPI2 khi gọi display.init()
    // Nhưng NAND cũng dùng SPI2, nên cần đảm bảo SPI đã sẵn sàng
    SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI, -1);

    // --- Khởi tạo modules ---
    Serial.println(F("[Init] Display..."));
    Serial.flush();
    display.init(spiMutex);
    display.setBacklight(BACKLIGHT_DAY_PERCENT);
    display.showMessage("Booting...");

    Serial.println(F("[Init] NAND Storage..."));
    Serial.flush();
    if (!nand.init(spiMutex)) {
        display.showMessage("NAND Error!");
        Serial.println(F("[FATAL] NAND init failed"));
        Serial.flush();
        while (1) delay(1000);
    }

    Serial.println(F("[Init] Media Player..."));
    Serial.flush();
    player.init(&nand, &display);

    Serial.println(F("[Init] UI Controller..."));
    Serial.flush();
    ui.init(PIN_TOUCH, &display);

    // --- Boot screen ---
    ui.showBootScreen();

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
    Serial.flush();
}

// ============================================================================
// Loop (không dùng — FreeRTOS tasks xử lý mọi thứ)
// ============================================================================
void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000)); // Yield, loop() không làm gì
}