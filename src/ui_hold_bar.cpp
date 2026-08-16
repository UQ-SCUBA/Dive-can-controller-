#include "ui_hold_bar.h"
#include "state.h"
#include "ui_common.h"

namespace dc {

static lv_obj_t *poHoldTrack; // faint full-width track behind the fill
static lv_obj_t *poHoldBar;   // colored fill, width proportional to hold progress

void uiHoldBarCreate(lv_obj_t *parent) {
  poHoldTrack = lv_obj_create(parent);
  lv_obj_remove_style_all(poHoldTrack);
  lv_obj_set_pos(poHoldTrack, 0, 0);
  lv_obj_set_size(poHoldTrack, 320, 3);
  lv_obj_set_style_bg_color(poHoldTrack, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(poHoldTrack, 38, 0); // ~0.15 (0-255 scale), matches the mockup
  lv_obj_add_flag(poHoldTrack, LV_OBJ_FLAG_HIDDEN);

  poHoldBar = lv_obj_create(parent);
  lv_obj_remove_style_all(poHoldBar);
  lv_obj_set_pos(poHoldBar, 0, 0);
  lv_obj_set_size(poHoldBar, 0, 3);
  lv_obj_set_style_bg_opa(poHoldBar, LV_OPA_COVER, 0);
  lv_obj_add_flag(poHoldBar, LV_OBJ_FLAG_HIDDEN);
}

void uiHoldBarUpdate() {
  // Priority order mirrors the mockup's drawHoldProgress(): combo (source
  // toggle, yellow) > Menu-hold (gas-edit toggle, cyan) > Action-hold
  // (shutdown, red).
  float frac = 0.0f;
  lv_color_t color = colCyan();
  if (state.comboProgress > 0.0f) {
    frac = state.comboProgress;
    color = colYellow();
  } else if (state.menuProgress > 0.0f) {
    frac = state.menuProgress;
    color = colCyan();
  } else if (state.actionProgress > 0.0f) {
    frac = state.actionProgress;
    color = colRed();
  }

  if (frac > 0.0f) {
    lv_obj_set_style_bg_color(poHoldBar, color, 0);
    lv_obj_set_width(poHoldBar, (int32_t)(320 * frac));
    lv_obj_remove_flag(poHoldTrack, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(poHoldBar, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(poHoldTrack, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(poHoldBar, LV_OBJ_FLAG_HIDDEN);
  }
}

} // namespace dc
