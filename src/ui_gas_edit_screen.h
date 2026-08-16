#pragma once
#include "lvgl.h"

namespace dc {

// LVGL widget port of the mockup's drawGasEditPage() — the page opened by
// holding Menu alone for GAS_EDIT_HOLD_MS (see gestures.cpp). Creates its
// own full-screen, opaque container as a child of `parent`, so it must be
// created after uiDiveScreenCreate() to render on top of it.
void uiGasEditScreenCreate(lv_obj_t *parent);

// Refreshes text/highlight from dc::state. Only meaningful (and should only
// be called) while state.uiMode == UiMode::GasEdit.
void uiGasEditScreenUpdate();

// The screen's root container, for app.cpp to show/hide by uiMode.
lv_obj_t *uiGasEditScreenRoot();

} // namespace dc
