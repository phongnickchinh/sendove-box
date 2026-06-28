#ifndef DISPLAY_DRIVER_H
#define DISPLAY_DRIVER_H

#include <Arduino.h>
#include <TFT_eSPI.h>

// ============================================================================
// DisplayDriver — Wrapper cho TFT display + SPI Mutex + Backlight PWM
// ============================================================================
// Module chia sẻ — được gọi bởi:
// - MediaPlayer (đẩy frame video qua pushFrameBuffer)
// - UIController (vẽ mặt đồng hồ, icon trạng thái)
//
// Sử dụng TFT_eSPI library (cần cấu hình User_Setup.h hoặc platformio.ini)
// SPI Mutex chia sẻ với SDCardManager.
// ============================================================================

class DisplayDriver {
public:
    /// Khởi tạo TFT display và cấu hình backlight PWM
    /// @param backlightPin Chân GPIO điều khiển backlight (BLK)
    /// @param spiMutex Mutex chia sẻ bus SPI
    /// @return true nếu khởi tạo thành công
    bool init(uint8_t backlightPin, SemaphoreHandle_t spiMutex);

    /// Đẩy một frame buffer RGB565 ra màn hình
    /// @param buffer Con trỏ tới mảng pixel RGB565
    /// @param w Chiều rộng frame
    /// @param h Chiều cao frame
    void pushFrameBuffer(const uint16_t* buffer, uint16_t w, uint16_t h);

    /// Vẽ mặt đồng hồ số
    /// @param hour Giờ (0-23)
    /// @param minute Phút (0-59)
    void drawClockFace(uint8_t hour, uint8_t minute);

    /// Vẽ thanh trạng thái (pin + Wi-Fi)
    /// @param batteryPercent Phần trăm pin (0-100)
    /// @param wifiConnected Trạng thái Wi-Fi
    void drawStatusBar(uint8_t batteryPercent, bool wifiConnected);

    /// Hiển thị thông báo lên màn hình (1 dòng text ở giữa)
    /// @param message Nội dung thông báo
    void showMessage(const char* message);

    /// Điều chỉnh độ sáng backlight
    /// @param percent Phần trăm sáng (0 = tắt, 100 = sáng tối đa)
    void setBacklight(uint8_t percent);

    /// Tắt hoàn toàn: tắt backlight + đưa TFT vào sleep mode
    void turnOff();

    /// Bật lại TFT từ sleep mode
    void turnOn();

    /// Xóa toàn bộ màn hình (fill đen)
    void clear();

    /// Lấy con trỏ TFT_eSPI để sử dụng API nâng cao nếu cần
    TFT_eSPI* getTFT();

private:
    TFT_eSPI _tft;
    uint8_t  _backlightPin = 0;
    uint8_t  _backlightChannel = 0; // LEDC channel cho PWM
    SemaphoreHandle_t _spiMutex = nullptr;

    bool acquireSPI();
    void releaseSPI();
};

#endif // DISPLAY_DRIVER_H
