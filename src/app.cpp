#include "app.h"
#include "state.h"
#include "deco.h"
#include "gestures.h"
#include "ui_dive_screen.h"
#include "ui_gas_edit_screen.h"
#include "ui_hold_bar.h"
#include "ui_heartbeat.h"
#include "app_hal.h"
#include "device_profile.h"

// Note on Menu/Action input: the CYD has no physical buttons of its own
// (unlike the APECS4 this UI is modeled on), so real-hardware input is still
// an open question — either two GPIO buttons wired in separately, or
// touchscreen zones. Not decided yet, so it's not wired up on the `cyd`
// build target. For now onMenuDown()/onMenuUp()/onActionDown()/onActionUp()
// are only ever called from the sim-only debug panel (see
// input_sim_debug.cpp), which is enough to exercise the whole state
// machine — short presses, the hold-Menu gas-edit toggle, and the
// hold-Action-to-sleep gesture (see gestures.h's GAS_EDIT_HOLD_MS/
// SHUTDOWN_HOLD_MS) — while the input question gets settled.

namespace dc {

static lv_obj_t *deviceArea;

static uint32_t lastTickMs = 0;
static bool wasSleeping = false;

static void tickTimerCb(lv_timer_t *timer) {
  LV_UNUSED(timer);
  uint32_t now = lv_tick_get();
  if (lastTickMs == 0) lastTickMs = now;
  float dtRealSec = (now - lastTickMs) / 1000.0f;
  lastTickMs = now;

  gesturesTick(now);

  if (state.sleeping && !wasSleeping) {
    lv_obj_add_flag(deviceArea, LV_OBJ_FLAG_HIDDEN);
    // esp32: persists tissues + a wake-elapsed-time reference and deep-sleeps
    // the chip — does not return. Other targets: no-op; the screen just
    // stays blank (above) until Action is pressed again.
    hal_enter_sleep();
  } else if (!state.sleeping && wasSleeping) {
    lv_obj_remove_flag(deviceArea, LV_OBJ_FLAG_HIDDEN);
  }
  wasSleeping = state.sleeping;

  if (state.sleeping) return; // powered off — no sensing/computation/redraw

  if (state.simRunning) {
    float dtMin = dtRealSec * (state.simSpeed / 60.0f);
    integrateLiveTissues(dtMin);
    updateDiveTimer(dtMin);
  }
  recomputeDecoDisplay();

  if (state.uiMode == UiMode::GasEdit) {
    lv_obj_remove_flag(uiGasEditScreenRoot(), LV_OBJ_FLAG_HIDDEN);
    uiGasEditScreenUpdate();
  } else {
    lv_obj_add_flag(uiGasEditScreenRoot(), LV_OBJ_FLAG_HIDDEN);
    uiDiveScreenUpdate();
  }
  uiHoldBarUpdate();
  uiHeartbeatUpdate(dtRealSec);
}

void appInit() {
  stateInit();

  deviceArea = lv_obj_create(lv_screen_active());
  lv_obj_remove_style_all(deviceArea);
  lv_obj_remove_flag(deviceArea, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_pos(deviceArea, 0, 0);
  lv_obj_set_size(deviceArea, DEVICE_W, DEVICE_H);
  uiDiveScreenCreate(deviceArea);
  uiGasEditScreenCreate(deviceArea); // created after, so it renders on top of the dive screen
  uiHoldBarCreate(deviceArea);       // created after, so it renders on top of both screens
  uiHeartbeatCreate(deviceArea);     // created last, so it's visible over everything, on every screen

#if defined(DEVICE_SHAPE_ROUND) && !defined(ARDUINO)
  // Layout aid only (sim/emulator builds, not real hardware): a thin outline
  // right at the round panel's actual visible edge -- the physical bezel
  // masks everything outside this circle on the real device, so anything
  // drawn near/past it needs to be checked against this, not against the
  // square window's corners. Created last so it stays on top of every
  // screen's content instead of getting drawn over.
  lv_obj_t *roundEdgeGuide = lv_obj_create(deviceArea);
  lv_obj_remove_style_all(roundEdgeGuide);
  lv_obj_remove_flag(roundEdgeGuide, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_pos(roundEdgeGuide, 0, 0);
  lv_obj_set_size(roundEdgeGuide, DEVICE_W, DEVICE_H);
  lv_obj_set_style_radius(roundEdgeGuide, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(roundEdgeGuide, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_color(roundEdgeGuide, lv_color_white(), 0);
  lv_obj_set_style_border_width(roundEdgeGuide, 1, 0);
  lv_obj_set_style_border_opa(roundEdgeGuide, LV_OPA_COVER, 0);
#endif

  lv_timer_create(tickTimerCb, 100, nullptr);
}

} // namespace dc
