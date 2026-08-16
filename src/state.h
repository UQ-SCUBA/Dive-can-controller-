#pragma once
#include <cstdint>

// Direct port of the HTML mockup's `state` object
// (~/Downloads/DiveCAN_Handset/handset-display-preview.html) to a C++ struct.

namespace dc {

enum class CellState { Ok, Voted, Fail };
enum class Source { Loop, Bailout };
enum class SpMode { Sp05, Sp07, Sp10, Sp12 }; // 0.5 / 0.7 / 1.0 / 1.2 bar
enum class UiMode { Dive, SpEdit, GasSelect, GasEdit };
enum class GasEditField { Fo2, Fhe, Enabled };

constexpr int NUM_CELLS = 3;
constexpr int NUM_GAS_MIXES = 3; // plus state.diluent, tracked separately
// The full bailout-candidate space: the NUM_GAS_MIXES slots plus the
// diluent (see gasCandidate()) — the diluent doubles as a bailout option.
constexpr int NUM_BAILOUT_CANDIDATES = NUM_GAS_MIXES + 1;
constexpr int NUM_COMPARTMENTS = 16;

// At/deeper than this the diver is considered submerged; shallower is "at
// the surface" — matches the manual's power-off depth gate (see
// gestures.cpp's canShutdown).
constexpr float SURFACE_DEPTH_M = 0.2f;
// Continuous minutes at the surface before the displayed dive timer resets.
constexpr float DIVE_END_SURFACE_MIN = 2.0f;

struct GasMix {
  const char *label;
  float fo2;
  float fhe; // reserved for trimix; FN2 implied = 1 - fo2 - fhe
  bool enabled;
};

struct State {
  CellState cells[NUM_CELLS] = {CellState::Ok, CellState::Ok, CellState::Ok};
  SpMode spMode = SpMode::Sp07;
  Source source = Source::Loop;
  int bailoutGasIdx = 0;

  float depth = 24.0f; // meters
  bool wetSensor = false;

  // Tracked separately per inert gas — ZHL-16 half-times/M-values differ
  // for He vs N2, and a compartment's ceiling depends on the *combined*
  // loading (see deco.cpp's ceilingDepthFor()), not just total inert
  // pressure, so a single lumped array can't represent trimix correctly.
  float tissuesN2[NUM_COMPARTMENTS];
  float tissuesHe[NUM_COMPARTMENTS];
  float simMinutes = 0.0f;
  bool simRunning = true;
  // Scales real elapsed time into simulated dive time (see app.cpp's
  // tickTimerCb(): dtMin = dtRealSec * (simSpeed/60)) -- 1 = real time (1
  // sim-minute passes per real minute), 60 = 60x fast-forward. Was left at
  // 60 for bench testing (to watch NDL/deco countdowns move without
  // waiting); real time is the correct default now that there's a real
  // depth input to eventually replace this whole tissue-loading stand-in.
  int simSpeed = 1;

  // Displayed "DIVE TIME" — independent of the tissue/deco integration
  // above (which always tracks actual elapsed time regardless of depth).
  // Only advances while submerged (depth >= SURFACE_DEPTH_M); pauses at the
  // surface and resets to 0 after DIVE_END_SURFACE_MIN spent there
  // continuously. See state.cpp's updateDiveTimer().
  float diveTime = 0.0f;
  float surfaceTime = 0.0f; // continuous minutes spent at the surface (depth < SURFACE_DEPTH_M)

  // Deepest depth reached this dive — resets alongside diveTime (see
  // state.cpp's updateDiveTimer()).
  float maxDepth = 0.0f;

  UiMode uiMode = UiMode::Dive;
  SpMode spCursor = SpMode::Sp07;
  int gasCursor = 0;

  int gasEditSlot = 0;
  GasEditField gasEditField = GasEditField::Fo2;
  bool gasEditLastStopSelected = false; // cursor is on the gas-edit page's LAST STOP line, not a gas field
  bool gasEditDiluentSelected = false;  // cursor is on the DILUENT row (only Fo2/Fhe apply, no Enabled)

  // Shallowest depth the last deco stop will be reported at — see
  // deco.h's LAST_STOP_MIN_M/LAST_STOP_MAX_M and deco.cpp's
  // ceilingDepthFor(). Set from the gas-edit page.
  int lastStopDepth = 3;

  GasMix gasMixes[NUM_GAS_MIXES];

  // What's actually filling the loop — distinct from the bailout slots
  // above, and used for tissue loading while on Source::Loop. Also doubles
  // as a bailout candidate in its own right (see gasCandidate() below) —
  // the diluent is always breathable, so its `enabled` stays true and is
  // never exposed for toggling on the gas-edit page.
  GasMix diluent;

  // Hold-to-shutdown gesture (see gestures.cpp) and its resulting power
  // state. `sleeping` true means the screen is blanked and, on the esp32
  // target, the chip is actually deep-asleep — see app_hal's
  // hal_enter_sleep()/hal_restore_from_sleep().
  bool sleeping = false;
  bool actionDown = false;
  bool actionLongFired = false;
  uint32_t actionDownAtMs = 0; // 0 = not currently timing a hold
  float actionProgress = 0.0f; // 0..1 while holding toward the shutdown gesture; 0 otherwise

  // Hold-Menu-alone gesture: opens/closes the gas-edit page.
  bool menuDown = false;
  bool menuLongFired = false;
  uint32_t menuDownAtMs = 0; // 0 = not currently timing a hold
  float menuProgress = 0.0f; // 0..1 while holding toward the gas-edit toggle; 0 otherwise

  // Hold-Menu+Action-together gesture: toggles source between Loop (CCR)
  // and Bailout (OC). Tracked separately from menuDown/actionDown above
  // (those still drive the individual gas-edit/shutdown hold gestures) —
  // see gesturesTick()'s canCombo.
  bool comboLongFired = false;
  uint32_t comboDownAtMs = 0; // 0 = not currently timing a hold
  float comboProgress = 0.0f; // 0..1 while holding toward the source toggle; 0 otherwise
};

// Decompression display values, recomputed periodically — mirrors the
// mockup's `cache` object rather than being derived every frame.
struct DecoCache {
  int ceilD = 0;   // meters; 0 = no obligation
  int ndl = -1;    // minutes; -1 = not yet computed
  int stopRemain = -1;
  int tts = -1;
  uint32_t lastCalcMs = 0;
};

extern State state;
extern DecoCache cache;

float spValue(SpMode mode);
void stateInit();

// Bailout-candidate accessors, spanning state.gasMixes[] plus state.diluent
// as one logical list of NUM_BAILOUT_CANDIDATES options (indices
// 0..NUM_GAS_MIXES-1 are the mixes, NUM_GAS_MIXES is the diluent). Used
// wherever gas selection/cycling needs to treat the diluent as just another
// bailout choice instead of special-casing it.
GasMix &gasCandidate(int idx);
// Reverse lookup for a pointer returned by pickAutoBailout()/
// pickBestBailoutGas() — those can't do plain pointer arithmetic against
// state.gasMixes since the result might be &state.diluent instead, which
// isn't contiguous with the array. -1 if g isn't a candidate pointer.
int gasCandidateIndex(const GasMix *g);

// Advances or pauses/resets state.diveTime per state.depth — see the field
// comments above. Deliberately separate from deco.cpp/integrateLiveTissues:
// tissue loading always tracks real elapsed time regardless of depth, but
// the displayed dive clock should not.
void updateDiveTimer(float dtMin);

} // namespace dc
