#pragma once
#include "lvgl.h"

namespace dc {

// The thin hold-progress bar across the top of the screen, ported from the
// mockup's drawHoldProgress(). It overlays whichever screen is active (dive
// screen or gas-edit page), so it's created after both and owns its own
// tick — see app.cpp.
void uiHoldBarCreate(lv_obj_t *parent);
void uiHoldBarUpdate();

} // namespace dc
