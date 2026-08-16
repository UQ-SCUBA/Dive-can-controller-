# DiveCAN Handset Firmware

LVGL 9.3 firmware for a custom CCR handset targeting a 2.1" round 360x360
GC9B72 SPI panel on an ESP32 dev board (bring-up board: ESPduino-32D): a
DiveCAN bus client that also runs its own Bühlmann ZHL-16C deco model. See
`~/Downloads/DiveCAN_Handset/` for the design summary, the APECS4 manual
this UI's layout is modeled on, and the HTML/canvas mockup this firmware is
being ported from.

The project originally targeted the rectangular ESP32-2432S028R ("Cheap
Yellow Display" — CYD); that target (`cyd`/`emulator_64bits`) is still in
the tree as a secondary/reference build, but the round panel is now the
primary hardware and `platformio.ini`'s default target.

Built on the official [`lvgl/lv_platformio`](https://github.com/lvgl/lv_platformio)
template, which gives us several PlatformIO environments sharing one codebase:

- **`round`** — the primary hardware target: a 2.1" round 360x360 GC9B72
  SPI panel (see `hal/esp32/displays/LGFX_GC9B72_360.hpp`), no touch IC on
  the panel itself. Flashed and verified on real hardware — display init,
  colors, and backlight all confirmed working. Menu/Action are read as
  capacitive touch pins (GPIO2/GPIO4) as an interim stand-in until real
  mechanical buttons are wired in.
- **`emulator_round`** — SDL2 sim of the round panel (360x360 window), for
  iterating on the round UI layout without the physical board.
- **`emulator_64bits`** — SDL2 sim of the original rectangular layout.
- **`cyd`** — the original rectangular ESP32-2432S028R build, via
  [LovyanGFX](https://github.com/lovyan03/LovyanGFX). Compiles, but is
  unverified against real hardware — kept as a secondary/fallback target
  now that `round` is primary.

## Project status (Milestone 1)

**Working:** main dive screen (PO2, per-cell status, S.P., source, depth,
deco stop/NDL, TTS, dive time, battery), the Gas Edit page, the full
ZHL-16C tissue-loading engine, short-press Menu/Action behavior (S.P. edit,
bailout gas select), and two of the three long-press gestures from the
mockup — hold Menu alone to open/close the Gas Edit page, hold Action
alone at the surface to power off. On the `round` target this is flashed
and confirmed working on real hardware (display, colors, backlight, and
touch-pin Menu/Action all verified); elsewhere it's driven by a debug
panel standing in for the real DiveCAN bus / depth sensor / physical
buttons.

**Not built yet (next milestones):**
- The Menu+Action combo gesture (OC/CCR loop/bailout toggle) from the
  mockup — the only one of the three long-press/combo gestures not yet
  implemented.
- Real DiveCAN/TWAI bus parsing — protocol reference is now in
  `docs/DiveCAN_Protocol_Reference.md` (compiled from
  github.com/QuickRecon/DiveCAN), so this is unblocked, just not
  implemented yet. Cell/status values are still debug-panel inputs, not
  real bus messages.
- Persisting gas mixes across power cycles (NVS/flash).
- Real mechanical Menu/Action buttons and the depth sensor, on `round` —
  currently touch pins (GPIO2/GPIO4) stand in for the buttons, and depth is
  still a debug-panel input.
- Physical Menu/Action input on `cyd`. That panel has no buttons of its
  own — whether that means two GPIO buttons wired in separately or
  touchscreen zones is still an open question, so nothing is wired up on
  that build target.

**Known risk:** the `round` target's init command sequence
(`hal/esp32/displays/LGFX_GC9B72_360.hpp`) is ported from a known-good
reference driver for this exact chip and has been verified on real
hardware (display brings up, colors correct after an `rgb_order` fix). The
SPI bus still runs faster (40MHz) than that reference driver's own tested
ceiling (~20MHz) — drop `cfg.freq_write` first if bring-up on a different
board shows corruption/artifacts. Separately, the `cyd` pin mapping in
`hal/esp32/displays/LGFX_CYD_2432S028.hpp` is still just the
widely-published community-standard one for that board, not verified
against real hardware. Common surprises on that board: `pin_rst` sometimes
actually wired to GPIO 4 instead of unconnected, and occasional
`rgb_order`/`invert` flips between production runs.

## Build / run

```sh
# simulator (SDL2 window) -- round panel layout
pio run -e emulator_round -t upload

# real hardware -- round GC9B72 panel, flash + run
pio run -e round -t upload

# rectangular CYD target -- secondary/reference, build-check only
pio run -e cyd
```

## Install SDL2 (required for the simulator)

**Linux (Ubuntu/Debian):**
```sh
sudo apt-get install libsdl2-dev       # 64-bit
# or: sudo apt-get install gcc-multilib g++-multilib libsdl2-dev:i386   # 32-bit
```

**macOS:** `brew install sdl2` (see `platformio.ini` for the Homebrew include/lib
path lines needed on Apple Silicon).

**Windows:** [MSYS2](https://www.msys2.org/) — `pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-SDL2`,
then add its `bin` folder to PATH.

## Adding a new display config

Only relevant if targeting different hardware later. Add a new
`hal/esp32/displays/*.hpp` exposing a LovyanGFX `tft` plus `WIDTH`/`HEIGHT`
macros (see `LGFX_GC9B72_360.hpp` or `LGFX_CYD_2432S028.hpp`), point
`hal/esp32/app_hal.cpp`'s single include at it, and add a matching
`[env:...]` in `platformio.ini`.

## Source layout

- `src/state.h/.cpp` — the handset's state struct (cells, setpoint, source,
  gas mixes, tissue loading), ported from the mockup's JS `state` object.
- `src/deco.h/.cpp` — the ZHL-16C engine: tissue integration, ceiling/NDL/
  stop-time/TTS projection.
- `src/gestures.h/.cpp` — Menu/Action short-press dispatch, plus the hold
  gestures (gas-edit toggle, shutdown) and their idle timeouts.
- `src/device_profile.h` — single source of truth for device screen
  size/shape (`DEVICE_W`/`DEVICE_H`/`DEVICE_ROUND`), driven by the
  `DEVICE_SHAPE_ROUND` build flag.
- `src/ui_common.h` — shared color palette and label-creation helpers used
  by every `ui_*.cpp` screen/overlay.
- `src/ui_dive_screen.h`, `.cpp`/`_round.cpp` — the main dive screen's LVGL
  widget tree + per-tick refresh; rect and round layouts are separate
  files swapped per env (see `platformio.ini`'s `build_src_filter`), not a
  single file branching on `DEVICE_SHAPE_ROUND`.
- `src/ui_gas_edit_screen.h`, `.cpp`/`_round.cpp` — the Gas Edit page,
  same rect/round split.
- `src/ui_hold_bar.h`, `.cpp`/`_round.cpp` — the thin progress bar shown
  across the top of the screen while a Menu/Action hold gesture is timing
  out toward firing.
- `src/ui_heartbeat.h`, `.cpp`/`_round.cpp` — the bottom-right liveness
  indicator (currently a punctuation-symbol spinner) proving the UI loop
  hasn't frozen, independent of the esp32 HAL's hardware watchdog.
- `src/fonts/` — `lv_font_conv`-generated 7-segment (DSEG7) fonts for the
  PO2 readout, round layout only.
- `src/app.h/.cpp` — orchestration: builds the device-screen container,
  owns the periodic tick timer.
- `src/input_sim_debug.h/.cpp` — sim-only debug panel (depth/cells/source/
  Menu/Action), excluded from both real-hardware builds (`cyd`, `round`).
- `hal/` — per-target display/input drivers (from the upstream template).
