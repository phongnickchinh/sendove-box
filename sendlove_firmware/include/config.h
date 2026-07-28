#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Hardware SPI2 Bus (Shared between TFT and NAND Flash)
static constexpr uint8_t PIN_SPI_MOSI = 6;
static constexpr uint8_t PIN_SPI_MISO = 5;
static constexpr uint8_t PIN_SPI_SCK = 4;

// TFT Display (ST7789 240x240 IPS, CS-less)
static constexpr int8_t PIN_TFT_CS = -1;
static constexpr uint8_t PIN_TFT_DC = 7;
static constexpr uint8_t PIN_TFT_RST = 9;
static constexpr uint8_t PIN_TFT_BLK = 3;

static constexpr uint16_t SCREEN_WIDTH = 240;
static constexpr uint16_t SCREEN_HEIGHT = 240;

// NAND Flash W25Q128 (Shared Hardware SPI2)
static constexpr uint8_t PIN_NAND_CS = 8;

// --- Touch Sensor (TTP223) --------------------------------------------------
static constexpr uint8_t PIN_TOUCH = 10; // Active HIGH (INPUT_PULLDOWN)

// ============================================================================
// Phase 2 — Chưa triển khai (chân dự trữ)
// ============================================================================
// --- I2S Audio (MAX98357A) --- Cần GPIO 0, 1, 2 rảnh
// static constexpr uint8_t PIN_I2S_BCLK = 2;   // Bit Clock
// static constexpr uint8_t PIN_I2S_LRC  = 1;   // Left/Right Clock (Word
// Select) static constexpr uint8_t PIN_I2S_DOUT = 0;   // Data Out

// --- LED Indicator ---
// static constexpr uint8_t PIN_LED      = 20;  // Breathing LED (PWM)

// --- Battery ADC ---
// static constexpr uint8_t PIN_BATTERY_ADC = 2; // ADC1_CH2
// static constexpr float BATTERY_VOLTAGE_DIVIDER_RATIO = 2.0f;
// static constexpr float BATTERY_FULL_VOLTAGE  = 4.2f;
// static constexpr float BATTERY_EMPTY_VOLTAGE = 3.0f;
// static constexpr uint8_t BATTERY_LOW_THRESHOLD = 10;
// static constexpr uint8_t PIN_SD_CS    = ???;

// Timing & Power Constants
static constexpr uint64_t SLEEP_TIMER_US = 5ULL * 60 * 1000000;
static constexpr uint32_t INACTIVITY_SLEEP_TIMEOUT_MS =
    30000; // TODO: Increase to 60000-300000 for production
static constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000;
static constexpr uint8_t WIFI_RETRY_MAX = 3;
static constexpr uint32_t TOUCH_DEBOUNCE_MS = 50;
static constexpr uint32_t CLOCK_DISPLAY_DURATION_MS = 5000;

// Display Backlight
static constexpr uint8_t BACKLIGHT_DAY_PERCENT = 100;
static constexpr uint8_t BACKLIGHT_NIGHT_PERCENT = 20;
static constexpr uint8_t BACKLIGHT_OFF = 0;

// Media Playback
static constexpr uint8_t TARGET_FPS = 15;
static constexpr uint32_t FRAME_DURATION_MS = 1000 / TARGET_FPS;
// static constexpr uint16_t I2S_SAMPLE_RATE       = 16000;
// static constexpr uint8_t  I2S_BITS_PER_SAMPLE   = 16;

// NAND Slot Config
static constexpr uint8_t NAND_SLOT_COUNT = 5;
static constexpr uint32_t NAND_SLOT_ADDRS[NAND_SLOT_COUNT] = {
    0x010000, 0x340000, 0x670000, 0x9A0000, 0xCD0000};

// Firebase Configuration (Lưu trong config_secrets.h để chống lộ API trên Git)
#include "config_secrets.h"

static constexpr uint32_t FIREBASE_TIMEOUT_MS = 5000;
static constexpr const char *NVS_KEY_LAST_DOWNLOAD_TS = "last_dl_ts";
static constexpr uint8_t MAX_ALARMS = 10;


// OTA Configuration
static constexpr const char *OTA_HOSTNAME = "sendlovebox";
static constexpr const char *FW_VERSION = "2.1.0";

// Wi-Fi & NTP Configuration (Fallback credentials if NVS is empty)
static constexpr const char *DEFAULT_WIFI_SSID = "@Ruijie-s4617";
static constexpr const char *DEFAULT_WIFI_PASSWORD = "56Daiyen";

static constexpr const char *NTP_SERVER_1 = "time.google.com";
static constexpr const char *NTP_SERVER_2 = "asia.pool.ntp.org";
static constexpr const char *NTP_SERVER_3 = "pool.ntp.org";
static constexpr const char *TIMEZONE_ENV = "ICT-7";

// FreeRTOS Task Priorities & Stack Sizes
static constexpr UBaseType_t TASK_PRIORITY_POWER_MANAGER = 4;
static constexpr UBaseType_t TASK_PRIORITY_MEDIA_PLAYER = 3;
static constexpr UBaseType_t TASK_PRIORITY_NETWORK = 2;
static constexpr UBaseType_t TASK_PRIORITY_UI_CONTROLLER = 5;

static constexpr uint32_t TASK_STACK_POWER_MANAGER = 4096;
static constexpr uint32_t TASK_STACK_MEDIA_PLAYER = 8192;
static constexpr uint32_t TASK_STACK_NETWORK = 8192;
static constexpr uint32_t TASK_STACK_UI_CONTROLLER = 4096;

// NVS Namespace
static constexpr const char *NVS_NAMESPACE = "sendlove";

// Storage Provider Configuration
#define STORAGE_TYPE_NAND 0
#define STORAGE_TYPE_SD   1
#define ACTIVE_STORAGE_TYPE STORAGE_TYPE_NAND

#ifdef WOKWI_SIMULATION
static constexpr uint8_t PIN_BUZZER = 0;
static constexpr uint32_t BUZZER_PLAY_DURATION_MS = 2000;
#endif

#endif // CONFIG_H

