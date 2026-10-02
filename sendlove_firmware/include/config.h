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

// Not wired yet: PIN_LED (breathing LED), PIN_BATTERY_ADC + divider.

// Timing & Power Constants
static constexpr uint64_t SLEEP_TIMER_US = 5ULL * 60 * 1000000;
static constexpr uint32_t INACTIVITY_SLEEP_TIMEOUT_MS = 60000; // TODO: Increase to 60000-300000 for production
static constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000;
// A touch during a sync queues the play command for at most this long (product decision).
static constexpr uint32_t PENDING_PLAY_MAX_WAIT_MS = 60000;
// Idle time required after a sync before the queued command plays (product decision).
static constexpr uint32_t PENDING_PLAY_SETTLE_MS = 10000;

// Display Backlight
static constexpr uint8_t BACKLIGHT_DAY_PERCENT = 100;

// User settings from the web, stored in NVS (they survive a faulty SD card).
// Default volume 100 = the level the box always used (product decision). The "do NOT
// dim the backlight" rule (MEMORY.md §4) doesn't apply to a user-chosen level (§28).
static constexpr uint8_t SETTINGS_DEFAULT_BRIGHTNESS = 100;
static constexpr uint8_t SETTINGS_DEFAULT_VOLUME = 100;
// Brightness floor: at 0 the screen is black and the box looks dead.
static constexpr uint8_t SETTINGS_MIN_BRIGHTNESS = 5;
// Minimum brightness of the alarm screen.
static constexpr uint8_t ALARM_MIN_BRIGHTNESS = 60;
// MAX98357A SD_MODE pin (HIGH = play, LOW = shutdown; used at volume 0).
// -1 = not wired (breadboard). Set to 20 on the PCB (BOM.md).
static constexpr int8_t PIN_AMP_SD = -1;

// Media Playback
static constexpr uint8_t TARGET_FPS = 15;
static constexpr uint32_t FRAME_DURATION_MS = 1000 / TARGET_FPS;

// NAND Slot Config
static constexpr uint8_t NAND_SLOT_COUNT = 3;
static constexpr uint32_t NAND_SLOT_ADDRS[NAND_SLOT_COUNT] = {
    0x010000, 0x560000, 0xAB0000};

// Size cap for one media download: an early exit with a clear log for an endless
// stream or a lying Content-Length. On SD it is the only cap; on NAND writeChunk()
// also stops at the slot capacity (5,570,560 bytes).
static constexpr uint32_t MAX_MEDIA_BYTES = 25500000;

// Firebase configuration (kept in config_secrets.h so keys never reach Git)
#include "config_secrets.h"

static constexpr uint32_t FIREBASE_TIMEOUT_MS = 10000;

// Sync period while awake. Each cycle costs 3 TLS handshakes (~40KB of heap each),
// see MEMORY.md §21. Product decision: 20s.
static constexpr uint32_t SYNC_INTERVAL_MS = 20000;
static constexpr const char *NVS_KEY_LAST_DOWNLOAD_TS = "last_dl_ts";
static constexpr uint8_t MAX_ALARMS = 10;

// Ringing alarm (product decision): touch = snooze 5 min, hold = dismiss, stops after 1 min.
static constexpr uint32_t ALARM_SNOOZE_SEC = 5 * 60;
static constexpr uint32_t ALARM_RING_MAX_MS = 60000;
static constexpr uint32_t ALARM_BEEP_PERIOD_MS = 1000;

// Alarm music (MEMORY.md §28, product decision): 10 tracks, 5-60s, 16 kHz mono (the
// 8 kHz rule is lifted for alarm music ONLY). 60s × 16000 × 2 B fits the 2MB cap;
// do NOT use AUDIO_MAX_PCM_BYTES (the message limit) here.
static constexpr size_t   ALARM_MUSIC_MAX_TRACKS = 10;
static constexpr uint32_t ALARM_MUSIC_MAX_BYTES  = 2000000;
// Per sync cycle; a longer cycle would outlast a queued play command (§27).
static constexpr uint8_t  ALARM_MUSIC_PER_SYNC   = 2;
// Ramp: from 30% of the chosen level to full over 20s.
static constexpr uint32_t ALARM_RAMP_MS          = 20000;
static constexpr uint8_t  ALARM_RAMP_START_PCT   = 30;

// 1 = verify the TLS certificate on every Firebase connection (root CAs in
// firebase_root_ca.h). 0 = setInsecure(): EMERGENCY FALLBACK only, open to MITM.
#define FIREBASE_TLS_VERIFY 1

// 1 = authenticate with the box's own idToken (the `auth.uid === $box_id` rules apply);
// 0 = fall back to the admin Database Secret.
// Set 1 ONLY when all three hold, or the box loses its connection entirely:
//   (a) Email/Password sign-in is enabled in the Firebase Console
//   (b) scripts/provision_box_auth.js was run and BOX_AUTH_* are in config_secrets.h
//   (c) database.rules.json is deployed
#define FIREBASE_USE_IDTOKEN 1

// A Firebase idToken JWT is ~900-1100 bytes. 1400 leaves margin.
static constexpr size_t FIREBASE_ID_TOKEN_MAX_LEN = 1400;
static constexpr size_t FIREBASE_REFRESH_TOKEN_MAX_LEN = 400;

// Longest URL (messages): ~160-byte base + "&auth=" + idToken, 1566 bytes worst
// case. The token must go in the query (MEMORY.md §17). The buffer is a NetworkManager
// member, not a local: TASK_STACK_NETWORK is only 6144.
static constexpr size_t FIREBASE_URL_MAX_LEN = 1792;

// OTA Configuration
static constexpr const char *OTA_HOSTNAME = "sendlovebox";
static constexpr const char *FW_VERSION = "0.1.1";
// OTA touch sequence (product decision): hold 3s, hold 3s, then hold
// TOUCH_OTA_HOLD_MS. The TTP223 recalibrates after ~7.7s of continuous touch and
// reports a release: do NOT raise the 6s toward 7s.
static constexpr uint32_t TOUCH_OTA_HOLD_MS = 6000;
// The second 3s hold must arrive within this many ms of the first.
static constexpr uint32_t OTA_SEQ_STEP_WINDOW_MS = 6000;
// The 6s hold must START within this many ms of the prompt.
static constexpr uint32_t OTA_SEQ_FINAL_WINDOW_MS = 12000;
// No new chunk for this long while flashing -> Update.abort(); otherwise a dropped
// connection leaves the box stuck in "updating" (no sleep, no alarms).
static constexpr uint32_t OTA_STALL_TIMEOUT_MS = 30000;
// A new image is marked valid after surviving this long; a reset before = rollback.
static constexpr uint32_t OTA_VERIFY_DELAY_MS = 60000;

// Wi-Fi & NTP Configuration (Fallback credentials if NVS is empty)
static constexpr size_t WIFI_SSID_MAX_LEN = 33;  // 32 chars + null terminator
static constexpr size_t WIFI_PASS_MAX_LEN = 65;  // 64 chars + null terminator
static constexpr const char *DEFAULT_WIFI_SSID = "@Ruijie-s4617";
static constexpr const char *DEFAULT_WIFI_PASSWORD = "56Daiyen";

static constexpr const char *NTP_SERVER_1 = "time.google.com";
static constexpr const char *NTP_SERVER_2 = "asia.pool.ntp.org";
static constexpr const char *NTP_SERVER_3 = "pool.ntp.org";
static constexpr const char *TIMEZONE_ENV = "ICT-7";

// FreeRTOS task priorities & stack sizes (PowerManager has no task of its own)
static constexpr UBaseType_t TASK_PRIORITY_MEDIA_PLAYER = 3;
static constexpr UBaseType_t TASK_PRIORITY_NETWORK = 2;
static constexpr UBaseType_t TASK_PRIORITY_UI_CONTROLLER = 5;

// 8192: VLW fonts alloca() each glyph bitmap on this task's stack (2-3KB for a
// 48-56px digit). LayoutEngine logs "[LAY] stack con N B" to measure it (MEMORY.md §24).
static constexpr uint32_t TASK_STACK_MEDIA_PLAYER = 8192;
static constexpr uint32_t TASK_STACK_NETWORK = 6144;
static constexpr uint32_t TASK_STACK_UI_CONTROLLER = 4096;

// NVS Namespace
static constexpr const char *NVS_NAMESPACE = "sendlove";

// Storage Provider Configuration
#define STORAGE_TYPE_NAND 0
#define STORAGE_TYPE_SD 1
#define ACTIVE_STORAGE_TYPE STORAGE_TYPE_SD

// 1 = wipe the W25Q128 on the next boot; set back to 0 afterwards.
#define ERASE_NOR_ON_BOOT 0

// I2S Audio (MAX98357A)
static constexpr uint8_t PIN_I2S_BCLK = 0;
static constexpr uint8_t PIN_I2S_LRC  = 1;
static constexpr uint8_t PIN_I2S_DOUT = 2;
static constexpr uint32_t AUDIO_SAMPLE_RATE    = 8000;  // Hz — default when the AUDC header is bad
// Safety cap for the AUDC header's pcmSize: 16 kHz mono 16-bit × 18s.
static constexpr uint32_t AUDIO_MAX_PCM_BYTES  = 600000;
// 256 bytes mono = 128 samples; oversampled x4 to stereo = EXACTLY 1 DMA buffer.
static constexpr uint32_t AUDIO_PCM_CHUNK_SIZE = 256;
// READ size, separate from the chunk above: SD reads are expensive (mutex + VFS), so
// batching to 1024 B cuts 9 reads per video frame to 3 for 768 B more RAM.
static constexpr uint32_t AUDIO_READ_CHUNK_SIZE = 1024;
static constexpr uint8_t  AUDIO_DMA_BUF_COUNT  = 12;   // 12 × 512 samples = 192ms @ 32kHz (24KB DMA RAM)
static constexpr uint16_t AUDIO_DMA_BUF_LEN    = 512;   // samples per DMA buffer
// BCLK at 8 kHz is too low for the MAX98357A (constant crackle), so I2S runs at
// AUDIO_OVERSAMPLE x the file rate, interpolating between samples (AudioPlayer::fillChunk).
static constexpr uint8_t  AUDIO_OVERSAMPLE     = 4;

// SD card storage (ACTIVE_STORAGE_TYPE == STORAGE_TYPE_SD). The SD module REPLACES
// the W25Q128 on the same pins and CS (GPIO 8); the two are never fitted together.
static constexpr uint8_t PIN_SD_CS = PIN_NAND_CS;

// The manifest keeps one unread byte per slot (no 8-slot bitmask limit as on NAND).
static constexpr uint8_t SD_SLOT_COUNT = 20;

// The 4MHz default is too slow for 15fps video. 10MHz for the breadboard: "Read
// short" / "Bad jpegSize" are the symptoms of a bus running too fast.
static constexpr uint32_t SD_SPI_FREQ_HZ = 10000000;

static constexpr const char *SD_MEDIA_DIR = "/media";
static constexpr const char *SD_MANIFEST_PATH = "/media/index.bin";

// Caption cap; equals the NAND build's SLOT_TEXT_MAX_LEN.
static constexpr uint16_t SD_TEXT_MAX_LEN = 256;

// ONE SPI mode for the whole bus — MODE3 IS MANDATORY: the CS-less ST7789 stays
// black in MODE0 and loses byte framing if masters switch CPOL (MEMORY.md §2).
// lib/SD is a copy patched to MODE3 (sd_diskio.cpp); change both places together.
static constexpr uint8_t SPI_BUS_MODE = 3;

#endif // CONFIG_H
