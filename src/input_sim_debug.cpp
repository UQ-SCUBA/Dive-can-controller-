#ifndef ARDUINO

#include "input_sim_debug.h"
#include "state.h"
#include "gestures.h"
#include "device_profile.h"
#include "lvgl.h"
#include <cstdio>
#include <cstdint>

// Stands in for the HTML mockup's sidebar: the only way to drive depth/cell
// state/source while there's no real DiveCAN bus or depth sensor. Lives in
// the sim window's extra width (device area is 0..DEVICE_W, this panel
// starts just past it — see platformio.ini's SDL_HOR_RES, which is sized
// wider than DEVICE_W for both the rect and round emulator envs). Menu/
// Action here call the same onMenuDown()/onMenuUp()/onActionDown()/
// onActionUp() real button hardware will eventually call.

namespace {
constexpr int PANEL_X = dc::DEVICE_W + 10;
}

namespace dc {

static lv_obj_t *cellBtnLabel[NUM_CELLS];
static lv_obj_t *sourceBtnLabel;
static lv_obj_t *depthValueLabel;
static lv_obj_t *powerStatusLabel;

static void debugTickCb(lv_timer_t *timer) {
  LV_UNUSED(timer);
  lv_label_set_text(powerStatusLabel, state.sleeping ? "Device: ASLEEP (tap Action to wake)" : "Device: awake");
}

static void updateCellBtnLabel(int i) {
  const char *txt = state.cells[i] == CellState::Ok ? "OK"
                     : state.cells[i] == CellState::Voted ? "VOTED"
                                                           : "FAIL";
  char buf[24];
  snprintf(buf, sizeof(buf), "Cell %d: %s", i + 1, txt);
  lv_label_set_text(cellBtnLabel[i], buf);
}

static void cellBtnClicked(lv_event_t *e) {
  int i = (int)(intptr_t)lv_event_get_user_data(e);
  CellState s = state.cells[i];
  state.cells[i] = s == CellState::Ok       ? CellState::Voted
                   : s == CellState::Voted  ? CellState::Fail
                                             : CellState::Ok;
  updateCellBtnLabel(i);
}

static void updateSourceBtnLabel() {
  lv_label_set_text(sourceBtnLabel, state.source == Source::Loop ? "Source: LOOP" : "Source: BAILOUT");
}

static void sourceBtnClicked(lv_event_t *e) {
  LV_UNUSED(e);
  setSource(state.source == Source::Loop ? Source::Bailout : Source::Loop);
  updateSourceBtnLabel();
}

static void depthSliderChanged(lv_event_t *e) {
  lv_obj_t *slider = (lv_obj_t *)lv_event_get_target(e);
  state.depth = (float)lv_slider_get_value(slider);
  char buf[16];
  snprintf(buf, sizeof(buf), "Depth: %dm", (int)state.depth);
  lv_label_set_text(depthValueLabel, buf);
}

static void menuBtnPressed(lv_event_t *e) { LV_UNUSED(e); onMenuDown(); }
static void menuBtnReleased(lv_event_t *e) { LV_UNUSED(e); onMenuUp(); }
static void actionBtnPressed(lv_event_t *e) { LV_UNUSED(e); onActionDown(); }
static void actionBtnReleased(lv_event_t *e) { LV_UNUSED(e); onActionUp(); }

void inputSimDebugCreate() {
  lv_obj_t *panel = lv_obj_create(lv_screen_active());
  lv_obj_set_pos(panel, PANEL_X, 0);
  lv_obj_set_size(panel, 300, 320);
  lv_obj_set_style_pad_all(panel, 8, 0);

  lv_obj_t *title = lv_label_create(panel);
  lv_label_set_text(title, "DEBUG PANEL (sim only)");
  lv_obj_set_pos(title, 0, 0);

  // Both buttons get PRESSED/RELEASED (not CLICKED) so gestures.cpp can time
  // a hold — Menu alone for GAS_EDIT_HOLD_MS opens/closes the gas-edit page,
  // Action alone for SHUTDOWN_HOLD_MS at <0.2m depth sleeps the device (see
  // gesturesTick()). A short tap on either still dispatches its pressX()
  // via onXUp(). Action also doubles as the wake button once asleep.
  lv_obj_t *menuBtn = lv_button_create(panel);
  lv_obj_set_pos(menuBtn, 0, 26);
  lv_obj_set_size(menuBtn, 122, 34);
  lv_obj_add_event_cb(menuBtn, menuBtnPressed, LV_EVENT_PRESSED, nullptr);
  lv_obj_add_event_cb(menuBtn, menuBtnReleased, LV_EVENT_RELEASED, nullptr);
  lv_obj_t *menuLabel = lv_label_create(menuBtn);
  lv_label_set_text(menuLabel, "Menu");
  lv_obj_center(menuLabel);

  lv_obj_t *actionBtn = lv_button_create(panel);
  lv_obj_set_pos(actionBtn, 138, 26);
  lv_obj_set_size(actionBtn, 122, 34);
  lv_obj_add_event_cb(actionBtn, actionBtnPressed, LV_EVENT_PRESSED, nullptr);
  lv_obj_add_event_cb(actionBtn, actionBtnReleased, LV_EVENT_RELEASED, nullptr);
  lv_obj_t *actionLabel = lv_label_create(actionBtn);
  lv_label_set_text(actionLabel, "Action");
  lv_obj_center(actionLabel);

  lv_obj_t *slider = lv_slider_create(panel);
  lv_obj_set_pos(slider, 0, 90);
  lv_obj_set_size(slider, 260, 12);
  lv_slider_set_range(slider, 0, 60);
  lv_slider_set_value(slider, (int)state.depth, LV_ANIM_OFF);
  lv_obj_add_event_cb(slider, depthSliderChanged, LV_EVENT_VALUE_CHANGED, nullptr);

  depthValueLabel = lv_label_create(panel);
  lv_obj_set_pos(depthValueLabel, 0, 108);
  char depthBuf[16];
  snprintf(depthBuf, sizeof(depthBuf), "Depth: %dm", (int)state.depth);
  lv_label_set_text(depthValueLabel, depthBuf);

  powerStatusLabel = lv_label_create(panel);
  lv_obj_set_pos(powerStatusLabel, 0, 124);
  lv_label_set_text(powerStatusLabel, "Device: awake");
  lv_timer_create(debugTickCb, 100, nullptr);

  for (int i = 0; i < NUM_CELLS; i++) {
    lv_obj_t *btn = lv_button_create(panel);
    lv_obj_set_pos(btn, 0, 140 + i * 40);
    lv_obj_set_size(btn, 260, 34);
    lv_obj_add_event_cb(btn, cellBtnClicked, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    cellBtnLabel[i] = lv_label_create(btn);
    lv_obj_center(cellBtnLabel[i]);
    updateCellBtnLabel(i);
  }

  lv_obj_t *srcBtn = lv_button_create(panel);
  lv_obj_set_pos(srcBtn, 0, 264);
  lv_obj_set_size(srcBtn, 260, 34);
  lv_obj_add_event_cb(srcBtn, sourceBtnClicked, LV_EVENT_CLICKED, nullptr);
  sourceBtnLabel = lv_label_create(srcBtn);
  lv_obj_center(sourceBtnLabel);
  updateSourceBtnLabel();
}

} // namespace dc

#endif // ARDUINO
