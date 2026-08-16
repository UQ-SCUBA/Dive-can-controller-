#include "ui_hold_bar.h"
#include "state.h"
#include "ui_common.h"
#include "device_profile.h"

// Round counterpart to ui_hold_bar.cpp. The rect version pins a full-width
// strip to y=0, which on a round screen would sit almost entirely under the
// bezel (the safe chord at y=0 is a single point) -- recentered here as a
// narrower bar around the vertical middle instead, where the safe chord is
// widest.

namespace dc {

constexpr int HOLD_BAR_W = 240;
constexpr int HOLD_BAR_Y = 170;

static lv_obj_t *poHoldTrack;
static lv_obj_t *poHoldBar;

void uiHoldBarCreate(lv_obj_t *parent) {
  poHoldTrack = lv_obj_create(parent);
  lv_obj_remove_style_all(poHoldTrack);
  lv_obj_set_pos(poHoldTrack, (DEVICE_W - HOLD_BAR_W) / 2, HOLD_BAR_Y);
  lv_obj_set_size(poHoldTrack, HOLD_BAR_W, 3);
  lv_obj_set_style_bg_color(poHoldTrack, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(poHoldTrack, 38, 0); // ~0.15 (0-255 scale), matches the mockup
  lv_obj_add_flag(poHoldTrack, LV_OBJ_FLAG_HIDDEN);

  poHoldBar = lv_obj_create(parent);
  lv_obj_remove_style_all(poHoldBar);
  lv_obj_set_pos(poHoldBar, (DEVICE_W - HOLD_BAR_W) / 2, HOLD_BAR_Y);
  lv_obj_set_size(poHoldBar, 0, 3);
  lv_obj_set_style_bg_opa(poHoldBar, LV_OPA_COVER, 0);
  lv_obj_add_flag(poHoldBar, LV_OBJ_FLAG_HIDDEN);
}

void uiHoldBarUpdate() {
  float frac = 0.0f;
  lv_color_t color = colCyan();
  if (state.menuProgress > 0.0f) {
    frac = state.menuProgress;
    color = colCyan();
  } else if (state.actionProgress > 0.0f) {
    frac = state.actionProgress;
    color = colRed();
  }

  if (frac > 0.0f) {
    lv_obj_set_style_bg_color(poHoldBar, color, 0);
    lv_obj_set_width(poHoldBar, (int32_t)(HOLD_BAR_W * frac));
    lv_obj_remove_flag(poHoldTrack, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(poHoldBar, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(poHoldTrack, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(poHoldBar, LV_OBJ_FLAG_HIDDEN);
  }
}

} // namespace dc
