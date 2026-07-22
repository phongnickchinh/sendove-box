#ifndef DISPLAY_DRIVER_H
#define DISPLAY_DRIVER_H

#include <Arduino.h>
#include <LovyanGFX.hpp>
#include "config.h"

// ============================================================================
// LGFX — Cấu hình LovyanGFX cho ESP32-C3 + ST7789 240x240 (Không CS)
// ============================================================================
// SPI Mode 3 BẮT BUỘC cho màn hình không có chân CS.
// Bus shared = true vì chia sẻ SPI2 với NAND Flash W25Q128.
// ============================================================================
class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_ST7789   _panel_instance;
    lgfx::Bus_SPI        _bus_instance;
    lgfx::Light_PWM      _light_instance;

public:
    LGFX(void) {
        // --- SPI Bus ---
        {
            auto cfg = _bus_instance.config();
            cfg.spi_host  = SPI2_HOST;
            cfg.spi_mode  = 3;          // Mode 3 bắt buộc cho màn không CS
            cfg.freq_write = 40000000;  // 40MHz SPI write
            cfg.freq_read  = 16000000;  // 16MHz SPI read
            cfg.pin_sclk  = PIN_SPI_SCK;
            cfg.pin_mosi  = PIN_SPI_MOSI;
            cfg.pin_miso  = -1;         // TFT không dùng MISO
            cfg.pin_dc    = PIN_TFT_DC;

            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }
        // --- Panel ---
        {
            auto cfg = _panel_instance.config();
            cfg.pin_cs     = PIN_TFT_CS;   // -1 (không CS)
            cfg.pin_rst    = PIN_TFT_RST;
            cfg.pin_busy   = -1;
            cfg.panel_width  = SCREEN_WIDTH;
            cfg.panel_height = SCREEN_HEIGHT;
            cfg.offset_x   = 0;
            cfg.offset_y   = 0;
            cfg.dummy_read_pixel = 8;
            cfg.dummy_read_bits  = 1;
            cfg.readable   = false;
            cfg.invert     = true;      // Đặc trưng ST7789 IPS
            cfg.rgb_order  = true;      // Sửa lỗi đảo màu đỏ/xanh
            cfg.dlen_16bit = false;
            cfg.bus_shared = true;      // Chia sẻ SPI với NAND Flash

            _panel_instance.config(cfg);
        }
        // --- Backlight PWM ---
        {
            auto cfg = _light_instance.config();
            cfg.pin_bl     = PIN_TFT_BLK;
            cfg.invert     = false;
            cfg.freq       = 44100;     // 44.1kHz PWM
            cfg.pwm_channel = 7;

            _light_instance.config(cfg);
            _panel_instance.setLight(&_light_instance);
        }

        setPanel(&_panel_instance);
    }
};

// ============================================================================
// DisplayDriver — Wrapper cho LGFX display + SPI Mutex
// ============================================================================
// Module chia sẻ — được gọi bởi:
// - MediaPlayer (đẩy JPEG frame qua pushImage)
// - UIController (vẽ mặt đồng hồ, icon trạng thái)
//
// SPI Mutex chia sẻ với NandStorage (cùng bus SPI2).
// ============================================================================

class DisplayDriver {
public:
    /// Khởi tạo TFT display
    /// @param spiMutex Mutex chia sẻ bus SPI
    /// @return true nếu khởi tạo thành công
    bool init(SemaphoreHandle_t spiMutex);

    /// Đẩy một vùng pixel lên màn hình (dùng cho JPEGDEC callback)
    /// @param x Tọa độ X
    /// @param y Tọa độ Y
    /// @param w Chiều rộng
    /// @param h Chiều cao
    /// @param pixels Con trỏ tới mảng pixel RGB565
    void pushImage(int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t* pixels);

    /// Vẽ mặt đồng hồ số
    void drawClockFace(uint8_t hour, uint8_t minute);

    /// Vẽ thanh trạng thái (pin + Wi-Fi)
    void drawStatusBar(uint8_t batteryPercent, bool wifiConnected);

    /// Hiển thị thông báo lên màn hình (1 dòng text ở giữa)
    void showMessage(const char* message);

    /// Điều chỉnh độ sáng backlight (0 = tắt, 100 = sáng tối đa)
    void setBacklight(uint8_t percent);

    /// Tắt hoàn toàn: tắt backlight + đưa TFT vào sleep mode
    void turnOff();

    /// Bật lại TFT từ sleep mode
    void turnOn();

    /// Xóa toàn bộ màn hình (fill đen)
    void clear();

    /// Lấy con trỏ LGFX để sử dụng API nâng cao nếu cần
    LGFX* getTFT();

    /// Bắt đầu sử dụng SPI bus (cho external callers cần vẽ nhiều thao tác)
    bool acquireSPI();

    /// Kết thúc sử dụng SPI bus
    void releaseSPI();

private:
    LGFX _tft;
    SemaphoreHandle_t _spiMutex = nullptr;
};

#endif // DISPLAY_DRIVER_H
