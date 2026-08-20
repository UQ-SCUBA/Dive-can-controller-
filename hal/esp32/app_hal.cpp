
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
#include "dive_can_hal.h"
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
// plain GPIO buttons instead (see displays/LGFX_GC9B72_360.hpp's
// MENU_BTN_GPIO/ACTION_BTN_GPIO), internal pull-up, grounded on press.
// Polled from hal_loop() below rather than through an lv_indev, since
// these are discrete on/off inputs, not a pointer device.
//
// Interrupt-driven, not polled -- an earlier version read digitalRead()
// once per hal_loop() iteration, which meant a press could only ever be
// noticed as often as the main loop got back around to checking it. A slow
// frame (the segmented bargraph's render cost, or anything else that ever
// eats into a tick) directly ate into how many chances a quick press had to
// be seen, and could stretch a short tap's *measured* duration to whatever
// the loop's gap happened to be. attachInterrupt() below fires on every
// pin transition the instant it happens, independent of whatever the main
// loop/LVGL is doing, so a press is always captured at its true time.
//
// Debounced in the ISR itself (raw digitalRead() on a hand-grounded pin
// with no debounce cap bounces for a few ms around each transition) by
// ignoring any edge that lands within DEBOUNCE_MS of the last *accepted*
// one -- collapses a bounce burst back to one clean edge per press without
// needing to wait for N ms of stable polling (there's no polling anymore).
constexpr uint32_t DEBOUNCE_MS = 25;

// Do not hand only the current level to the main loop.  A complete short tap
// can start and finish while LVGL is flushing a slow frame; in that case the
// old level-only hand-off was back at "up" before pollButtons() ran and the
// tap was lost.  This small single-producer (ISR), single-consumer (loop)
// ring retains both edges until the main loop can dispatch them.
constexpr uint8_t BUTTON_EVENT_QUEUE_SIZE = 8;

struct ButtonEvent {
  bool down;
  uint32_t atMs;
};

struct ButtonIsrState {
  volatile uint32_t lastEdgeMs = 0;  // millis() at the last accepted (non-bounce) edge
  volatile bool rawDown = false;     // latest physical level, including filtered bounce edges
  volatile uint8_t head = 0;         // next slot written by the ISR
  volatile uint8_t tail = 0;         // next slot read by hal_loop()
  ButtonEvent events[BUTTON_EVENT_QUEUE_SIZE];
};

static ButtonIsrState menuIsr;
static ButtonIsrState actionIsr;

static void IRAM_ATTR recordButtonEdge(ButtonIsrState *button, int pin) {
  uint32_t now = millis();
  // Always retain the current physical level, even for an edge rejected as
  // bounce.  In particular, a very quick release can otherwise be filtered
  // out after its press was accepted, leaving the logical button latched down.
  button->rawDown = digitalRead(pin) == LOW;
  if (now - button->lastEdgeMs < DEBOUNCE_MS) return;
  button->lastEdgeMs = now;

  const uint8_t head = button->head;
  const uint8_t next = (head + 1) % BUTTON_EVENT_QUEUE_SIZE;
  // There can only be two useful edges per physical tap, so eight entries
  // gives the UI ample time to recover from a slow redraw.  If the consumer
  // is stalled for still longer, retain the already queued event sequence
  // rather than corrupting it by overwriting the oldest event.
  if (next == button->tail) return;
  button->events[head].down = button->rawDown;
  button->events[head].atMs = now;
  button->head = next; // publish only after the event has been written
}

// IRAM_ATTR: ISRs must live in IRAM on ESP32 so they're still reachable
// while flash cache is disabled (e.g. mid flash-write) -- keep these tiny,
// no Serial/heap/anything blocking. digitalRead()/millis() are both safe to
// call from ISR context on ESP32.
static void IRAM_ATTR menuPinIsr() {
  recordButtonEdge(&menuIsr, MENU_BTN_GPIO);
}

static void IRAM_ATTR actionPinIsr() {
  recordButtonEdge(&actionIsr, ACTION_BTN_GPIO);
}

// Drains ISR-captured edges into the gesture layer.  The ISR is the only
// writer of head and the loop is the only writer of tail, so this is a simple
// lock-free single-producer/single-consumer queue.
static bool takeButtonEvent(ButtonIsrState *button, ButtonEvent *event) {
  const uint8_t tail = button->tail;
  if (tail == button->head) return false;
  *event = button->events[tail];
  button->tail = (tail + 1) % BUTTON_EVENT_QUEUE_SIZE;
  return true;
}

static bool menuBtnWasDown = false;
static bool actionBtnWasDown = false;

static void dispatchMenuButton(bool down, uint32_t atMs) {
  if (down == menuBtnWasDown) return;
  menuBtnWasDown = down;
  if (down) dc::onMenuDown(atMs);
  else dc::onMenuUp(atMs);
}

static void dispatchActionButton(bool down, uint32_t atMs) {
  if (down == actionBtnWasDown) return;
  actionBtnWasDown = down;
  if (down) dc::onActionDown(atMs);
  else dc::onActionUp(atMs);
}

static void pollButtons(void)
{
  ButtonEvent event;
  while (takeButtonEvent(&menuIsr, &event)) {
    dispatchMenuButton(event.down, event.atMs);
  }
  while (takeButtonEvent(&actionIsr, &event)) {
    dispatchActionButton(event.down, event.atMs);
  }

  // An edge inside DEBOUNCE_MS is deliberately not queued.  Reconcile the
  // latest raw level here so a filtered release never leaves a button stuck
  // down.  This does not make the loop responsible for catching taps: the
  // ISR queue above still captures every accepted press/release edge.
  const uint32_t now = millis();
  if (menuBtnWasDown != menuIsr.rawDown && now - menuIsr.lastEdgeMs >= DEBOUNCE_MS)
    dispatchMenuButton(menuIsr.rawDown, now);
  if (actionBtnWasDown != actionIsr.rawDown && now - actionIsr.lastEdgeMs >= DEBOUNCE_MS)
    dispatchActionButton(actionIsr.rawDown, now);
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
#if defined(DEVICE_SHAPE_ROUND)
  // Internal pull-up: idles high, button press grounds the pin (see
  // pollButtons() above and displays/LGFX_GC9B72_360.hpp's pin comment).
  pinMode(MENU_BTN_GPIO, INPUT_PULLUP);
  pinMode(ACTION_BTN_GPIO, INPUT_PULLUP);
  // CHANGE: fire on both press and release edges -- menuPinIsr()/
  // actionPinIsr() read the resulting level themselves rather than caring
  // which direction triggered them.
  attachInterrupt(digitalPinToInterrupt(MENU_BTN_GPIO), menuPinIsr, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ACTION_BTN_GPIO), actionPinIsr, CHANGE);
  diveCanHalInit();
#endif
}

void hal_loop(void)
{
  /* NO while loop in this function! (handled by framework) */
  // Proves this iteration started; if lv_timer_handler() below hangs, no
  // further resets happen and the watchdog fires.
  esp_task_wdt_reset();
#if defined(DEVICE_SHAPE_ROUND)
  pollButtons();
  diveCanHalPoll();
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
#else
// Round build's wake source is ACTION_BTN_GPIO itself (a valid RTC GPIO) --
// same active-low button pollButtons() reads while awake, so waking is just
// "press Action", matching its role as the wake button once the
// pollButtons() short-press path is live again post-wake.
constexpr gpio_num_t WAKE_GPIO = static_cast<gpio_num_t>(ACTION_BTN_GPIO);
#endif

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

  esp_sleep_enable_ext0_wakeup(WAKE_GPIO, 0 /* wake on low */);
  esp_deep_sleep_start(); // does not return
}

void hal_restore_from_sleep(void)
{
  if (rtcSleepStartSec == 0) return; // fresh power-on/reset — nothing pending
  if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_EXT0) return;

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
