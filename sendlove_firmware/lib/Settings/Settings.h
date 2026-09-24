#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>
#include <atomic>

// ============================================================================
// Settings — cài đặt người dùng ĐANG CÓ HIỆU LỰC (độ sáng, âm lượng phát tin)
// ============================================================================
// Nguồn: NVS lúc boot (begin), cloud khi `config_flag` bật (apply, gọi từ task WakeSync).
// Người đọc: Task_MediaPlayer, Task_UIController, DisplayDriver, MediaPlayer -> atomic.
// Cài đặt nằm ở NVS chứ không ở thẻ SD: thẻ hỏng thì hộp vẫn đúng độ sáng/âm lượng.
// ============================================================================

namespace Settings {

extern std::atomic<uint8_t> brightness;   // SETTINGS_MIN_BRIGHTNESS..100
extern std::atomic<uint8_t> volume;       // 0..100, 0 = tắt tiếng
extern std::atomic<uint32_t> appliedRev;  // config_rev lần áp dụng gần nhất
/// Tăng mỗi lần độ sáng đổi -> vòng lặp màn hình biết để ghi lại PWM.
extern std::atomic<uint32_t> brightnessEpoch;

/// Đọc NVS. Gọi một lần trong setup(), SAU ConfigManager có NVS.
void begin();

/// Kẹp giá trị, lưu NVS, cập nhật RAM. Giá trị < 0 = cloud không gửi trường đó -> giữ cũ.
/// Trả true nếu ghi NVS thành công (kể cả khi không có gì đổi).
bool apply(int newBrightness, int newVolume, uint32_t rev);

/// % PWM thật cho màn thường. Gamma 2 để thanh trượt trên web tăng đều theo cảm nhận mắt.
uint8_t currentBacklight();
/// % PWM cho màn báo thức: không tối hơn ALARM_MIN_BRIGHTNESS.
uint8_t alarmBacklight();

/// Hệ số Q15 cho âm lượng 0..100 theo dB (100 = 0dB = 32768, 1 = -40dB, 0 = câm).
/// 32768 là mốc "đúng bằng đường cũ": AudioPlayer bỏ hẳn phép nhân khi gặp giá trị này.
int32_t volumeGainQ15(uint8_t vol);

static constexpr int32_t GAIN_UNITY = 32768;

}  // namespace Settings

#endif  // SETTINGS_H
