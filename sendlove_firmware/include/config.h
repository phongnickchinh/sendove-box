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

// --- Chân dự trữ, chưa nối trên bo ---
// PIN_LED (LED nhịp thở), PIN_BATTERY_ADC + bộ chia áp cho đo pin.
// Chân I2S đã nối thật, xem khối I2S Audio ở cuối file.

// Timing & Power Constants
static constexpr uint64_t SLEEP_TIMER_US = 5ULL * 60 * 1000000;
static constexpr uint32_t INACTIVITY_SLEEP_TIMEOUT_MS = 60000; // TODO: Increase to 60000-300000 for production
static constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000;

// Display Backlight
static constexpr uint8_t BACKLIGHT_DAY_PERCENT = 100;

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

// Nhịp sync khi hộp đang thức. Mỗi chu kỳ tốn 3 lần bắt tay TLS (~40KB heap mỗi
// lần) kể cả khi không có tin mới, nên 10s cũ vừa tốn pin vừa ép heap liên tục
// (MEMORY.md §21). User chốt 20s 2026-09-18: tin tới chậm hơn tối đa 10 giây.
static constexpr uint32_t SYNC_INTERVAL_MS = 20000;
static constexpr const char *NVS_KEY_LAST_DOWNLOAD_TS = "last_dl_ts";
static constexpr uint8_t MAX_ALARMS = 10;

// Báo thức đang kêu (user chốt 2026-09-18): chạm ngắn = báo lại sau 5 phút,
// chạm giữ = tắt, không ai chạm thì tự tắt sau 1 phút. Bíp mỗi giây một hồi.
static constexpr uint32_t ALARM_SNOOZE_SEC = 5 * 60;
static constexpr uint32_t ALARM_RING_MAX_MS = 60000;
static constexpr uint32_t ALARM_BEEP_PERIOD_MS = 1000;

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

// URL dai nhat la cua messages: base ~160 byte (host + /messages/<BOX_ID>.json
// + orderBy + startAt) cong "&auth=" (6) cong idToken.
//   Do that 2026-09-05 voi token 945 byte -> URL 1105 byte.
//   Xau nhat theo FIREBASE_ID_TOKEN_MAX_LEN = 1400 -> 1566 byte. Bien 226 byte.
// Do RTDB CHI nhan token qua query (xem MEMORY.md muc 17), khong cach nao tranh.
// Buffer nay la THANH VIEN cua NetworkManager, khong phai bien cuc bo: dat
// tren stack thi moi ham ton them ~1.8KB trong khi TASK_STACK_NETWORK chi 6144.
static constexpr size_t FIREBASE_URL_MAX_LEN = 1792;

// OTA Configuration
static constexpr const char *OTA_HOSTNAME = "sendlovebox";
// Cửa sổ mở web server OTA sau khi cờ emergency_ota/normal_ota trên cloud bật.
// Hộp thức suốt cửa sổ này (không ngủ) nên đừng kéo dài quá mức cần thiết.
static constexpr uint32_t OTA_WINDOW_MS = 10 * 60 * 1000;
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

// FreeRTOS Task Priorities & Stack Sizes (PowerManager không có task riêng)
static constexpr UBaseType_t TASK_PRIORITY_MEDIA_PLAYER = 3;
static constexpr UBaseType_t TASK_PRIORITY_NETWORK = 2;
static constexpr UBaseType_t TASK_PRIORITY_UI_CONTROLLER = 5;

static constexpr uint32_t TASK_STACK_MEDIA_PLAYER = 6144;
static constexpr uint32_t TASK_STACK_NETWORK = 6144;
static constexpr uint32_t TASK_STACK_UI_CONTROLLER = 4096;

// NVS Namespace
static constexpr const char *NVS_NAMESPACE = "sendlove";

// Storage Provider Configuration
#define STORAGE_TYPE_NAND 0
#define STORAGE_TYPE_SD 1
#define ACTIVE_STORAGE_TYPE STORAGE_TYPE_SD

// Bật tạm thời để xóa sạch dữ liệu trên NOR/W25Q128 lúc boot kế tiếp.
// Sau khi nạp xong và xác nhận dữ liệu đã được xóa, đổi về 0 rồi build lại.
#define ERASE_NOR_ON_BOOT 0

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

// ============================================================
// SD Card Storage (chỉ có tác dụng khi ACTIVE_STORAGE_TYPE == STORAGE_TYPE_SD)
// ============================================================
// Module SD THAY THẾ chip W25Q128 trên đúng bộ chân cũ (SCK 4 / MOSI 6 / MISO 5),
// dùng lại luôn CS = GPIO 8. Hai chip không bao giờ cùng nằm trên bo.
static constexpr uint8_t PIN_SD_CS = PIN_NAND_CS;

// 20 slot tin nhắn trên thẻ (NAND chỉ có NAND_SLOT_COUNT = 3 vì bị giới hạn 16MB).
// Bản SD dùng 1 byte cờ unread cho mỗi slot trong manifest, KHÔNG dùng bitmask
// uint8_t như NandStorageProvider, nên không bị trần 8 slot.
static constexpr uint8_t SD_SLOT_COUNT = 20;

// SD.begin() mặc định 4MHz — quá chậm cho video 15fps (đọc 1 frame JPEG ~15KB
// đã ăn hết ngân sách 66ms). Thư viện tự hạ về 400kHz trong lúc init rồi mới
// dùng con số này. Hạ xuống 10MHz nếu breadboard sinh "Read short"/"Bad jpegSize".
static constexpr uint32_t SD_SPI_FREQ_HZ = 10000000;

static constexpr const char *SD_MEDIA_DIR = "/media";
static constexpr const char *SD_MANIFEST_PATH = "/media/index.bin";

// Trần caption lưu trong file sidecar — bằng SLOT_TEXT_MAX_LEN của bản NAND
// để hai bản hiển thị giống hệt nhau.
static constexpr uint16_t SD_TEXT_MAX_LEN = 256;

// MODE SPI DÙNG CHUNG CHO CẢ BUS (LovyanGFX + storage) — BẮT BUỘC MODE3.
// ST7789 không có chân CS: đã thử thực tế 2026-09-17, chạy MODE0 thì màn hình
// đen hoàn toàn. Mọi chủ bus khác cũng phải MODE3, vì đổi CPOL giữa hai chủ bus
// làm SCK nhảy mức lúc idle -> 1 sườn lên giả -> ST7789 lệch khung byte
// (MEMORY.md mục 2). Thư viện SD gốc hardcode MODE0 nên đã chép vào lib/SD và
// đổi sang MODE3 (lib/SD/src/sd_diskio.cpp, struct AcquireSPI) — hằng số này
// không điều khiển được thư viện đó, sửa thì sửa cả hai chỗ.
static constexpr uint8_t SPI_BUS_MODE = 3;

#endif // CONFIG_H
