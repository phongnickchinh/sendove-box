#ifndef DISPLAY_DRIVER_H
#define DISPLAY_DRIVER_H

#include "config.h"
#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <esp_sleep.h>

/// LovyanGFX configuration for ST7789 240x240 (CS-less, Mode 3 (obligatory), Shared SPI2)
/// Mode lay tu SPI_BUS_MODE (config.h). Da thu 2026-09-17: MODE0 -> man hinh den.
class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789 _panel_instance;
  lgfx::Bus_SPI _bus_instance;

public:
  LGFX(void) {
    {
      auto cfg = _bus_instance.config();
      cfg.spi_host = SPI2_HOST;
      cfg.spi_mode = SPI_BUS_MODE;
      // 40MHz thay vi 20MHz. Day mot frame 240x240 RGB565 = 921.600 bit;
      // o 20MHz rieng phan day len LCD da ton ~46ms, trong khi ngan sach
      // mot frame o 15fps chi la 66ms -> cong them giai ma JPEG (~22ms) va
      // doc flash (~4ms) thanh ~74ms, vuot ngan sach ~8ms va troi dan so voi
      // audio (I2S chay bang clock phan cung, khong bao gio cho).
      // 40MHz cat phan day con ~23ms -> ~52ms, du duoi ngan sach.
      // Khong day cao hon: chan 4/5/6 khong trung IOMUX cua FSPI tren
      // ESP32-C3 nen SPI di qua GPIO matrix, tran thuc te quanh 40MHz.
      cfg.freq_write = 40000000;
      cfg.freq_read = 16000000;
      cfg.pin_sclk = PIN_SPI_SCK;
      cfg.pin_mosi = PIN_SPI_MOSI;
      cfg.pin_miso = PIN_SPI_MISO;
      cfg.pin_dc = PIN_TFT_DC;

      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }
    {
      auto cfg = _panel_instance.config();
      cfg.pin_cs = PIN_TFT_CS; // -1 (không CS)
      cfg.pin_rst = PIN_TFT_RST;
      cfg.pin_busy = -1;
      cfg.panel_width = SCREEN_WIDTH;
      cfg.panel_height = SCREEN_HEIGHT;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits = 1;
      cfg.readable = false;
      cfg.invert = true;
      cfg.rgb_order = true;
      cfg.dlen_16bit = false;
      cfg.bus_shared = true;

      _panel_instance.config(cfg);
    }

    setPanel(&_panel_instance);
  }
};

/// Wrapper for LGFX display and SPI mutex management
class DisplayDriver {
public:
  /// Initialize display hardware and SPI mutex
  bool init(SemaphoreHandle_t spiMutex);

  /// Push raw RGB565 pixel block to display
  void pushImage(int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t *pixels);

  /// Draw digital clock face
  void drawClockFace(uint8_t hour, uint8_t minute);

  /// Draw top status bar with battery and Wi-Fi status
  void drawStatusBar(uint8_t batteryPercent, bool wifiConnected);

  /// Display a centered message on screen
  void showMessage(const char *message);

  /// Word-wrap và vẽ caption ASCII (đã bỏ dấu tiếng Việt từ trước bởi caller)
  /// trong 1 vùng chữ nhật (x,y,w,h). Tự ngắt dòng theo bề rộng pixel thực tế
  /// bằng font ChakraPetch_SemiBold_16, giới hạn số dòng vừa chiều cao vùng,
  /// dòng cuối thêm "..." nếu văn bản dài hơn chỗ hiển thị.
  void showWrappedText(const char *asciiText, int32_t x, int32_t y, int32_t w, int32_t h,
                        uint16_t color = 0xFFFF);

  /// Set backlight brightness percentage (0-100)
  void setBacklight(uint8_t percent);

  /// Turn off display and lock backlight GPIO LOW for sleep
  void turnOff();

  /// Turn on display and re-initialize LGFX pipeline after light sleep
  void turnOn();

  /// Flash backlight 3x to confirm chip is alive after wakeup (works before LGFX re-init)
  void wakeupFlash();

  /// Fill screen with black
  void clear();

  /// Get underlying LGFX instance
  LGFX *getTFT();

  /// Acquire SPI bus mutex with default 1000ms timeout
  bool acquireSPI();

  /// Acquire SPI bus mutex with custom timeout (used by ScreenLogger)
  bool acquireSPI(uint32_t timeoutMs);

  /// Release SPI bus mutex
  void releaseSPI();

private:
  LGFX _tft;
  SemaphoreHandle_t _spiMutex = nullptr;
  bool _isSleeping = false;
};

#endif // DISPLAY_DRIVER_H
