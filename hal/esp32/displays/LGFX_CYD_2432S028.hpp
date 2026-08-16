#pragma once

#include <LovyanGFX.hpp>

/**
 * ESP32-2432S028R "Cheap Yellow Display" — ILI9341 320x240 (physically a
 * portrait 240x320 panel, rotated to landscape below) + XPT2046 resistive
 * touch, sharing the same SPI bus as the panel with a separate CS line.
 *
 * Pin mapping is the widely-published community-standard one for this board
 * (matches the pinout used by most public CYD/Arduino examples). It has NOT
 * been verified against real hardware yet — no board on hand. Re-check
 * against the actual silkscreen/schematic once it arrives; the usual
 * surprises on this board are pin_rst actually being wired (some batches use
 * GPIO 4 instead of unconnected) and occasional rgb_order/invert flips
 * between production runs.
 *
 * Recommended board settings for platformio.ini:
 *
 * board = esp32dev
 * framework = arduino
 */

#define WIDTH 320
#define HEIGHT 240

class LGFX : public lgfx::LGFX_Device
{
  lgfx::Panel_ILI9341 _panel_instance;
  lgfx::Bus_SPI _bus_instance;
  lgfx::Light_PWM _light_instance;
  lgfx::Touch_XPT2046 _touch_instance;

public:
  LGFX(void)
  {
    { // SPI bus shared by panel and touch
      auto cfg = _bus_instance.config();
      cfg.spi_host = VSPI_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 40000000;
      cfg.freq_read = 16000000;
      cfg.spi_3wire = false;
      cfg.use_lock = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk = 14;
      cfg.pin_mosi = 13;
      cfg.pin_miso = 12;
      cfg.pin_dc = 2;
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }

    { // Panel — native physical orientation is portrait 240x320;
      // setRotation(1) in app_hal.cpp flips to the 320x240 landscape
      // logical resolution declared in WIDTH/HEIGHT above.
      auto cfg = _panel_instance.config();
      cfg.pin_cs = 15;
      cfg.pin_rst = -1;
      cfg.pin_busy = -1;
      cfg.panel_width = 240;
      cfg.panel_height = 320;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.offset_rotation = 0;
      cfg.readable = false;
      cfg.invert = false;
      cfg.rgb_order = false;
      cfg.dlen_16bit = false;
      cfg.bus_shared = true;
      _panel_instance.config(cfg);
    }

    { // Backlight
      auto cfg = _light_instance.config();
      cfg.pin_bl = 21;
      cfg.invert = false;
      cfg.freq = 44100;
      cfg.pwm_channel = 7;
      _light_instance.config(cfg);
      _panel_instance.setLight(&_light_instance);
    }

    { // XPT2046 touch — same SPI bus as the panel, separate CS/IRQ
      auto cfg = _touch_instance.config();
      cfg.x_min = 0;
      cfg.x_max = 239;
      cfg.y_min = 0;
      cfg.y_max = 319;
      cfg.pin_int = 36;
      cfg.bus_shared = true;
      cfg.offset_rotation = 0;
      cfg.spi_host = VSPI_HOST;
      cfg.freq = 2500000;
      cfg.pin_sclk = 14;
      cfg.pin_mosi = 13;
      cfg.pin_miso = 12;
      cfg.pin_cs = 33;
      _touch_instance.config(cfg);
      _panel_instance.setTouch(&_touch_instance);
    }

    setPanel(&_panel_instance);
  }
};

LGFX tft;
