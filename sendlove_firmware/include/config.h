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

// --- Reserved pins, not wired on the board yet ---
// PIN_LED (breathing LED), PIN_BATTERY_ADC + a voltage divider for battery sensing.
// The I2S pins are wired; see the I2S Audio block at the end of this file.

// Timing & Power Constants
static constexpr uint64_t SLEEP_TIMER_US = 5ULL * 60 * 1000000;
static constexpr uint32_t INACTIVITY_SLEEP_TIMEOUT_MS = 60000; // TODO: Increase to 60000-300000 for production
static constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000;
// Short touch during a sync: the play command waits up to this many ms and plays by
// itself once sync ends (no second touch). Past the deadline it is dropped. Product
// decision: 60s.
static constexpr uint32_t PENDING_PLAY_MAX_WAIT_MS = 60000;
// After a sync the box must stay IDLE this many ms before the queued command plays, to
// let the chip rest (product decision). Counted in the loop without blocking: a new
// sync restarts the count.
static constexpr uint32_t PENDING_PLAY_SETTLE_MS = 10000;

// Display Backlight
static constexpr uint8_t BACKLIGHT_DAY_PERCENT = 100;

// User settings (web: PUT /boxes/:id/config -> config_flag). Stored in NVS, so they
// still apply when the SD card is faulty. Product decision: the default volume is 100 =
// the level the box always used, so a firmware update doesn't make it quieter. The "do
// NOT dim the backlight" decision in MEMORY.md §4 forbids the firmware dimming BY
// ITSELF; it doesn't apply to a level the user chose (§28).
static constexpr uint8_t SETTINGS_DEFAULT_BRIGHTNESS = 100;
static constexpr uint8_t SETTINGS_DEFAULT_VOLUME = 100;
// Brightness floor: at 0 the screen is black and the box looks dead.
static constexpr uint8_t SETTINGS_MIN_BRIGHTNESS = 5;
// The alarm screen is always at least this bright, even if the setting is lower.
static constexpr uint8_t ALARM_MIN_BRIGHTNESS = 60;
// The MAX98357A's SD_MODE pin (BOM.md: GPIO20, HIGH = play, LOW = shutdown). -1 = not
// wired on the current board (the breadboard ties SD_MODE high) -> the firmware touches
// no pin. Change to 20 on the PCB. At volume 0 the amp is shut down.
static constexpr int8_t PIN_AMP_SD = -1;

// Media Playback
static constexpr uint8_t TARGET_FPS = 15;
static constexpr uint32_t FRAME_DURATION_MS = 1000 / TARGET_FPS;
// static constexpr uint16_t I2S_SAMPLE_RATE       = 16000;
// static constexpr uint8_t  I2S_BITS_PER_SAMPLE   = 16;

// NAND Slot Config
static constexpr uint8_t NAND_SLOT_COUNT = 3;
static constexpr uint32_t NAND_SLOT_ADDRS[NAND_SLOT_COUNT] = {
    0x010000, 0x560000, 0xAB0000};

// Size cap for one media download. On NAND, writeChunk() already stops at
// _slotCapacity (slots are 0x560000-0x010000 = 0x550000 = 5,570,560 bytes apart);
// on SD this is the only size cap. Either way it is an early exit with a clear
// log: without it, a URL returning an endless stream (or a lying Content-Length)
// makes the box download for minutes and then fail somewhere else with no
// explanation.
static constexpr uint32_t MAX_MEDIA_BYTES = 25500000;

// Firebase configuration (kept in config_secrets.h so keys never reach Git)
#include "config_secrets.h"

static constexpr uint32_t FIREBASE_TIMEOUT_MS = 10000;

// Sync period while the box is awake. Each cycle costs 3 TLS handshakes (~40KB of
// heap each) even with no new message, so a short period drains the battery and keeps
// the heap under pressure (MEMORY.md §21). Product decision: 20s.
static constexpr uint32_t SYNC_INTERVAL_MS = 20000;
static constexpr const char *NVS_KEY_LAST_DOWNLOAD_TS = "last_dl_ts";
static constexpr uint8_t MAX_ALARMS = 10;

// A ringing alarm (product decision): short touch = snooze 5 minutes, hold = dismiss,
// untouched = stops by itself after 1 minute. One beep per second.
static constexpr uint32_t ALARM_SNOOZE_SEC = 5 * 60;
static constexpr uint32_t ALARM_RING_MAX_MS = 60000;
static constexpr uint32_t ALARM_BEEP_PERIOD_MS = 1000;

// Alarm music (MEMORY.md §28). Product decision: 10 tracks per box, 5-60s each, 16 kHz
// mono (the "stay at 8 kHz" rule is lifted for alarm music ONLY). 60s × 16000 × 2 bytes
// = 1,920,000 + a 54-byte header -> a 2MB cap; do NOT use AUDIO_MAX_PCM_BYTES (the
// 600KB limit for messages).
static constexpr size_t   ALARM_MUSIC_MAX_TRACKS = 10;
static constexpr uint32_t ALARM_MUSIC_MAX_BYTES  = 2000000;
// Max tracks downloaded per sync cycle: too long a cycle would let a queued play
// command (§27) hit its 60s deadline.
static constexpr uint8_t  ALARM_MUSIC_PER_SYNC   = 2;
// Ramp: start at 30% of the chosen level (the volume scale is already in dB), full level after 20s.
static constexpr uint32_t ALARM_RAMP_MS          = 20000;
static constexpr uint8_t  ALARM_RAMP_START_PCT   = 30;

// Verify the TLS certificate on every Firebase connection (instead of setInsecure()).
// The root CAs are in include/firebase_root_ca.h.
// 0 = back to setInsecure() — an EMERGENCY FALLBACK, only once the fault is known to
// be in the TLS layer (read the "[NET] tls: ..." log lines). In mode 0 the box is
// open to MITM again: anyone on the same network can read/alter messages and obtain
// FIREBASE_AUTH_SECRET.
#define FIREBASE_TLS_VERIFY 1

// Authenticate to Firebase with the box's own idToken (Firebase Auth) instead of the
// admin-level Database Secret.
//   1 = signInWithPassword -> idToken (1h lifetime), refresh token stored in NVS.
//       Only then do the `auth.uid === $box_id` rules take effect.
//   0 = back to `?auth=<FIREBASE_AUTH_SECRET>` (FALLBACK).
// Set 1 ONLY when all three hold, or the box loses its connection entirely:
//   (a) Firebase Console > Authentication > Email/Password enabled
//   (b) `node scripts/provision_box_auth.js <BOX_ID>` was run and
//       BOX_AUTH_EMAIL/BOX_AUTH_PASSWORD are filled in config_secrets.h
//   (c) database.rules.json is deployed to the `iot-app-839a2` instance
#define FIREBASE_USE_IDTOKEN 1

// A Firebase idToken JWT is ~900-1100 bytes. 1400 leaves margin.
static constexpr size_t FIREBASE_ID_TOKEN_MAX_LEN = 1400;
static constexpr size_t FIREBASE_REFRESH_TOKEN_MAX_LEN = 400;

// The longest URL is the messages one: a ~160-byte base (host +
// /messages/<BOX_ID>.json + orderBy + startAt) plus "&auth=" (6) plus the idToken.
//   Measured with a 945-byte token -> a 1105-byte URL.
//   Worst case at FIREBASE_ID_TOKEN_MAX_LEN = 1400 -> 1566 bytes. Margin: 226 bytes.
// RTDB accepts the token ONLY in the query string (see MEMORY.md §17), so this is
// unavoidable. The buffer is a MEMBER of NetworkManager, not a local: on the stack
// each function would cost ~1.8KB more while TASK_STACK_NETWORK is only 6144.
static constexpr size_t FIREBASE_URL_MAX_LEN = 1792;

// OTA Configuration
static constexpr const char *OTA_HOSTNAME = "sendlovebox";
static constexpr const char *FW_VERSION = "0.1.1";
// Touch sequence to enter/leave OTA mode (product decision): hold 3s → release → hold
// 3s → release (a "hold 6s more" prompt appears) → hold TOUCH_OTA_HOLD_MS. Not one long
// hold, because the TTP223 RECALIBRATES after 7-8s of continuous touch and then reports
// a release while the finger is still down (measured on the box: release at
// 7700-7800ms; widely confirmed for this chip). 6s leaves ~1.7s of margin. Do NOT
// raise it toward 7s.
static constexpr uint32_t TOUCH_OTA_HOLD_MS = 6000;
// The second 3s hold must arrive within this many ms of the first.
static constexpr uint32_t OTA_SEQ_STEP_WINDOW_MS = 6000;
// Once the prompt shows, the 6s hold must START within this many ms (a hold in progress isn't cancelled).
static constexpr uint32_t OTA_SEQ_FINAL_WINDOW_MS = 12000;
// While flashing, this many ms without a new chunk -> Update.abort(). Without it, a
// TCP connection dropped midway leaves _isUpdating stuck true forever: the box never
// sleeps and never rings an alarm again until it is unplugged.
static constexpr uint32_t OTA_STALL_TIMEOUT_MS = 30000;
// A new image must survive this many ms after boot before it is marked valid. A reset
// before that (crash, WDT, brownout) makes the bootloader roll back (main.cpp).
static constexpr uint32_t OTA_VERIFY_DELAY_MS = 60000;

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

// FreeRTOS task priorities & stack sizes (PowerManager has no task of its own)
static constexpr UBaseType_t TASK_PRIORITY_MEDIA_PLAYER = 3;
static constexpr UBaseType_t TASK_PRIORITY_NETWORK = 2;
static constexpr UBaseType_t TASK_PRIORITY_UI_CONTROLLER = 5;

// 8192: theme VLW fonts (LovyanGFX VLWfont::drawChar) allocate each glyph bitmap with
// alloca(w*h) ON THIS TASK'S STACK — a 48-56px digit is 2-3KB per glyph. MEMORY.md §24
// says not to raise stacks without measuring; this is a new need with concrete
// numbers, and LayoutEngine prints "[LAY] stack con N B" after the first VLW draw so
// it can be measured on the device.
static constexpr uint32_t TASK_STACK_MEDIA_PLAYER = 8192;
static constexpr uint32_t TASK_STACK_NETWORK = 6144;
static constexpr uint32_t TASK_STACK_UI_CONTROLLER = 4096;

// NVS Namespace
static constexpr const char *NVS_NAMESPACE = "sendlove";

// Storage Provider Configuration
#define STORAGE_TYPE_NAND 0
#define STORAGE_TYPE_SD 1
#define ACTIVE_STORAGE_TYPE STORAGE_TYPE_SD

// Enable temporarily to wipe the NOR/W25Q128 on the next boot.
// After flashing and confirming the wipe, set it back to 0 and rebuild.
#define ERASE_NOR_ON_BOOT 0

// ============================================================
// I2S Audio (MAX98357A)
// ============================================================
static constexpr uint8_t PIN_I2S_BCLK = 0;
static constexpr uint8_t PIN_I2S_LRC  = 1;
static constexpr uint8_t PIN_I2S_DOUT = 2;
static constexpr uint32_t AUDIO_SAMPLE_RATE    = 8000;  // Hz — default when the AUDC header is bad
// Safety cap for the pcmSize read from the AUDC header: 16 kHz mono 16-bit × 18s.
// The web already trims media; this is only the last line of defense.
static constexpr uint32_t AUDIO_MAX_PCM_BYTES  = 600000;
// 256 bytes mono = 128 samples = 16ms @ 8000Hz.
// Oversampled x4 to stereo = 512 samples = 2048 bytes = EXACTLY 1 DMA BUFFER.
// Saves 12KB of static RAM in AudioPlayer and fills the DMA buffer with no leftover.
static constexpr uint32_t AUDIO_PCM_CHUNK_SIZE = 256;
// The READ size, separate from the oversampling chunk above. 256 was tuned for raw
// NAND reads that take microseconds; on an SD card each read is a spiMutex take + the
// NOP hack + an fread through VFS/FATFS. A 16 kHz file at 15fps needs ~2134 B of audio
// per frame => 9 such reads per frame. Batching to 1024 B/read leaves 3 and costs only
// 768 B more (_stereo stays 2048 B; raising AUDIO_PCM_CHUNK_SIZE itself would cost ~7 KB).
static constexpr uint32_t AUDIO_READ_CHUNK_SIZE = 1024;
static constexpr uint8_t  AUDIO_DMA_BUF_COUNT  = 12;   // 12 × 512 samples = 192ms @ 32kHz (24KB DMA RAM)
static constexpr uint16_t AUDIO_DMA_BUF_LEN    = 512;   // samples per DMA buffer
// BCLK at 8 kHz mono (~256 kHz) is too low for the MAX98357A -> constant crackle.
// Bench test: the same tone is clean at 44.1 kHz and crackles at 8 kHz -> not an I2S
// pin mix-up. Fix: open I2S at AUDIO_OVERSAMPLE x the file rate and repeat each file
// sample AUDIO_OVERSAMPLE times when writing to DMA (AudioPlayer::fillChunk) to keep
// the pitch. The stored PCM file stays 8 kHz.
static constexpr uint8_t  AUDIO_OVERSAMPLE     = 4;

// ============================================================
// SD card storage (only used when ACTIVE_STORAGE_TYPE == STORAGE_TYPE_SD)
// ============================================================
// The SD module REPLACES the W25Q128 on the same pins (SCK 4 / MOSI 6 / MISO 5) and
// reuses CS = GPIO 8. The two chips are never on the board together.
static constexpr uint8_t PIN_SD_CS = PIN_NAND_CS;

// 20 message slots on the card (NAND has only NAND_SLOT_COUNT = 3 because of its
// 16MB). The SD build stores one unread byte per slot in the manifest, NOT a uint8_t
// bitmask like NandStorageProvider, so it isn't capped at 8 slots.
static constexpr uint8_t SD_SLOT_COUNT = 20;

// SD.begin() defaults to 4MHz — too slow for 15fps video (reading one ~15KB JPEG
// frame eats the whole 66ms budget). The library drops to 400kHz during init and
// then uses this value. 10MHz for the breadboard: "Read short" / "Bad jpegSize"
// are the symptoms of a bus running too fast.
static constexpr uint32_t SD_SPI_FREQ_HZ = 10000000;

static constexpr const char *SD_MEDIA_DIR = "/media";
static constexpr const char *SD_MANIFEST_PATH = "/media/index.bin";

// Caption cap stored in the sidecar file — equal to the NAND build's
// SLOT_TEXT_MAX_LEN so both builds display identically.
static constexpr uint16_t SD_TEXT_MAX_LEN = 256;

// ONE SPI MODE FOR THE WHOLE BUS (LovyanGFX + storage) — MODE3 IS MANDATORY.
// The ST7789 has no CS pin: tested on hardware, in MODE0 the screen stays black.
// Every other bus master must be MODE3 too, because switching CPOL between masters
// flips SCK's idle level -> one spurious rising edge -> the ST7789 loses byte
// framing (MEMORY.md §2). The stock SD library hardcodes MODE0, so it was copied to
// lib/SD and changed to MODE3 (lib/SD/src/sd_diskio.cpp, struct AcquireSPI) — this
// constant doesn't control that library; change both places together.
static constexpr uint8_t SPI_BUS_MODE = 3;

#endif // CONFIG_H
