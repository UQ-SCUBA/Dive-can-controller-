#include "state.h"
#include "deco.h"

namespace dc {

State state;
DecoCache cache;

float spValue(SpMode mode) {
  switch (mode) {
    case SpMode::Sp05: return 0.5f;
    case SpMode::Sp07: return 0.7f;
    case SpMode::Sp10: return 1.0f;
    case SpMode::Sp12: return 1.2f;
  }
  return 0.7f;
}

void stateInit() {
  state.gasMixes[0] = {"MIX 1", 0.18f, 0.45f, true};
  state.gasMixes[1] = {"MIX 2", 0.50f, 0.00f, true};
  state.gasMixes[2] = {"MIX 3", 1.00f, 0.00f, true};
  state.diluent = {"DIL", 0.21f, 0.00f, true};

  for (int i = 0; i < NUM_COMPARTMENTS; i++) {
    state.tissuesN2[i] = SURFACE_N2;
    state.tissuesHe[i] = 0.0f; // no He in the atmosphere
  }
}

GasMix &gasCandidate(int idx) { return idx < NUM_GAS_MIXES ? state.gasMixes[idx] : state.diluent; }

int gasCandidateIndex(const GasMix *g) {
  if (g == &state.diluent) return NUM_GAS_MIXES;
  for (int i = 0; i < NUM_GAS_MIXES; i++)
    if (g == &state.gasMixes[i]) return i;
  return -1;
}

void updateDiveTimer(float dtMin) {
  if (dtMin <= 0.0f) return;
  if (state.depth >= SURFACE_DEPTH_M) {
    state.diveTime += dtMin;
    state.surfaceTime = 0.0f;
    if (state.depth > state.maxDepth) state.maxDepth = state.depth;
  } else {
    state.surfaceTime += dtMin;
    if (state.surfaceTime >= DIVE_END_SURFACE_MIN) {
      state.diveTime = 0.0f;
      state.maxDepth = 0.0f;
    }
  }
}

} // namespace dc
