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
static constexpr uint32_t INACTIVITY_SLEEP_TIMEOUT_MS = 60000; // TODO: Increase to 60000-300000 for production
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
static constexpr uint8_t NAND_SLOT_COUNT = 3;
static constexpr uint32_t NAND_SLOT_ADDRS[NAND_SLOT_COUNT] = {
    0x010000, 0x560000, 0xAB0000};

// Tran kich thuoc mot lan tai media. Khoang cach 2 slot dau la
// 0x560000-0x010000 = 0x550000 = 5.570.560 byte, nen 5.500.000 nam vua duoi
// mot slot.
// Day KHONG phai lop chan tran bo nho — writeChunk() da chan o _slotCapacity
// tu truoc. Day la thoat som + log ro ly do: truoc do mot URL tra ve stream
// vo tan (hoac Content-Length noi doi) khien box tai vai phut roi moi chet o
// cho khac, khong ai biet vi sao.
static constexpr uint32_t MAX_MEDIA_BYTES = 5500000;

// Firebase Configuration (Lưu trong config_secrets.h để chống lộ API trên Git)
#include "config_secrets.h"

static constexpr uint32_t FIREBASE_TIMEOUT_MS = 10000;
static constexpr const char *NVS_KEY_LAST_DOWNLOAD_TS = "last_dl_ts";
static constexpr uint8_t MAX_ALARMS = 10;

// Bat xac thuc chung chi TLS cho moi ket noi Firebase (thay cho setInsecure()).
// Root CA nam o include/firebase_root_ca.h.
// Dat ve 0 = quay lai setInsecure() — DUONG LUI KHAN CAP, chi dung khi da xac
// dinh loi la o tang TLS (doc dong log "[NET] tls: ..."). Chay o che do 0 nghia
// la box lai ho MITM: bat ky ai trong cung mang doc/sua duoc tin nhan va lay
// duoc FIREBASE_AUTH_SECRET.
#define FIREBASE_TLS_VERIFY 1

// Xac thuc voi Firebase bang idToken rieng cua box (Firebase Auth) thay vi
// Database Secret quyen admin.
//   1 = signInWithPassword -> idToken (han 1h) -> header Authorization: Bearer,
//       refresh token luu NVS. Rules `auth.uid === $box_id` moi co hieu luc.
//   0 = quay lai `?auth=<FIREBASE_AUTH_SECRET>` (DUONG LUI).
// Dat 1 CHI KHI da du 3 dieu kien, neu khong box se mat ket noi hoan toan:
//   (a) Firebase Console > Authentication > bat Email/Password
//   (b) da chay `node scripts/provision_box_auth.js <BOX_ID>` va dien
//       BOX_AUTH_EMAIL/BOX_AUTH_PASSWORD vao config_secrets.h
//   (c) da deploy database.rules.json len dung instance `iot-app-839a2`
#define FIREBASE_USE_IDTOKEN 1

// Trong idToken JWT cua Firebase (~900-1100 byte). De du bien 1400.
static constexpr size_t FIREBASE_ID_TOKEN_MAX_LEN = 1400;
static constexpr size_t FIREBASE_REFRESH_TOKEN_MAX_LEN = 400;

// URL dai nhat: base ~160 byte (host + /messages/<BOX_ID>.json + orderBy +
// startAt) cong "&auth=" cong ca idToken. Do RTDB CHI nhan token qua query
// (do that 2026-09-05, xem MEMORY.md muc 17), khong cach nao tranh duoc.
// Buffer nay la THANH VIEN cua NetworkManager, khong phai bien cuc bo: dat
// tren stack thi moi ham ton them ~1.8KB trong khi TASK_STACK_NETWORK chi 6144.
static constexpr size_t FIREBASE_URL_MAX_LEN = 1792;

// OTA Configuration
static constexpr const char *OTA_HOSTNAME = "sendlovebox";
static constexpr const char *FW_VERSION = "2.1.0";

// Wi-Fi & NTP Configuration (Fallback credentials if NVS is empty)
static constexpr size_t WIFI_SSID_MAX_LEN = 33;  // 32 chars + null terminator
static constexpr size_t WIFI_PASS_MAX_LEN = 65;  // 64 chars + null terminator
static constexpr const char *DEFAULT_WIFI_SSID = "@Ruijie-s4617";
static constexpr const char *DEFAULT_WIFI_PASSWORD = "56Daiyen";
// static constexpr const char *DEFAULT_WIFI_SSID = "@Ruijie-s4617";
// static constexpr const char *DEFAULT_WIFI_PASSWORD = "56Daiyen";

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
static constexpr uint32_t TASK_STACK_MEDIA_PLAYER = 6144;
static constexpr uint32_t TASK_STACK_NETWORK = 6144;
static constexpr uint32_t TASK_STACK_UI_CONTROLLER = 4096;

// NVS Namespace
static constexpr const char *NVS_NAMESPACE = "sendlove";

// Storage Provider Configuration
#define STORAGE_TYPE_NAND 0
#define STORAGE_TYPE_SD 1
#define ACTIVE_STORAGE_TYPE STORAGE_TYPE_NAND

// Bật tạm thời để xóa sạch dữ liệu trên NOR/W25Q128 lúc boot kế tiếp.
// Sau khi nạp xong và xác nhận dữ liệu đã được xóa, đổi về 0 rồi build lại.
#define ERASE_NOR_ON_BOOT 0
#ifdef WOKWI_SIMULATION
static constexpr uint8_t PIN_BUZZER = 0;
static constexpr uint32_t BUZZER_PLAY_DURATION_MS = 2000;
#endif

// ============================================================
// I2S Audio (MAX98357A)
// ============================================================
static constexpr uint8_t PIN_I2S_BCLK = 0;
static constexpr uint8_t PIN_I2S_LRC  = 1;
static constexpr uint8_t PIN_I2S_DOUT = 2;
static constexpr uint32_t AUDIO_SAMPLE_RATE    = 8000;  // Hz — mặc định khi header AUDC hỏng
// Trần an toàn cho pcmSize đọc từ header AUDC: 16kHz mono 16-bit × 18s.
// Video bị web cắt ở 15s, audio cũng phải bị cắt theo — đây chỉ là chốt chặn cuối.
static constexpr uint32_t AUDIO_MAX_PCM_BYTES  = 600000;
// 256 bytes mono = 128 samples = 16ms @ 8000Hz.
// Oversample x4 Stereo = 512 samples = 2048 bytes = ĐÚNG 1 DMA BUFFER.
// Tiết kiệm 12KB static RAM trong AudioPlayer, giúp DMA fit chuẩn 100% không dư rác.
static constexpr uint32_t AUDIO_PCM_CHUNK_SIZE = 256;
static constexpr uint8_t  AUDIO_DMA_BUF_COUNT  = 12;   // 12 × 512 samples = 192ms @ 32kHz (24KB DMA RAM)
static constexpr uint16_t AUDIO_DMA_BUF_LEN    = 512;   // samples per DMA buffer
// BCLK ở 8kHz mono (~256kHz) quá thấp cho MAX98357A -> rè liên tục. Test tay
// (2026-09-01): cùng tone phát sạch ở 44.1kHz, rè ở 8kHz -> không phải do
// đảo chân I2S. Fix: I2S vẫn mở ở AUDIO_OVERSAMPLE x tốc độ file, mỗi mẫu file
// lặp lại AUDIO_OVERSAMPLE lần khi ghi ra DMA (AudioPlayer::fillChunk) để giữ
// đúng cao độ. File PCM trên NAND vẫn 8kHz, không đổi.
static constexpr uint8_t  AUDIO_OVERSAMPLE     = 4;

#endif // CONFIG_H
