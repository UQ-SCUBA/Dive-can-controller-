#pragma once

#include <LovyanGFX.hpp>

/**
 * 2.1" round TFT LCD, GC9B72 driver IC, 360x360, 4-wire SPI, no touch
 * controller on the module.
 *
 * LovyanGFX has no dedicated Panel_GC9B72 class, so the init command list
 * below is ported from MaliosDark/Arduino_GC9B72 (an Arduino_GFX driver),
 * which in turn ports its sequence from the xboot project's fb-gc9b72.c —
 * a known-good reference driver for this exact chip, not a same-family
 * guess. This replaces an earlier version of this file that reused
 * GC9A01's init sequence verbatim as a placeholder; that guess is gone now
 * that a real GC9B72 sequence is available. Still UNVERIFIED against our
 * own physical hardware (bring-up pending). If colors are wrong/inverted or
 * the image is mirrored/offset on first bring-up, the usual suspects are
 * cfg.invert, cfg.rgb_order, getMadCtl()'s rotation table, or
 * offset_x/offset_y — not necessarily the init command list itself.
 *
 * SPI/backlight pins below match the real wiring on the ESPduino-32D bring-
 * up board: sclk=18, mosi=23, cs=16, dc=14, rst=13, bl=17 (miso left
 * unconnected — write-only panel). GPIO16/17 are free GPIOs on a plain
 * WROOM module (no embedded PSRAM); if this board turns out to be a WROVER
 * variant, those two would need to move since PSRAM claims them. Menu/
 * Action are two physical GPIO buttons (this module has no touch IC), read
 * in hal/esp32/app_hal.cpp; their pins (32/33 below) are still placeholders,
 * not yet matched to real wiring.
 *
 * Recommended board settings for platformio.ini:
 *
 * board = esp32dev
 * framework = arduino
 */

#define WIDTH 360
#define HEIGHT 360

// Menu/Action input — capacitive touch pins (ESP32 touch channels T2/T0),
// for now, ahead of real mechanical buttons being wired in. Chosen over
// T9/T8 (GPIO32/33) for easier physical access on this board. Note GPIO2
// is a boot-mode strapping pin (must read low/floating at reset for a
// normal flash boot) -- shouldn't matter in practice since nothing else
// drives it, but worth knowing if boot ever gets flaky. See app_hal.cpp's
// DEVICE_SHAPE_ROUND input path.
#define MENU_TOUCH_GPIO 2
#define ACTION_TOUCH_GPIO 4

namespace lgfx {
inline namespace v1 {

// Real GC9B72 init sequence — ported from MaliosDark/Arduino_GC9B72 (itself
// ported from xboot's fb-gc9b72.c), ORed with CMD_INIT_DELAY where the
// source has a post-command delay. See the file header comment above for
// provenance and the unverified-on-real-hardware caveat.
struct Panel_GC9B72 : public lgfx::Panel_GC9xxx {
  Panel_GC9B72(void) {
    _cfg.panel_width  = _cfg.memory_width  = 360;
    _cfg.panel_height = _cfg.memory_height = 360;

    // Same GC9xxx family as GC9A01, which malfunctions on a closing NOP;
    // disabling it here too out of caution (unconfirmed for GC9B72
    // specifically, but the panel is write-only/non-readable regardless).
    _nop_closing = false;
  }

protected:
  const uint8_t* getInitCommands(uint8_t listno) const override {
    static constexpr uint8_t list0[] = {
        0xFE, 0,
        0xEF, 0,
        0x80, 1, 0x19,
        0x82, 1, 0x09,
        0x83, 1, 0x03,
        0x88, 1, 0x00,
        0x89, 1, 0x38,
        0x8A, 1, 0x40,
        0x8B, 1, 0x0A,
        0x8C, 1, 0x00,
        0x81, 1, 0xFF,
        0x84, 1, 0xFF,
        0x85, 1, 0xFF,
        0x86, 1, 0xFF,
        0x87, 1, 0xFF,
        0x8E, 1, 0xFF,
        0x8F, 1, 0xFF,
        0x98, 1, 0x3E,
        0x99, 1, 0x3E,
        0x7D, 1, 0x72,

        0x70, 10, 0x02, 0x03, 0x03, 0x06, 0x03, 0x03, 0x09, 0x07, 0x09, 0x03,
        0x90, 4, 0x06, 0x06, 0x01, 0x01,
        0x93, 3, 0x02, 0xFF, 0x00,
        0xCB, 1, 0x02,
        0xFB, 2, 0x00, 0x00,
        0xF6, 1, 0xC0,
        0x6C, 7, 0x00, 0x00, 0x22, 0x00, 0xCC, 0x04, 0x58,
        0xAA, 2, 0x0B, 0x00,
        0xEC, 1, 0x07,
        0xF9, 1, 0x40,
        0xEB, 2, 0x01, 0x67,
        0x74, 6, 0x01, 0x60, 0x00, 0x00, 0x00, 0x00,
        0xB5, 3, 0x14, 0x14, 0x14,

        0x6E, 32,
          0x0B, 0x0B, 0x09, 0x09, 0x13, 0x13, 0x11, 0x11,
          0x16, 0x15, 0x01, 0x04, 0x00, 0x0D, 0x1D, 0x00,
          0x00, 0x1D, 0x0D, 0x00, 0x04, 0x08, 0x15, 0x16,
          0x12, 0x12, 0x14, 0x14, 0x0A, 0x0A, 0x0C, 0x0C,

        0x60, 4, 0x38, 0x1C, 0x13, 0x56,
        0x61, 4, 0xF8, 0x0A, 0x13, 0x56,
        0x62, 4, 0xF8, 0x0B, 0x13, 0x56,
        0x63, 4, 0x38, 0x1C, 0x13, 0x56,
        0x64, 6, 0x38, 0x20, 0x72, 0xF8, 0x13, 0x56,
        0x65, 6, 0x78, 0x1A, 0x70, 0x0B, 0x56, 0x13,
        0x66, 6, 0x38, 0x24, 0x72, 0xFC, 0x13, 0x56,
        0x68, 7, 0xB3, 0x08, 0x0E, 0x08, 0x0E, 0x0A, 0x0A,
        0x69, 7, 0xB3, 0x08, 0x0E, 0x08, 0x0E, 0x0A, 0x0A,
        0x6A, 2, 0x00, 0x00,

        0x3A, 1, 0x05, // COLMOD: 16bpp RGB565
        0x36, 1, 0x00, // MADCTL (overridden per-rotation by getMadCtl() below)

        0x7C, 2, 0xB6, 0x29,
        0xAC, 1, 0x40,
        0xC3, 1, 0x1A,
        0xC4, 1, 0x24,
        0xC9, 1, 0x2F,

        0xF0, 6, 0x11, 0x17, 0x08, 0x06, 0x05, 0x38,
        0xF1, 6, 0x4D, 0x72, 0x72, 0x2D, 0x34, 0x8F,
        0xF2, 6, 0x11, 0x17, 0x08, 0x06, 0x05, 0x38,
        0xF3, 6, 0x4D, 0x72, 0x72, 0x2D, 0x34, 0x8F,

        0xB4, 1, 0x0A,
        0x35, 1, 0x00, // Tearing effect line ON

        0xFE, 0,
        0xEE, 0,

        0x11, 0+CMD_INIT_DELAY, 120, // sleep out
        0x29, 0+CMD_INIT_DELAY, 20,  // display on
        0xFF, 0xFF, // end
    };
    switch (listno) {
    case 0: return list0;
    default: return nullptr;
    }
  }
};

} // namespace v1
} // namespace lgfx

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_GC9B72 _panel_instance;
  lgfx::Bus_SPI _bus_instance;
  lgfx::Light_PWM _light_instance;

public:
  LGFX(void) {
    { // SPI bus
      auto cfg = _bus_instance.config();
      cfg.spi_host = VSPI_HOST;
      cfg.spi_mode = 0;
      // Arduino_GC9B72's README says it's only tested up to ~20MHz on this
      // chip (lower still if using long jumper wires) — if bring-up shows
      // display artifacts/corruption, try dropping this before anything
      // else.
      cfg.freq_write = 40000000;
      cfg.freq_read = 16000000;
      cfg.spi_3wire = false;
      cfg.use_lock = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk = 18;
      cfg.pin_mosi = 23;
      cfg.pin_miso = -1; // module's SDO left unconnected — write-only panel
      cfg.pin_dc = 14;
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }

    { // Panel
      auto cfg = _panel_instance.config();
      cfg.pin_cs = 16;
      cfg.pin_rst = 13;
      cfg.pin_busy = -1;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.offset_rotation = 0;
      cfg.readable = false;
      cfg.invert = false;
      // Bring-up on real hardware showed colGreen() (0x35e07a, G-dominant)
      // rendering correctly but colRed() (0xff3b30, R-dominant) rendering as
      // blue -- a textbook RGB/BGR panel-order mismatch (only R/B swap,
      // which is why G-dominant colors looked fine). Flipped from false.
      cfg.rgb_order = true;
      cfg.dlen_16bit = false;
      cfg.bus_shared = false;
      _panel_instance.config(cfg);
    }

    { // Backlight
      auto cfg = _light_instance.config();
      cfg.pin_bl = 17;
      cfg.invert = false;
      cfg.freq = 44100;
      cfg.pwm_channel = 7;
      _light_instance.config(cfg);
      _panel_instance.setLight(&_light_instance);
    }

    setPanel(&_panel_instance);
  }
};

LGFX tft;
