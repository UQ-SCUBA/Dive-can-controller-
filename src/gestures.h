#pragma once
#include "state.h"
#include <cstdint>

// Short-press Menu/Action dispatch and the hold gestures, ported from the
// mockup's pressMenu()/pressAction()/updateButtonHold().

namespace dc {

constexpr uint32_t SP_EDIT_TIMEOUT_MS = 5000; // matches the manual's 5s idle timeout
constexpr uint32_t GAS_EDIT_IDLE_TIMEOUT_MS = 15000; // gas-edit gets more room to think than SP_EDIT_TIMEOUT_MS
constexpr uint32_t SHUTDOWN_HOLD_MS = 3500; // hold Action alone at the surface (<0.2m) to power off
constexpr uint32_t GAS_EDIT_HOLD_MS = 3500; // hold Menu alone to open/close the gas-edit page
constexpr uint32_t COMBO_HOLD_MS = 3500; // hold Menu+Action together to toggle Loop/Bailout source
// Below this, releasing Menu is a short press (cycle S.P./gas/gas-edit-field);
// at or above it but short of GAS_EDIT_HOLD_MS, releasing is an aborted hold
// and does nothing — avoids accidentally cycling the field/mix when the
// diver meant to hold through to the gas-edit screen but let go early.
constexpr uint32_t MENU_SHORT_PRESS_MAX_MS = 2000;

void pressMenu();
void pressAction();

// Menu/Action press/release edges — call from whatever wires up real input
// (the sim debug panel today; GPIO or a touch zone once real-hardware input
// is decided). The onXDown()/onXUp() pair starts/stops timing a hold;
// onXUp() dispatches the short-press pressX() unless the hold already fired
// its gesture (see gesturesTick()) or ran long enough to count as an
// aborted hold rather than a short press (Menu only — see
// MENU_SHORT_PRESS_MAX_MS). onActionUp() also wakes the device back up if
// it was sleeping.
// Pass the edge timestamp when the input source has one (the ESP32 GPIO ISR
// does).  The zero default keeps simulator callers simple and uses LVGL's
// current tick there.
void onMenuDown(uint32_t nowMs = 0);
void onMenuUp(uint32_t nowMs = 0);
void onActionDown(uint32_t nowMs = 0);
void onActionUp(uint32_t nowMs = 0);

// Call periodically (e.g. every UI tick) with the current lv_tick_get() value
// — reverts uiMode to Dive after an idle timeout (SP_EDIT_TIMEOUT_MS, or
// GAS_EDIT_IDLE_TIMEOUT_MS while in the gas-edit page), turns a sustained
// Action hold into state.sleeping once it reaches SHUTDOWN_HOLD_MS, turns a
// sustained Menu hold into entering/exiting the gas-edit page once it
// reaches GAS_EDIT_HOLD_MS (from any uiMode — e.g. holding through an
// already-open S.P./gas-select box works, not just from a fresh press), and
// turns a sustained Menu+Action-together hold into a Loop/Bailout source
// toggle once it reaches COMBO_HOLD_MS (takes priority over the two
// individual-button hold gestures above while both are down).
void gesturesTick(uint32_t nowMs);

// Skips disabled slots; returns fromIdx unchanged if none are enabled.
int nextEnabledGasIndex(int fromIdx);

// Applies the current gas-edit field's +1%/toggle step to state.gasMixes.
void incrementGasField();

// Steps state.lastStopDepth up by STOP_INCREMENT, wrapping from
// LAST_STOP_MAX_M back to LAST_STOP_MIN_M.
void cycleLastStopDepth();

// Sets state.source. On Loop, state.bailoutGasIdx is just a standby hint —
// the PO2 block auto-tracks whatever the best enabled gas is instead of
// checking that specific index (see ui_dive_screen.cpp), so it can go
// stale while parked on the loop. Transitioning onto open-circuit commits
// it to the current best pick, so the diver starts bailout on a safe gas;
// from then on it's manual — bailout gas choice doesn't auto-change again
// until the diver picks one themself via GasSelect.
void setSource(Source s);

} // namespace dc
