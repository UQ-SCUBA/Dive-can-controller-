#pragma once
#include "state.h"

// Port of the mockup's ZHL-16C engine — same math, same function names,
// same simplifications (see the mockup's disclaimer: reference
// implementation for UI/logic prototyping only, not validated dive-planning
// software).

namespace dc {

extern const float HALFTIMES[NUM_COMPARTMENTS]; // N2 half-times, minutes
extern const float A_COEF[NUM_COMPARTMENTS];
extern const float B_COEF[NUM_COMPARTMENTS];

extern const float HALFTIMES_HE[NUM_COMPARTMENTS]; // He half-times, minutes
extern const float A_HE_COEF[NUM_COMPARTMENTS];
extern const float B_HE_COEF[NUM_COMPARTMENTS];

constexpr float PH2O = 0.0627f;                      // bar, alveolar water vapor
constexpr float SURFACE_N2 = (1.0f - PH2O) * 0.79f;  // starting tissue loading
constexpr int STOP_INCREMENT = 3;                    // meters
constexpr float ASCENT_RATE = 9.0f;                  // m/min
constexpr float MOD_CEILING = 1.4f;                  // bar, reference PPO2 for MOD

// Working-PPO2 alert band, used for the live PO2 readout/warnings — a wider,
// more permissive range than MOD_CEILING (which is a planning limit for
// choosing gases, not a live alarm threshold).
constexpr float PPO2_HYPOXIA_MIN = 0.18f; // bar, below this = hypoxia risk
constexpr float PPO2_TOX_MAX = 1.6f;      // bar, above this = O2 tox risk

// Deeper than this, narcosis is enough of a concern that a safe (non-
// hypoxic, non-hyperoxic) helium-containing bailout candidate takes
// precedence over a non-helium one, even if the non-helium option is
// richer — helium isn't narcotic, nitrogen is. Shallower than this,
// narcosis isn't a practical concern, so the helium fraction is ignored
// and selection is purely by PPO2 band + richness, same as before.
constexpr float NARCOSIS_HE_PRIORITY_DEPTH_M = 35.0f;

// User-selectable floor for ceilingDepthFor() (see state.lastStopDepth):
// once a stop is required at all, it's never reported shallower than this,
// so the diver holds one longer stop here instead of graduated stops down
// to STOP_INCREMENT. Both are multiples of STOP_INCREMENT.
constexpr int LAST_STOP_MIN_M = 3;
constexpr int LAST_STOP_MAX_M = 9;

float ata(float depthM);

// Effective PPO2 source, mirrors the mockup's getEffectiveSource(): resolves
// auto-bailout-on-3-cell-fail, manual bailout, or loop consensus.
struct EffectiveSource {
  bool isBailout;
  bool isAuto;           // true if this is the automatic 3-cell-fail bailout
  const GasMix *gas;     // the active bailout gas; nullptr if bailout but none is safe
};
EffectiveSource getEffectiveSource();

// True when the S.P. field should show blanked ("--") rather than the
// current setpoint value: bailed out (manual bailout or the automatic
// 3-cell-fail one — see getEffectiveSource()), unless actively editing it.
// state.spMode itself is never touched while this is true (pressMenu() keeps
// SpEdit unreachable during manual bailout), so it comes back unchanged once
// back on the loop — no separate restore step needed.
inline bool spDisplayBlanked(bool editingSp) {
  return !editingSp && getEffectiveSource().isBailout;
}

const GasMix *pickAutoBailout(float depthM); // nullptr if none safe

// Richest enabled gas whose PPO2 falls within [PPO2_HYPOXIA_MIN,
// PPO2_TOX_MAX] at depthM — the "best mix" for bailout gas planning
// (maximizes O2 for decompression efficiency while staying PPO2-safe).
// Deliberately a different policy than pickAutoBailout() (which picks the
// *leanest* safe gas, to minimize O2 exposure in the emergency 3-cell-fail
// scenario it's used for) — nullptr if nothing enabled is safe here.
const GasMix *pickBestBailoutGas(float depthM);

float gasMOD(const GasMix &g);               // meters; -1 if fo2 <= 0

float cellBaseValue(int idx);                // loop PO2 tracks the setpoint
bool computeConsensus(float *outPo2);        // false if no trusted cells
float getActivePPO2AtDepth(float depthM, bool *ok);

int ceilingDepthFor(const float tissuesN2[NUM_COMPARTMENTS], const float tissuesHe[NUM_COMPARTMENTS]);
void stepTissues(float tissuesN2[NUM_COMPARTMENTS], float tissuesHe[NUM_COMPARTMENTS], float pN2, float pHe,
                  float dtMin);
void integrateLiveTissues(float dtMin);

// Forward-projects holding at atDepth (not necessarily the diver's actual
// depth — see the mockup's comment on why stop-remaining always projects
// from the *required* ceiling depth, not wherever the diver actually is).
// Returns -1 if the condition is never met within maxMinutes, or if there's
// no valid PPO2 source to project with.
int forwardProjectNdl(float atDepth, int maxMinutes);                     // until a ceiling first appears
int forwardProjectStopRemain(float atDepth, int targetCeilD, int maxMinutes); // until ceiling <= targetCeilD

int computeTTS(); // -1 if no valid PPO2 source encountered along the way

void recomputeDecoDisplay(); // fills dc::cache from dc::state

} // namespace dc
