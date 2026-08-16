#pragma once
#include "lvgl.h"

namespace dc {

// Call once from setup()/main(). Builds the 320x240 device-screen container
// (mounted top-left of whatever the active LVGL display is — exactly fills
// a real 320x240 CYD screen, or the top-left corner of the wider sim window
// that leaves room for the debug panel), then starts the periodic tick that
// drives tissue integration, the deco recompute, and the UI refresh.
void appInit();

} // namespace dc
