#pragma once
#include "lvgl.h"

namespace dc {

// Small ASCII spinner in the bottom right — a purely visual liveness cue
// (the UI is still updating, not frozen), independent of the esp32 HAL's
// hardware watchdog. Created after every screen so it renders on top and
// stays visible regardless of uiMode.
void uiHeartbeatCreate(lv_obj_t *parent);

// Call every tick with real elapsed seconds (not sim-scaled) so the spinner
// advances a steady one frame per second regardless of dive-sim speed.
void uiHeartbeatUpdate(float dtRealSec);

} // namespace dc
