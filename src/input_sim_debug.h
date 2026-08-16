#pragma once

// Sim-only debug panel (see input_sim_debug.cpp) — not compiled into the
// `cyd` env (excluded via platformio.ini's build_src_filter). Only called
// from main.cpp's non-ARDUINO branch.

namespace dc {

void inputSimDebugCreate();

} // namespace dc
