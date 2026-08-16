#pragma once

// Single source of truth for device-screen size/shape, driven by the
// DEVICE_SHAPE_ROUND build flag (set in platformio.ini's round/emulator_round
// envs). app.cpp uses this to size the shared deviceArea container; the
// per-screen ui_*.cpp files are swapped entirely per env (see
// build_src_filter) rather than branching on DEVICE_ROUND, since their
// layouts differ too much to share coordinates.

namespace dc {

#if defined(DEVICE_SHAPE_ROUND)
constexpr int DEVICE_W = 360;
constexpr int DEVICE_H = 360;
constexpr bool DEVICE_ROUND = true;
#else
constexpr int DEVICE_W = 320;
constexpr int DEVICE_H = 240;
constexpr bool DEVICE_ROUND = false;
#endif

} // namespace dc
