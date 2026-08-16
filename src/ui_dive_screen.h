#pragma once
#include "lvgl.h"

namespace dc {

// Creates all dive-screen widgets as children of `parent`, which is expected
// to be a 320x240 black-background container representing the device screen
// (the same container app.cpp mounts the Menu/Action buttons under).
void uiDiveScreenCreate(lv_obj_t *parent);

// Refreshes label text/colors from dc::state / dc::cache. Call periodically
// (e.g. every ~100ms) — mirrors the mockup's drawDiveScreen().
void uiDiveScreenUpdate();

} // namespace dc
