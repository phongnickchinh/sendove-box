#ifndef DISPLAY_DRIVER_H
#define DISPLAY_DRIVER_H

#include "config.h"
#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <esp_sleep.h>

/// LovyanGFX configuration for ST7789 240x240 (CS-less, Mode 3 (obligatory), Shared SPI2)
/// The mode comes from SPI_BUS_MODE (config.h). Tested on hardware: MODE0 -> black screen.
class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789 _panel_instance;
  lgfx::Bus_SPI _bus_instance;

public:
  LGFX(void) {
    {
      auto cfg = _bus_instance.config();
      cfg.spi_host = SPI2_HOST;
      cfg.spi_mode = SPI_BUS_MODE;
      // 40MHz: a frame push takes ~23ms (~46ms at 20MHz, which overruns the 66ms
      // budget at 15fps). Don't go higher: the GPIO matrix limit is ~40MHz.
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
      cfg.pin_cs = PIN_TFT_CS; // -1 (no CS)
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

  /// Display a centered message on screen
  void showMessage(const char *message);

  /// Word-wrap and draw an ASCII caption inside (x,y,w,h), by pixel width; the
  /// last line ends in "..." if the text doesn't fit.
  void showWrappedText(const char *asciiText, int32_t x, int32_t y, int32_t w, int32_t h,
                        uint16_t color = 0xFFFF);

  /// Set backlight brightness percentage (0-100)
  void setBacklight(uint8_t percent);

  /// The screen is off (turnOff, waiting for light sleep). A brightness change now must NOT turn the backlight on.
  bool isSleeping() const { return _isSleeping; }

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
