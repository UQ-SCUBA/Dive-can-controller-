#include "deco.h"
#include <cmath>

namespace dc {

const float HALFTIMES[NUM_COMPARTMENTS] = {
    5.0f, 8.0f, 12.5f, 18.5f, 27.0f, 38.3f, 54.3f, 77.0f,
    109.0f, 146.0f, 187.0f, 239.0f, 305.0f, 390.0f, 498.0f, 635.0f};
const float A_COEF[NUM_COMPARTMENTS] = {
    1.1696f, 1.0000f, 0.8618f, 0.7562f, 0.6667f, 0.5933f, 0.5282f, 0.4701f,
    0.4187f, 0.3798f, 0.3497f, 0.3223f, 0.2971f, 0.2737f, 0.2523f, 0.2327f};
const float B_COEF[NUM_COMPARTMENTS] = {
    0.5578f, 0.6514f, 0.7222f, 0.7825f, 0.8126f, 0.8434f, 0.8693f, 0.8910f,
    0.9092f, 0.9222f, 0.9319f, 0.9403f, 0.9477f, 0.9544f, 0.9602f, 0.9653f};

// Standard ZHL-16C helium coefficients (Buhlmann/Baker) — He half-times are
// the N2 ones / 2.65 (He's faster diffusion), and a/b follow the same
// empirical fit family as the N2 set above, just parameterized on the He
// half-time instead.
const float HALFTIMES_HE[NUM_COMPARTMENTS] = {
    1.88f, 3.02f, 4.72f, 6.99f, 10.21f, 14.48f, 20.53f, 29.11f,
    41.20f, 55.19f, 70.69f, 90.34f, 115.29f, 147.42f, 188.24f, 240.03f};
const float A_HE_COEF[NUM_COMPARTMENTS] = {
    1.6189f, 1.3830f, 1.1919f, 1.0458f, 0.9220f, 0.8205f, 0.7305f, 0.6502f,
    0.5950f, 0.5545f, 0.5333f, 0.5189f, 0.5181f, 0.5176f, 0.5172f, 0.5119f};
const float B_HE_COEF[NUM_COMPARTMENTS] = {
    0.4770f, 0.5747f, 0.6527f, 0.7223f, 0.7582f, 0.7957f, 0.8279f, 0.8553f,
    0.8757f, 0.8903f, 0.8997f, 0.9073f, 0.9122f, 0.9171f, 0.9217f, 0.9267f};

float ata(float depthM) { return 1.0f + depthM / 10.0f; }

// Shared safe-band selection for pickAutoBailout()/pickBestBailoutGas():
// restricts to enabled candidates within the PPO2 safe band, then — deeper
// than NARCOSIS_HE_PRIORITY_DEPTH_M — restricts further to helium-carrying
// candidates if at least one safe one exists (helium isn't narcotic,
// nitrogen is, so a safe trimix/heliox option there beats a richer but
// nitrogen-only one). Shallower than that, or with no safe helium option,
// the helium fraction doesn't affect the choice. Within whatever set
// remains, picks leanest (preferRichest=false, pickAutoBailout's policy —
// minimize O2 exposure for the unattended emergency case) or richest
// (preferRichest=true, pickBestBailoutGas's policy — decompression
// efficiency). nullptr if nothing enabled is in the safe band at all.
static const GasMix *selectSafeBailoutCandidate(float depthM, bool preferRichest) {
  float a = ata(depthM);
  bool preferHelium = false;
  if (depthM > NARCOSIS_HE_PRIORITY_DEPTH_M) {
    for (int i = 0; i < NUM_BAILOUT_CANDIDATES; i++) {
      const GasMix &g = gasCandidate(i);
      if (!g.enabled || g.fhe <= 0.0f) continue;
      float p = g.fo2 * a;
      if (p >= PPO2_HYPOXIA_MIN && p <= PPO2_TOX_MAX) {
        preferHelium = true;
        break;
      }
    }
  }

  const GasMix *picked = nullptr;
  for (int i = 0; i < NUM_BAILOUT_CANDIDATES; i++) {
    const GasMix &g = gasCandidate(i);
    if (!g.enabled) continue;
    float p = g.fo2 * a;
    if (p < PPO2_HYPOXIA_MIN || p > PPO2_TOX_MAX) continue;
    if (preferHelium && g.fhe <= 0.0f) continue;
    if (picked == nullptr || (preferRichest ? g.fo2 > picked->fo2 : g.fo2 < picked->fo2)) picked = &g;
  }
  return picked;
}

const GasMix *pickAutoBailout(float depthM) {
  const GasMix *leanest = selectSafeBailoutCandidate(depthM, /*preferRichest=*/false);
  if (leanest) return leanest;

  // Nothing enabled falls in the safe band (narcosis preference doesn't
  // apply here — with no safe option at all, the priority is just getting
  // PPO2 as close to survivable as possible) — still hand back the gas
  // whose PPO2 misses by the least (e.g. all options are too rich for this
  // depth: pick the leanest of them) rather than nullptr, so the diver
  // still has a tracked PPO2/tissue loading rather than a dead display.
  // The PPO2 readout's existing HIGH/LOW PO2 alert already warns them it's
  // not actually safe.
  float a = ata(depthM);
  const GasMix *closest = nullptr;
  float closestMiss = 0.0f;
  for (int i = 0; i < NUM_BAILOUT_CANDIDATES; i++) {
    const GasMix &g = gasCandidate(i);
    if (!g.enabled) continue;
    float p = g.fo2 * a;
    float miss = p < PPO2_HYPOXIA_MIN ? (PPO2_HYPOXIA_MIN - p) : (p - PPO2_TOX_MAX);
    if (closest == nullptr || miss < closestMiss) {
      closest = &g;
      closestMiss = miss;
    }
  }
  return closest;
}

const GasMix *pickBestBailoutGas(float depthM) {
  return selectSafeBailoutCandidate(depthM, /*preferRichest=*/true);
}

float gasMOD(const GasMix &g) {
  if (g.fo2 <= 0.0f) return -1.0f;
  return floorf(((MOD_CEILING / g.fo2) - 1.0f) * 10.0f);
}

// Loop PO2 tracks the active setpoint (solenoid holds the loop there).
float cellBaseValue(int /*idx*/) {
  float target = spValue(state.spMode);
  return target < 0.0f ? 0.0f : target;
}

bool computeConsensus(float *outPo2) {
  float sum = 0.0f;
  int n = 0;
  for (int i = 0; i < NUM_CELLS; i++) {
    if (state.cells[i] == CellState::Ok) {
      sum += cellBaseValue(i);
      n++;
    }
  }
  if (n == 0) return false;
  *outPo2 = sum / n;
  return true;
}

EffectiveSource getEffectiveSource() {
  bool allFail = true;
  for (int i = 0; i < NUM_CELLS; i++)
    if (state.cells[i] != CellState::Fail) allFail = false;

  if (allFail && state.source == Source::Loop) {
    return {true, true, pickAutoBailout(state.depth)};
  }
  if (state.source == Source::Bailout) {
    // The active bailout gas can be disabled from the gas-edit page without
    // state.bailoutGasIdx itself changing — nothing re-picks a replacement
    // automatically (that would silently start computing PPO2 for a
    // different gas than the diver last confirmed, which is worse). Treat
    // it the same as "no safe gas": every eff.gas call site already handles
    // nullptr (PO2 shows "--"/NO SAFE BAILOUT GAS, tissue integration
    // pauses rather than using a stale FO2).
    const GasMix &g = gasCandidate(state.bailoutGasIdx);
    return {true, false, g.enabled ? &g : nullptr};
  }
  return {false, false, nullptr};
}

float getActivePPO2AtDepth(float depthM, bool *ok) {
  EffectiveSource eff = getEffectiveSource();
  if (eff.isBailout) {
    if (eff.gas == nullptr) {
      *ok = false;
      return 0.0f;
    }
    *ok = true;
    return eff.gas->fo2 * ata(depthM);
  }
  float po2;
  *ok = computeConsensus(&po2);
  return *ok ? po2 : 0.0f;
}

// Resolves the FO2/FHe of whatever's actually driving inert-gas loading
// right now — the bailout gas if bailed out, state.diluent while on the
// loop (the bailout slots are only breathed once the diver actually bails
// to open-circuit) — plus the PPO2 at depthM. False if there's no valid
// source (mirrors getActivePPO2AtDepth's nullptr/no-consensus handling).
static bool activeInertSource(float depthM, float *outFo2, float *outFhe, float *outPpo2) {
  EffectiveSource eff = getEffectiveSource();
  if (eff.isBailout) {
    if (eff.gas == nullptr) return false;
    *outFo2 = eff.gas->fo2;
    *outFhe = eff.gas->fhe;
    *outPpo2 = eff.gas->fo2 * ata(depthM);
    return true;
  }
  if (!computeConsensus(outPpo2)) return false;
  *outFo2 = state.diluent.fo2;
  *outFhe = state.diluent.fhe;
  return true;
}

// Splits a total inspired inert-gas pressure between N2 and He per the
// breathing gas's dry-fraction ratio (FN2:FHe, where FN2 = 1 - fo2 - fhe).
static void splitInert(float fo2, float fhe, float pInertTotal, float *outPN2, float *outPHe) {
  float dryInert = 1.0f - fo2; // == fn2 + fhe
  if (dryInert <= 0.0f) {
    *outPN2 = 0.0f;
    *outPHe = 0.0f;
    return;
  }
  float pHe = pInertTotal * (fhe / dryInert);
  if (pHe < 0.0f) pHe = 0.0f;
  if (pHe > pInertTotal) pHe = pInertTotal;
  *outPHe = pHe;
  *outPN2 = pInertTotal - pHe;
}

int ceilingDepthFor(const float tissuesN2[NUM_COMPARTMENTS], const float tissuesHe[NUM_COMPARTMENTS]) {
  float maxCeilPamb = 0.0f;
  for (int i = 0; i < NUM_COMPARTMENTS; i++) {
    float pn2 = tissuesN2[i];
    float phe = tissuesHe[i];
    float pTotal = pn2 + phe;
    if (pTotal <= 0.0f) continue;
    // Combined M-value line: weight each gas's a/b by its share of this
    // compartment's total inert loading — the standard Buhlmann trimix
    // combination (see Erik Baker's "Understanding M-values").
    float a = (A_COEF[i] * pn2 + A_HE_COEF[i] * phe) / pTotal;
    float b = (B_COEF[i] * pn2 + B_HE_COEF[i] * phe) / pTotal;
    float c = (pTotal - a) * b;
    if (c > maxCeilPamb) maxCeilPamb = c;
  }
  float d = (maxCeilPamb - 1.0f) * 10.0f;
  if (d <= 0.0f) return 0;
  int ceil = (int)(ceilf(d / STOP_INCREMENT) * STOP_INCREMENT);
  if (ceil < state.lastStopDepth) ceil = state.lastStopDepth;
  return ceil;
}

void stepTissues(float tissuesN2[NUM_COMPARTMENTS], float tissuesHe[NUM_COMPARTMENTS], float pN2, float pHe,
                  float dtMin) {
  for (int i = 0; i < NUM_COMPARTMENTS; i++) {
    float kN2 = 0.693147f / HALFTIMES[i]; // ln(2)
    tissuesN2[i] += (pN2 - tissuesN2[i]) * (1.0f - expf(-kN2 * dtMin));
    float kHe = 0.693147f / HALFTIMES_HE[i];
    tissuesHe[i] += (pHe - tissuesHe[i]) * (1.0f - expf(-kHe * dtMin));
  }
}

void integrateLiveTissues(float dtMin) {
  if (dtMin <= 0.0f) return;
  float fo2, fhe, ppo2;
  if (!activeInertSource(state.depth, &fo2, &fhe, &ppo2)) return; // no valid source — don't corrupt tissue state on a gap
  float pInertTotal = ata(state.depth) - PH2O - ppo2;
  if (pInertTotal < 0.0f) pInertTotal = 0.0f;
  float pN2, pHe;
  splitInert(fo2, fhe, pInertTotal, &pN2, &pHe);
  stepTissues(state.tissuesN2, state.tissuesHe, pN2, pHe, dtMin);
  state.simMinutes += dtMin;
}

// Shared setup for the two forward-projection functions below: snapshots
// the current tissue state and computes the inert gas partial pressures at
// atDepth. Returns false (nothing to project) if there's no valid PPO2
// source there.
static bool beginProjection(float atDepth, float tissuesN2[NUM_COMPARTMENTS], float tissuesHe[NUM_COMPARTMENTS],
                             float *outPN2, float *outPHe) {
  float fo2, fhe, ppo2;
  if (!activeInertSource(atDepth, &fo2, &fhe, &ppo2)) return false;
  float pInertTotal = ata(atDepth) - PH2O - ppo2;
  if (pInertTotal < 0.0f) pInertTotal = 0.0f;
  splitInert(fo2, fhe, pInertTotal, outPN2, outPHe);
  for (int i = 0; i < NUM_COMPARTMENTS; i++) {
    tissuesN2[i] = state.tissuesN2[i];
    tissuesHe[i] = state.tissuesHe[i];
  }
  return true;
}

int forwardProjectNdl(float atDepth, int maxMinutes) {
  float tissuesN2[NUM_COMPARTMENTS], tissuesHe[NUM_COMPARTMENTS], pN2, pHe;
  if (!beginProjection(atDepth, tissuesN2, tissuesHe, &pN2, &pHe)) return -1;
  for (int t = 1; t <= maxMinutes; t++) {
    stepTissues(tissuesN2, tissuesHe, pN2, pHe, 1.0f);
    if (ceilingDepthFor(tissuesN2, tissuesHe) > 0) return t;
  }
  return -1;
}

int forwardProjectStopRemain(float atDepth, int targetCeilD, int maxMinutes) {
  float tissuesN2[NUM_COMPARTMENTS], tissuesHe[NUM_COMPARTMENTS], pN2, pHe;
  if (!beginProjection(atDepth, tissuesN2, tissuesHe, &pN2, &pHe)) return -1;
  for (int t = 1; t <= maxMinutes; t++) {
    stepTissues(tissuesN2, tissuesHe, pN2, pHe, 1.0f);
    if (ceilingDepthFor(tissuesN2, tissuesHe) <= targetCeilD) return t;
  }
  return -1;
}

// Total time to surface: assumes the diver complies from here — ascends to
// the current ceiling (or straight up if already shallower), then clears
// each stop in turn up to the surface, at ASCENT_RATE between stops.
int computeTTS() {
  float tissuesN2[NUM_COMPARTMENTS], tissuesHe[NUM_COMPARTMENTS];
  for (int i = 0; i < NUM_COMPARTMENTS; i++) {
    tissuesN2[i] = state.tissuesN2[i];
    tissuesHe[i] = state.tissuesHe[i];
  }

  float totalMin = 0.0f;
  int ceil = ceilingDepthFor(tissuesN2, tissuesHe);
  if (state.depth > ceil) totalMin += (state.depth - ceil) / ASCENT_RATE;

  while (ceil > 0) {
    // Once at the configured last stop, ceilingDepthFor() only ever reports
    // that depth or 0 (fully clear) — never anything in between — so the
    // next target is 0, not a further STOP_INCREMENT step down.
    int target = ceil > state.lastStopDepth ? ceil - STOP_INCREMENT : 0;
    float fo2, fhe, ppo2;
    if (!activeInertSource((float)ceil, &fo2, &fhe, &ppo2)) return -1;
    float pInertTotal = ata((float)ceil) - PH2O - ppo2;
    if (pInertTotal < 0.0f) pInertTotal = 0.0f;
    float pN2, pHe;
    splitInert(fo2, fhe, pInertTotal, &pN2, &pHe);
    int waited = 0;
    while (ceilingDepthFor(tissuesN2, tissuesHe) > target && waited < 240) {
      stepTissues(tissuesN2, tissuesHe, pN2, pHe, 1.0f);
      waited++;
    }
    // Didn't actually clear within the per-stop cap — same "give up rather
    // than report a fabricated number" convention as forwardProjectStopRemain
    // (see UI's "240+" fallback for -1). Without this, a stop that never
    // clears (e.g. too lean a gas to off-gas at that depth) would silently
    // report a total that's a lower bound, not a true TTS.
    if (ceilingDepthFor(tissuesN2, tissuesHe) > target) return -1;
    totalMin += waited + (ceil - target) / ASCENT_RATE;
    ceil = target;
  }
  return (int)ceilf(totalMin);
}

void recomputeDecoDisplay() {
  int ceilD = ceilingDepthFor(state.tissuesN2, state.tissuesHe);
  cache.ceilD = ceilD;
  if (ceilD == 0) {
    cache.ndl = forwardProjectNdl(state.depth, 240);
    cache.stopRemain = -1;
    cache.tts = (int)ceilf(state.depth / ASCENT_RATE);
  } else {
    cache.ndl = -1;
    cache.stopRemain = forwardProjectStopRemain((float)ceilD, ceilD - STOP_INCREMENT, 240);
    cache.tts = computeTTS();
  }
}

} // namespace dc
