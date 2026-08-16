# DiveCAN Handset Firmware

LVGL 9.3 firmware for a custom CCR handset targeting the ESP32-2432S028R
("Cheap Yellow Display" — CYD): a DiveCAN bus client that also runs its own
Bühlmann ZHL-16C deco model. See `~/Downloads/DiveCAN_Handset/` for the
design summary, the APECS4 manual this UI's layout is modeled on, and the
HTML/canvas mockup this firmware is being ported from.

Built on the official [`lvgl/lv_platformio`](https://github.com/lvgl/lv_platformio)
template, which gives us two PlatformIO environments sharing one codebase:

- **`emulator_64bits`** — runs against LVGL's SDL2 simulator on this machine.
  This is the primary target for now, since there's no CYD hardware on hand
  yet.
- **`cyd`** — the real ESP32-2432S028R build, via
  [LovyanGFX](https://github.com/lovyan03/LovyanGFX). Compiles, but is
  unverified — see "Known gaps" below.

## Project status (Milestone 1)

**Working (in the simulator):** main dive screen (PO2, per-cell status, S.P.,
source, depth, deco stop/NDL, TTS, dive time, battery), the full ZHL-16C
tissue-loading engine, and short-press Menu/Action behavior (S.P. edit,
bailout gas select). Driven by a debug panel standing in for the real DiveCAN
bus / depth sensor / physical buttons.

**Not built yet (next milestones):**
- The Gas Edit page and the three long-press/combo gestures (gas-edit hold,
  loop/bailout toggle, shutdown) from the mockup.
- Real DiveCAN/TWAI bus parsing — protocol reference is now in
  `docs/DiveCAN_Protocol_Reference.md` (compiled from
  github.com/QuickRecon/DiveCAN), so this is unblocked, just not
  implemented yet. Cell/status values are still debug-panel inputs, not
  real bus messages.
- Persisting gas mixes across power cycles (NVS/flash).
- Physical Menu/Action button input on real hardware. The CYD has no
  buttons of its own — whether that means two GPIO buttons wired in
  separately or touchscreen zones is still an open question, so nothing is
  wired up on the `cyd` build target yet.

**Known risk:** the CYD pin mapping in
`hal/esp32/displays/LGFX_CYD_2432S028.hpp` is the widely-published
community-standard one for this board, not yet verified against real
hardware. Common surprises on this board: `pin_rst` sometimes actually wired
to GPIO 4 instead of unconnected, and occasional `rgb_order`/`invert` flips
between production runs. Recheck once the board arrives.

## Build / run

```sh
# simulator (SDL2 window)
pio run -e emulator_64bits -t upload

# real hardware build-check only — no board to flash yet
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
macros (see `LGFX_CYD_2432S028.hpp`), point `hal/esp32/app_hal.cpp`'s single
include at it, and add a matching `[env:...]` in `platformio.ini`.

## Source layout

- `src/state.h/.cpp` — the handset's state struct (cells, setpoint, source,
  gas mixes, tissue loading), ported from the mockup's JS `state` object.
- `src/deco.h/.cpp` — the ZHL-16C engine: tissue integration, ceiling/NDL/
  stop-time/TTS projection.
- `src/gestures.h/.cpp` — Menu/Action short-press dispatch.
- `src/ui_dive_screen.h/.cpp` — the LVGL widget tree + per-tick refresh.
- `src/app.h/.cpp` — orchestration: builds the device-screen container,
  owns the periodic tick timer.
- `src/input_sim_debug.h/.cpp` — sim-only debug panel (depth/cells/source/
  Menu/Action), excluded from the `cyd` build.
- `hal/` — per-target display/input drivers (from the upstream template).
