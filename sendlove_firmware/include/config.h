#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ============================================================================
// SENDLOVE BOX — Hardware Configuration
// ============================================================================
// File này chỉ chứa hằng số phần cứng (Pinout) và cấu hình compile-time.
// Wi-Fi credentials được quản lý bởi ConfigManager (NVS), KHÔNG hardcode ở đây.
//
// ĐẶT TRONG include/ ĐỂ CẢ src/ VÀ lib/ ĐỀU TRUY CẬP ĐƯỢC.
// ============================================================================

// --- SPI Bus (Chia sẻ giữa TFT và SD Card) ---------------------------------
// TODO: Cập nhật pinout theo thiết kế phần cứng thực tế
static constexpr uint8_t PIN_SPI_MOSI = 6;
static constexpr uint8_t PIN_SPI_MISO = 5;
static constexpr uint8_t PIN_SPI_SCK  = 4;

// --- TFT Display (ST7735 1.77 inch) -----------------------------------------
static constexpr uint8_t PIN_TFT_CS   = 7;
static constexpr uint8_t PIN_TFT_DC   = 8;   // Data/Command
static constexpr uint8_t PIN_TFT_RST  = 10;
static constexpr uint8_t PIN_TFT_BLK  = 3;   // Backlight (PWM)

// Kích thước màn hình (đặt tên SCREEN_ để tránh xung đột với TFT_eSPI macros)
static constexpr uint16_t SCREEN_WIDTH  = 128;
static constexpr uint16_t SCREEN_HEIGHT = 160;

// --- MicroSD Card Module ----------------------------------------------------
static constexpr uint8_t PIN_SD_CS    = 9;

// --- I2S Audio (MAX98357A) --------------------------------------------------
static constexpr uint8_t PIN_I2S_BCLK = 2;   // Bit Clock
static constexpr uint8_t PIN_I2S_LRC  = 1;   // Left/Right Clock (Word Select)
static constexpr uint8_t PIN_I2S_DOUT = 0;   // Data Out

// --- Touch Sensor (TTP223) --------------------------------------------------
static constexpr uint8_t PIN_TOUCH    = 20;   // GPIO Interrupt

// --- LED Indicator ----------------------------------------------------------
static constexpr uint8_t PIN_LED      = 21;   // Breathing LED (PWM)

// --- Battery ADC ------------------------------------------------------------
static constexpr uint8_t PIN_BATTERY_ADC = 4; // ADC1 channel
// Voltage divider: R1 = 100kΩ, R2 = 100kΩ → ratio = 2.0
static constexpr float BATTERY_VOLTAGE_DIVIDER_RATIO = 2.0f;
static constexpr float BATTERY_FULL_VOLTAGE  = 4.2f;
static constexpr float BATTERY_EMPTY_VOLTAGE = 3.0f;
static constexpr uint8_t BATTERY_LOW_THRESHOLD = 10; // percent

// ============================================================================
// Timing & Power Constants
// ============================================================================
static constexpr uint64_t SLEEP_TIMER_US       = 5ULL * 60 * 1000000; // 5 phút
static constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000;            // 15 giây
static constexpr uint8_t  WIFI_RETRY_MAX       = 3; // Thất bại N lần → bật SoftAP
static constexpr uint32_t TOUCH_DEBOUNCE_MS    = 50;
static constexpr uint32_t CLOCK_DISPLAY_DURATION_MS = 5000; // 5 giây hiển thị đồng hồ

// ============================================================================
// Display Backlight
// ============================================================================
static constexpr uint8_t BACKLIGHT_DAY_PERCENT   = 100;
static constexpr uint8_t BACKLIGHT_NIGHT_PERCENT = 20;
static constexpr uint8_t BACKLIGHT_OFF           = 0;

// ============================================================================
// Media Playback
// ============================================================================
static constexpr uint8_t  TARGET_FPS            = 15;
static constexpr uint32_t FRAME_DURATION_MS     = 1000 / TARGET_FPS; // ~66ms
static constexpr uint16_t I2S_SAMPLE_RATE       = 16000; // 16kHz WAV
static constexpr uint8_t  I2S_BITS_PER_SAMPLE   = 16;

// File paths on SD card
static constexpr const char* SD_VIDEO_PATH = "/media/video.bin";
static constexpr const char* SD_AUDIO_PATH = "/media/voice.wav";

// ============================================================================
// Firebase Configuration
// ============================================================================
static constexpr const char* FIREBASE_HOST    = "your-project.firebaseio.com";
static constexpr const char* FIREBASE_API_KEY = "YOUR_API_KEY";
static constexpr const char* BOX_ID           = "box_id_001";

// ============================================================================
// Wi-Fi Provisioning (SoftAP)
// ============================================================================
static constexpr const char* AP_SSID     = "SendloveBox-Setup";
static constexpr const char* AP_PASSWORD = ""; // Open network for easy setup

// ============================================================================
// FreeRTOS Task Priorities & Stack Sizes
// ============================================================================
static constexpr UBaseType_t TASK_PRIORITY_POWER_MANAGER  = 4; // Cao nhất
static constexpr UBaseType_t TASK_PRIORITY_MEDIA_PLAYER   = 3;
static constexpr UBaseType_t TASK_PRIORITY_NETWORK        = 2;
static constexpr UBaseType_t TASK_PRIORITY_UI_CONTROLLER  = 1; // Thấp nhất

static constexpr uint32_t TASK_STACK_POWER_MANAGER = 4096;
static constexpr uint32_t TASK_STACK_MEDIA_PLAYER  = 8192;  // Cần nhiều cho DMA buffer
static constexpr uint32_t TASK_STACK_NETWORK       = 8192;  // Cần nhiều cho HTTP/TLS
static constexpr uint32_t TASK_STACK_UI_CONTROLLER = 4096;

// ============================================================================
// NVS Namespace
// ============================================================================
static constexpr const char* NVS_NAMESPACE = "sendlove";

// ============================================================================
// Wokwi Simulation — Thay thế phần cứng không hỗ trợ trong giả lập
// ============================================================================
#ifdef WOKWI_SIMULATION
// Active buzzer thay thế loa I2S (MAX98357A)
// Dùng chân DOUT cũ (GPIO 0) để đơn giản hóa wiring
static constexpr uint8_t PIN_BUZZER = 0;

// Thời gian buzzer kêu khi mô phỏng phát tin nhắn (ms)
static constexpr uint32_t BUZZER_PLAY_DURATION_MS = 2000;
#endif // WOKWI_SIMULATION

#endif // CONFIG_H
