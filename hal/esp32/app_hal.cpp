
#include "app_hal.h"
#include "lvgl.h"

#include <esp_sleep.h>
#include <esp_task_wdt.h>
#include <time.h>

#include "../../src/state.h"
#include "../../src/deco.h"
#include "../../src/gestures.h"

/* include only one display settings */
#if defined(DEVICE_SHAPE_ROUND)
#include "displays/LGFX_GC9B72_360.hpp"
#else
#include "displays/LGFX_CYD_2432S028.hpp"
#endif


static const uint32_t screenWidth = WIDTH;
static const uint32_t screenHeight = HEIGHT;

const unsigned int lvBufferSize = screenWidth * 30;
uint8_t lvBuffer[2][lvBufferSize];

static lv_display_t *lvDisplay;
static lv_indev_t *lvInput;

#if LV_USE_LOG != 0
static void lv_log_print_g_cb(lv_log_level_t level, const char *buf)
{
  LV_UNUSED(level);
  LV_UNUSED(buf);
}
#endif

/* Display flushing */
void my_disp_flush(lv_display_t *display, const lv_area_t *area, unsigned char *data)
{

  uint32_t w = lv_area_get_width(area);
  uint32_t h = lv_area_get_height(area);
  lv_draw_sw_rgb565_swap(data, w * h);

  if (tft.getStartCount() == 0)
  {
    tft.endWrite();
  }
  tft.pushImageDMA(area->x1, area->y1, area->x2 - area->x1 + 1, area->y2 - area->y1 + 1, (uint16_t *)data);
  lv_display_flush_ready(display); /* tell lvgl that flushing is done */
}

#if !defined(DEVICE_SHAPE_ROUND)
/*Read the touchpad*/
void my_touchpad_read(lv_indev_t *indev_driver, lv_indev_data_t *data)
{
  uint16_t touchX, touchY;
  bool touched = tft.getTouch(&touchX, &touchY);
  if (!touched)
  {
    data->state = LV_INDEV_STATE_REL;
  }
  else
  {
    data->state = LV_INDEV_STATE_PR;
    /*Set the coordinates*/
    data->point.x = touchX;
    data->point.y = touchY;
  }
}
#else
// No touch IC on the round GC9B72 module's panel itself — Menu/Action are
// standalone capacitive touch pins instead, for now (see
// displays/LGFX_GC9B72_360.hpp's MENU_TOUCH_GPIO/ACTION_TOUCH_GPIO).
// Polled from hal_loop() below rather than through an lv_indev, since
// these are discrete on/off inputs, not a pointer device.
//
// touchRead() on classic ESP32 (this board, not S2/S3) returns LOWER raw
// counts when touched -- opposite of a mechanical active-low button read,
// but the same "< threshold means pressed" shape. THRESHOLD is a rough
// starting point for bare-wire/pad testing; tune per actual pad size and
// wiring once real touch pads are in place.
constexpr touch_value_t TOUCH_THRESHOLD = 40;

static bool menuBtnWasDown = false;
static bool actionBtnWasDown = false;

static void pollButtons(void)
{
  bool menuDown = touchRead(MENU_TOUCH_GPIO) < TOUCH_THRESHOLD;
  bool actionDown = touchRead(ACTION_TOUCH_GPIO) < TOUCH_THRESHOLD;

  if (menuDown && !menuBtnWasDown) dc::onMenuDown();
  else if (!menuDown && menuBtnWasDown) dc::onMenuUp();
  menuBtnWasDown = menuDown;

  if (actionDown && !actionBtnWasDown) dc::onActionDown();
  else if (!actionDown && actionBtnWasDown) dc::onActionUp();
  actionBtnWasDown = actionDown;
}
#endif

/* Tick source, tell LVGL how much time (milliseconds) has passed */
static uint32_t my_tick(void)
{
  return millis();
}

// Task Watchdog Timer: resets the chip if the loop task stalls (a hung
// touch/display transaction, an infinite loop in an LVGL callback, etc.)
// for longer than this — see hal_loop()'s reset call.
constexpr uint32_t WATCHDOG_TIMEOUT_SEC = 5;

static void watchdogInit(void)
{
  esp_task_wdt_init(WATCHDOG_TIMEOUT_SEC, true /* panic (reset) on timeout */);
  esp_task_wdt_add(NULL); // subscribe the Arduino loop task (the caller here)
}

void hal_setup(void)
{
  watchdogInit();

  /* Initialize the display drivers */
  tft.init();
  tft.initDMA();
  // tft.init() leaves the backlight at LovyanGFX's default duty (127/255,
  // ~50%) via its own internal setBrightness() call. Halving that again
  // here for a power-draw experiment -- bump back toward 127 (or add a
  // brightness setting) if the screen ends up too dim to read.
  tft.setBrightness(64);
  tft.startWrite();
  tft.fillScreen(TFT_BLACK);

#if !defined(DEVICE_SHAPE_ROUND)
  /* Set display rotation to landscape */
  tft.setRotation(1);
#endif
  // Round panel: square and circularly symmetric, so no rotation needed —
  // orientation is whatever "up" the module is physically mounted as.

  /* Set the tick callback */
  lv_tick_set_cb(my_tick);

  /* Create LVGL display and set the flush function */
  lvDisplay = lv_display_create(screenWidth, screenHeight);
  lv_display_set_color_format(lvDisplay, LV_COLOR_FORMAT_RGB565);
  lv_display_set_flush_cb(lvDisplay, my_disp_flush);
  lv_display_set_buffers(lvDisplay, lvBuffer[0], lvBuffer[1], lvBufferSize, LV_DISPLAY_RENDER_MODE_PARTIAL);

#if !defined(DEVICE_SHAPE_ROUND)
  /* Set the touch input function */
  lvInput = lv_indev_create();
  lv_indev_set_type(lvInput, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(lvInput, my_touchpad_read);
#endif
  // No pinMode() needed for MENU_TOUCH_GPIO/ACTION_TOUCH_GPIO in the round
  // build -- touchRead() configures the touch peripheral on those pins
  // itself, on first call.
}

void hal_loop(void)
{
  /* NO while loop in this function! (handled by framework) */
  // Proves this iteration started; if lv_timer_handler() below hangs, no
  // further resets happen and the watchdog fires.
  esp_task_wdt_reset();
#if defined(DEVICE_SHAPE_ROUND)
  pollButtons();
#endif
  lv_timer_handler(); // Update the UI-
  delay(5);
}

// Survives deep sleep (unlike dc::state, which lives in normal RAM and is
// wiped when the chip powers down between EXT0 wakes).
RTC_DATA_ATTR static float rtcTissuesN2[dc::NUM_COMPARTMENTS];
RTC_DATA_ATTR static float rtcTissuesHe[dc::NUM_COMPARTMENTS];
RTC_DATA_ATTR static float rtcSimMinutes = 0.0f;
RTC_DATA_ATTR static float rtcDiveTime = 0.0f;
RTC_DATA_ATTR static float rtcSurfaceTime = 0.0f;
RTC_DATA_ATTR static float rtcMaxDepth = 0.0f;
RTC_DATA_ATTR static time_t rtcSleepStartSec = 0; // 0 = no hal_enter_sleep() pending restore

#if !defined(DEVICE_SHAPE_ROUND)
// XPT2046 touch IRQ (see displays/LGFX_CYD_2432S028.hpp's cfg.pin_int) —
// pulled low on contact, and a valid RTC GPIO, so it doubles as our deep
// sleep wake source. Any touch wakes the chip; there's no way to resolve
// touch position (or which zone was touched) until it's already booting
// back up, so a "wake touch" is just a reboot, not routed through
// gestures.cpp's Action-button handling.
constexpr gpio_num_t TOUCH_IRQ_GPIO = GPIO_NUM_36;
constexpr gpio_num_t WAKE_GPIO = TOUCH_IRQ_GPIO;
#endif
// Round build's wake source is ACTION_TOUCH_GPIO itself, via
// touchSleepWakeUpEnable() in hal_enter_sleep() below -- a level-based
// esp_sleep_enable_ext0_wakeup() doesn't apply here since an untouched
// capacitive pin doesn't hold a digital low the way a mechanical button
// did, matching its role as the wake button once the pollButtons()
// short-press path is live again post-wake.

void hal_enter_sleep(void)
{
  for (int i = 0; i < dc::NUM_COMPARTMENTS; i++) {
    rtcTissuesN2[i] = dc::state.tissuesN2[i];
    rtcTissuesHe[i] = dc::state.tissuesHe[i];
  }
  rtcSimMinutes = dc::state.simMinutes;
  rtcDiveTime = dc::state.diveTime;
  rtcSurfaceTime = dc::state.surfaceTime;
  rtcMaxDepth = dc::state.maxDepth;
  time(&rtcSleepStartSec);

  tft.sleep();
  tft.setBrightness(0);

#if defined(DEVICE_SHAPE_ROUND)
  // Same pin/threshold pollButtons() reads while awake (see its
  // TOUCH_THRESHOLD comment above) -- this call sets the wake threshold
  // and enables touchpad wakeup in one go.
  touchSleepWakeUpEnable(ACTION_TOUCH_GPIO, TOUCH_THRESHOLD);
#else
  esp_sleep_enable_ext0_wakeup(WAKE_GPIO, 0 /* wake on low */);
#endif
  esp_deep_sleep_start(); // does not return
}

void hal_restore_from_sleep(void)
{
  if (rtcSleepStartSec == 0) return; // fresh power-on/reset — nothing pending
#if defined(DEVICE_SHAPE_ROUND)
  if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_TOUCHPAD) return;
#else
  if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_EXT0) return;
#endif

  time_t now;
  time(&now);
  double elapsedMin = difftime(now, rtcSleepStartSec) / 60.0;

  for (int i = 0; i < dc::NUM_COMPARTMENTS; i++) {
    dc::state.tissuesN2[i] = rtcTissuesN2[i];
    dc::state.tissuesHe[i] = rtcTissuesHe[i];
  }
  dc::state.simMinutes = rtcSimMinutes;
  dc::state.diveTime = rtcDiveTime;
  dc::state.surfaceTime = rtcSurfaceTime;
  dc::state.maxDepth = rtcMaxDepth;
  if (elapsedMin > 0.0) {
    dc::stepTissues(dc::state.tissuesN2, dc::state.tissuesHe, dc::SURFACE_N2, 0.0f, (float)elapsedMin);
    // Deep sleep is only reachable at the surface (see gestures.cpp's
    // canShutdown), so the whole sleep duration counts as surface time too.
    dc::state.surfaceTime += (float)elapsedMin;
    if (dc::state.surfaceTime >= dc::DIVE_END_SURFACE_MIN) {
      dc::state.diveTime = 0.0f;
      dc::state.maxDepth = 0.0f;
    }
  }
  dc::state.sleeping = false;

  rtcSleepStartSec = 0;
}
