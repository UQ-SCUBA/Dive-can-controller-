#include "gestures.h"
#include "deco.h"
#include "lvgl.h"

namespace dc {

static uint32_t lastInteractionMs = 0;
static uint32_t sleepStartMs = 0; // lv_tick_get() at the moment sleeping went true
static bool wakeArmed = false;    // true once a fresh press-down lands while sleeping

static const SpMode SP_ORDER[] = {SpMode::Sp05, SpMode::Sp07, SpMode::Sp10, SpMode::Sp12};
static const int SP_ORDER_LEN = 4;

static int spOrderIndex(SpMode m) {
  for (int i = 0; i < SP_ORDER_LEN; i++)
    if (SP_ORDER[i] == m) return i;
  return 0;
}

int nextEnabledGasIndex(int fromIdx) {
  for (int step = 1; step <= NUM_BAILOUT_CANDIDATES; step++) {
    int idx = (fromIdx + step) % NUM_BAILOUT_CANDIDATES;
    if (gasCandidate(idx).enabled) return idx;
  }
  return fromIdx; // none enabled — stay put
}

void setSource(Source s) {
  if (s == Source::Bailout && state.source != Source::Bailout) {
    const GasMix *best = pickBestBailoutGas(state.depth);
    if (best) state.bailoutGasIdx = gasCandidateIndex(best);
  }
  state.source = s;
}

void pressMenu() {
  if (state.sleeping) return; // powered off — ignore input until woken
  lastInteractionMs = lv_tick_get();
  if (state.uiMode == UiMode::Dive) {
    // on bailout, S.P. doesn't apply — Menu opens gas select instead
    if (state.source == Source::Bailout) {
      state.uiMode = UiMode::GasSelect;
      state.gasCursor = gasCandidate(state.bailoutGasIdx).enabled
                             ? state.bailoutGasIdx
                             : nextEnabledGasIndex(state.bailoutGasIdx);
    } else {
      state.uiMode = UiMode::SpEdit;
      state.spCursor = state.spMode;
    }
  } else if (state.uiMode == UiMode::SpEdit) {
    state.spCursor = SP_ORDER[(spOrderIndex(state.spCursor) + 1) % SP_ORDER_LEN];
  } else if (state.uiMode == UiMode::GasSelect) {
    state.gasCursor = nextEnabledGasIndex(state.gasCursor);
  } else if (state.uiMode == UiMode::GasEdit) {
    constexpr int FIELD_COUNT = 3; // Fo2, Fhe, Enabled
    constexpr int GAS_FIELD_TOTAL = NUM_GAS_MIXES * FIELD_COUNT;
    constexpr int DILUENT_FIELD_COUNT = 2; // Fo2, Fhe only — the diluent can't be disabled
    constexpr int TOTAL = GAS_FIELD_TOTAL + 1 /*LAST STOP*/ + DILUENT_FIELD_COUNT;
    // Flat cursor order: the 4 bailout slots, then LAST STOP, then DILUENT.
    int flat;
    if (state.gasEditDiluentSelected) flat = GAS_FIELD_TOTAL + 1 + (int)state.gasEditField;
    else if (state.gasEditLastStopSelected) flat = GAS_FIELD_TOTAL;
    else flat = state.gasEditSlot * FIELD_COUNT + (int)state.gasEditField;

    flat = (flat + 1) % TOTAL;

    state.gasEditLastStopSelected = flat == GAS_FIELD_TOTAL;
    state.gasEditDiluentSelected = flat > GAS_FIELD_TOTAL;
    if (state.gasEditDiluentSelected) {
      state.gasEditField = (GasEditField)(flat - GAS_FIELD_TOTAL - 1); // 0=Fo2, 1=Fhe
    } else if (!state.gasEditLastStopSelected) {
      state.gasEditSlot = flat / FIELD_COUNT;
      state.gasEditField = (GasEditField)(flat % FIELD_COUNT);
    }
  }
}

void incrementGasField() {
  GasMix &g = state.gasEditDiluentSelected ? state.diluent : state.gasMixes[state.gasEditSlot];
  if (state.gasEditField == GasEditField::Fo2) {
    int v = (int)(g.fo2 * 100.0f + 0.5f) + 1;
    if (v > 100) v = 10;
    float fo2 = v / 100.0f;
    if (fo2 + g.fhe > 1.0f) g.fhe = (1.0f - fo2) < 0.0f ? 0.0f : (1.0f - fo2);
    g.fo2 = fo2;
  } else if (state.gasEditField == GasEditField::Fhe) {
    int v = (int)(g.fhe * 100.0f + 0.5f) + 1;
    if (v > 90) v = 0;
    float fhe = v / 100.0f;
    if (g.fo2 + fhe > 1.0f) g.fhe = (1.0f - g.fo2) < 0.0f ? 0.0f : (1.0f - g.fo2);
    else g.fhe = fhe;
  } else {
    g.enabled = !g.enabled;
  }
}

void cycleLastStopDepth() {
  state.lastStopDepth += STOP_INCREMENT;
  if (state.lastStopDepth > LAST_STOP_MAX_M) state.lastStopDepth = LAST_STOP_MIN_M;
}

void pressAction() {
  lastInteractionMs = lv_tick_get();
  if (state.uiMode == UiMode::SpEdit) {
    state.spMode = state.spCursor;
    state.uiMode = UiMode::Dive;
  } else if (state.uiMode == UiMode::GasSelect) {
    state.bailoutGasIdx = state.gasCursor;
    state.uiMode = UiMode::Dive;
  } else if (state.uiMode == UiMode::GasEdit) {
    if (state.gasEditLastStopSelected) cycleLastStopDepth();
    else incrementGasField();
  }
}

void onMenuDown() {
  if (state.sleeping) return; // powered off — ignore input until woken
  state.menuDown = true;
  state.menuLongFired = false;
  lastInteractionMs = lv_tick_get();
}

void onMenuUp() {
  if (!state.menuDown) return;
  uint32_t heldMs = state.menuDownAtMs == 0 ? 0 : (lv_tick_get() - state.menuDownAtMs);
  // Below MENU_SHORT_PRESS_MAX_MS: a short press — cycle S.P./gas/gas-edit
  // field. At/above it (but under GAS_EDIT_HOLD_MS, so the long-press
  // gesture hasn't already fired): an aborted hold, so letting go early
  // while reaching for the gas-edit screen doesn't also cycle whatever
  // field is underneath it.
  bool fireShort = !state.menuLongFired && heldMs < MENU_SHORT_PRESS_MAX_MS;
  state.menuDown = false;
  state.menuDownAtMs = 0;
  state.menuProgress = 0.0f;
  if (fireShort) pressMenu();
}

void onActionDown() {
  if (state.sleeping) {
    wakeArmed = true; // this press is a wake attempt, not a hold — see onActionUp()
    return;
  }
  state.actionDown = true;
  state.actionLongFired = false;
  lastInteractionMs = lv_tick_get();
}

void onActionUp() {
  if (state.sleeping) {
    if (!wakeArmed) return; // e.g. the tail end of the hold that caused sleep — not a fresh press
    wakeArmed = false;
    state.actionDown = false;
    state.actionLongFired = false;

    // Recalculate tissue loading for the time spent "off", as if breathing
    // air at the surface the whole time — and count that time toward the
    // surface interval, same as updateDiveTimer() would if ticks had kept
    // running (sleeping is only reachable at the surface, so all of it is
    // surface time).
    uint32_t elapsedMs = lv_tick_get() - sleepStartMs;
    float elapsedMin = elapsedMs / 60000.0f;
    if (elapsedMin > 0.0f) {
      stepTissues(state.tissuesN2, state.tissuesHe, SURFACE_N2, 0.0f, elapsedMin);
      state.surfaceTime += elapsedMin;
      if (state.surfaceTime >= DIVE_END_SURFACE_MIN) {
        state.diveTime = 0.0f;
        state.maxDepth = 0.0f;
      }
    }

    state.sleeping = false;
    lastInteractionMs = lv_tick_get();
    return;
  }

  if (!state.actionDown) return;
  bool fireShort = !state.actionLongFired;
  state.actionDown = false;
  state.actionDownAtMs = 0;
  state.actionProgress = 0.0f;
  if (fireShort) pressAction();
}

// Accumulates a button hold's progress (0..1) toward `holdMs` into
// *progress each tick. Returns true exactly once — the tick the hold first
// reaches holdMs — having already reset *down/*downAtMs/*progress ready for
// the next press, so the caller only needs to react to the gesture firing.
// While !canHold (button up, or some other condition blocking the gesture),
// just keeps the timer/progress cleared.
static bool accumulateHold(bool canHold, uint32_t nowMs, uint32_t holdMs, bool *down,
                            uint32_t *downAtMs, bool *longFired, float *progress) {
  if (!canHold) {
    *downAtMs = 0;
    *progress = 0.0f;
    return false;
  }
  if (*downAtMs == 0) *downAtMs = nowMs;
  uint32_t heldMs = nowMs - *downAtMs;
  *progress = heldMs >= holdMs ? 1.0f : (float)heldMs / (float)holdMs;
  if (*longFired || heldMs < holdMs) return false;
  *longFired = true;
  *down = false;
  *downAtMs = 0;
  *progress = 0.0f;
  return true;
}

void gesturesTick(uint32_t nowMs) {
  uint32_t idleTimeout = state.uiMode == UiMode::GasEdit ? GAS_EDIT_IDLE_TIMEOUT_MS : SP_EDIT_TIMEOUT_MS;
  // Exempt while Menu is actively held: guards a hold-to-gas-edit started
  // from S.P./gas-select against getting yanked back to Dive by the idle
  // timeout mid-hold, regardless of how GAS_EDIT_HOLD_MS and
  // SP_EDIT_TIMEOUT_MS happen to compare.
  if (!state.menuDown && state.uiMode != UiMode::Dive && (nowMs - lastInteractionMs) > idleTimeout) {
    state.uiMode = UiMode::Dive; // idle timeout — cancels without committing
  }

  bool canShutdown = state.uiMode == UiMode::Dive && state.depth < SURFACE_DEPTH_M;
  if (accumulateHold(state.actionDown && canShutdown, nowMs, SHUTDOWN_HOLD_MS, &state.actionDown,
                      &state.actionDownAtMs, &state.actionLongFired, &state.actionProgress)) {
    state.sleeping = true;
    sleepStartMs = nowMs;
  }

  // No uiMode restriction: holding through an already-open S.P./gas-select
  // box (e.g. short-press-then-hold) reaches the gas-edit screen too, per
  // onMenuUp()'s tiered short-press/aborted-hold/long-press-fire scheme.
  bool canGasEditHold = state.menuDown && !state.actionDown;
  if (accumulateHold(canGasEditHold, nowMs, GAS_EDIT_HOLD_MS, &state.menuDown, &state.menuDownAtMs,
                      &state.menuLongFired, &state.menuProgress)) {
    if (state.uiMode == UiMode::GasEdit) {
      state.uiMode = UiMode::Dive;
    } else {
      state.uiMode = UiMode::GasEdit;
      // bailoutGasIdx can point at the diluent (see gasCandidate()), which
      // isn't a valid state.gasMixes[] index — land the cursor on the
      // DILUENT row instead of an out-of-range gasEditSlot.
      state.gasEditDiluentSelected = state.bailoutGasIdx >= NUM_GAS_MIXES;
      state.gasEditSlot = state.gasEditDiluentSelected ? 0 : state.bailoutGasIdx;
      state.gasEditField = GasEditField::Fo2;
      state.gasEditLastStopSelected = false;
    }
    lastInteractionMs = nowMs;
  }
}

} // namespace dc
