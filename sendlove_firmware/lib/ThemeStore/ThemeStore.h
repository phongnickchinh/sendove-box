#ifndef THEME_STORE_H
#define THEME_STORE_H

#include <Arduino.h>

// ============================================================================
// ThemeStore — theme đang dùng, chiếu từ thẻ SD sang phân vùng flash `theme`
// ============================================================================
// User chốt "cách B" (2026-09-24, MEMORY.md §28): bus SPI dùng chung với ST7789 không
// có chân CS nên KHÔNG đọc nền/phông từ thẻ lúc vẽ được. Gói theme tải về thẻ
// (/theme/t_<id>_r<rev>/) rồi chép sang phân vùng flash 256KB, đọc qua
// esp_partition_mmap y như mảng PROGMEM StandbyBackground[] trước đây: 0 byte RAM,
// pushImage() từ con trỏ flash như cũ.
//
// Bố cục phân vùng: sector 0 = header (magic, rev, id, bảng asset, crc payload, crc
// header), payload từ 4096. Header ghi CUỐI CÙNG: mất điện giữa lúc chép thì header
// sai crc -> begin() coi như trống -> chép lại từ thẻ (case bắt buộc #1).
//
// Thẻ hỏng thì theme trong flash VẪN hiện (user chốt). Màn đen chữ trắng chỉ khi
// phân vùng trống/hỏng (vd. vừa nạp cáp bảng phân vùng mới).
//
// Asset: "layout" (JSON theme), "bg" (240x240 RGB565 LE, 115.200 B), "f_time", "f_date"
// (phông VLW do web cắt sẵn tập ký tự). Chỉ "layout" là bắt buộc.
// ============================================================================

namespace ThemeStore {

/// Tìm phân vùng, mmap, kiểm header + crc. Gọi trong setup(). false = chưa có theme.
bool begin();

bool valid();
/// Phân vùng `theme` có trong bảng phân vùng không (bản nạp trước 2026-09-24 thì không).
bool partitionPresent();
uint32_t rev();
const char* themeId();

/// Con trỏ tới asset trong flash (qua mmap), nullptr nếu không có. Hợp lệ tới lần cài kế.
const uint8_t* asset(const char* name, uint32_t* len);

/// Chép gói ở thư mục `dir` trên thẻ sang flash rồi mmap lại. Xoá/ghi flash làm CPU
/// đứng ~2s. CHỈ gọi từ task đang giữ quyền vẽ (Task_MediaPlayer) lúc không vẽ theme,
/// hoặc trong setup() — con trỏ asset cũ chết ngay khi bắt đầu.
bool installFromSd(const char* dir, const char* themeId, uint32_t rev);

/// WakeSync tải xong gói -> đặt yêu cầu; Task_MediaPlayer thấy thì cài (một chủ sở hữu).
void requestInstall(const char* dir, const char* themeId, uint32_t rev);
bool takeInstallRequest(char* dir, size_t dirLen, char* id, size_t idLen, uint32_t* rev);
bool installPending();

/// Boot mà phân vùng trống/hỏng: cài lại từ gói ghi trong /theme/active.json nếu thẻ có.
bool restoreFromSdIfNeeded();

}  // namespace ThemeStore

#endif  // THEME_STORE_H
