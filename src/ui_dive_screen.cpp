#include "ui_dive_screen.h"
#include "state.h"
#include "deco.h"
#include "ui_common.h"
#include <cstdio>

// LVGL widget port of the mockup's drawDiveScreen(). Layout mirrors the
// mockup's 320x240 field positions approximately (top-left label coords
// rather than canvas baseline coords) — close but not pixel-identical; fine
// polish is a follow-up once this is running on real hardware and can be
// eyeballed directly. The highlight box during S.P./gas-select edit is a
// static border here rather than the mockup's pulsing animation, to keep
// this first pass simple.

namespace dc {

// ---- widget refs ----
static lv_obj_t *poPO2Label;
static lv_obj_t *poPO2Value;
static lv_obj_t *poCellLabel[NUM_CELLS];
static lv_obj_t *poCellValue[NUM_CELLS];
static lv_obj_t *poSrcValue;
static lv_obj_t *poSpLabel;
static lv_obj_t *poSpValue;
static lv_obj_t *poWetLabel;
static lv_obj_t *poDepthValue;
static lv_obj_t *poDecoLabel;
static lv_obj_t *poDecoValue;
static lv_obj_t *poTtsValue;
static lv_obj_t *poDiveTimeValue;
static lv_obj_t *poMaxDepthValue;
static lv_obj_t *poHighlightBox; // shown over SRC or S.P. block while editing
static lv_obj_t *poBanner;
static lv_obj_t *poBannerLabel;

void uiDiveScreenCreate(lv_obj_t *parent) {
  lv_obj_set_style_bg_color(parent, colBg(), 0);
  lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(parent, 0, 0);
  lv_obj_set_style_pad_all(parent, 0, 0);
  lv_obj_set_style_radius(parent, 0, 0);

  // ---- left block: PO2 / cells / SRC / S.P. ----
  poPO2Label = mkLabel(parent, 8, 4, &lv_font_montserrat_12, colInkDim());
  lv_label_set_text(poPO2Label, "PO2");
  poPO2Value = mkLabel(parent, 6, 14, &lv_font_montserrat_46, colGreen());
  lv_label_set_text(poPO2Value, "0.70");

  static const char *cellNames[NUM_CELLS] = {"C1", "C2", "C3"};
  for (int i = 0; i < NUM_CELLS; i++) {
    int x = 8 + i * 44;
    poCellLabel[i] = mkLabel(parent, x, 70, &lv_font_montserrat_12, colInkDim());
    lv_label_set_text(poCellLabel[i], cellNames[i]);
    poCellValue[i] = mkLabel(parent, x, 82, &lv_font_montserrat_16, colGreen());
    lv_label_set_text(poCellValue[i], "0.70");
  }

  lv_obj_t *srcLabel = mkLabel(parent, 8, 108, &lv_font_montserrat_12, colInkDim());
  lv_label_set_text(srcLabel, "SRC");
  poSrcValue = mkLabel(parent, 8, 120, &lv_font_montserrat_16, colCyan());
  lv_label_set_text(poSrcValue, "CC");

  poSpLabel = mkLabel(parent, 8, 148, &lv_font_montserrat_12, colInkDim());
  lv_label_set_text(poSpLabel, "S.P. 0.7");
  poSpValue = mkLabel(parent, 8, 160, &lv_font_montserrat_20, colGreen());
  lv_label_set_text(poSpValue, "0.7");

  poWetLabel = mkLabel(parent, 8, 196, &lv_font_montserrat_12, colCyan());
  lv_label_set_text(poWetLabel, LV_SYMBOL_TINT " WET");
  lv_obj_add_flag(poWetLabel, LV_OBJ_FLAG_HIDDEN);

  // highlight box, reused for both S.P. and SRC edit states
  poHighlightBox = lv_obj_create(parent);
  lv_obj_remove_style_all(poHighlightBox);
  lv_obj_set_style_bg_opa(poHighlightBox, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_color(poHighlightBox, colCyan(), 0);
  lv_obj_set_style_border_width(poHighlightBox, 2, 0);
  lv_obj_set_style_radius(poHighlightBox, 4, 0);
  lv_obj_add_flag(poHighlightBox, LV_OBJ_FLAG_HIDDEN);

  // vertical divider
  lv_obj_t *divider = lv_obj_create(parent);
  lv_obj_remove_style_all(divider);
  lv_obj_set_pos(divider, 148, 10);
  lv_obj_set_size(divider, 1, 190);
  lv_obj_set_style_bg_color(divider, colInkDim(), 0);
  lv_obj_set_style_bg_opa(divider, LV_OPA_30, 0);

  // ---- right block: depth / deco / TTS / dive time ----
  lv_obj_t *depthLabel = mkLabel(parent, 158, 4, &lv_font_montserrat_12, colInkDim());
  lv_label_set_text(depthLabel, "DEPTH");
  poDepthValue = mkLabel(parent, 156, 14, &lv_font_montserrat_40, colInk());
  lv_label_set_text(poDepthValue, "0.0m");

  poDecoLabel = mkLabel(parent, 158, 70, &lv_font_montserrat_12, colInkDim());
  lv_label_set_text(poDecoLabel, "NDL");
  poDecoValue = mkLabel(parent, 156, 82, &lv_font_montserrat_28, colGreen());
  lv_label_set_text(poDecoValue, "0 min");

  lv_obj_t *ttsLabel = mkLabel(parent, 158, 122, &lv_font_montserrat_12, colInkDim());
  lv_label_set_text(ttsLabel, "TTS");
  poTtsValue = mkLabel(parent, 156, 134, &lv_font_montserrat_16, colInk());
  lv_label_set_text(poTtsValue, "--");

  lv_obj_t *diveTimeLabel = mkLabel(parent, 158, 154, &lv_font_montserrat_12, colInkDim());
  lv_label_set_text(diveTimeLabel, "DIVE TIME");
  poDiveTimeValue = mkLabel(parent, 156, 166, &lv_font_montserrat_16, colInk());
  lv_label_set_text(poDiveTimeValue, "0 min");

  poMaxDepthValue = mkLabel(parent, 156, 190, &lv_font_montserrat_12, colInkDim());
  lv_label_set_text(poMaxDepthValue, "MAX 0.0m");

  // bottom alert / hint banner
  poBanner = lv_obj_create(parent);
  lv_obj_remove_style_all(poBanner);
  lv_obj_set_pos(poBanner, 6, 210);
  lv_obj_set_size(poBanner, 308, 24);
  lv_obj_set_style_radius(poBanner, 3, 0);
  lv_obj_set_style_bg_opa(poBanner, LV_OPA_COVER, 0);
  lv_obj_add_flag(poBanner, LV_OBJ_FLAG_HIDDEN);
  poBannerLabel = lv_label_create(poBanner);
  lv_obj_center(poBannerLabel);
  lv_obj_set_style_text_font(poBannerLabel, &lv_font_montserrat_12, 0);
}

void uiDiveScreenUpdate() {
  EffectiveSource eff = getEffectiveSource();

  const char *autoBanner = nullptr;
  static char autoBannerBuf[40];
  if (eff.isAuto) {
    if (eff.gas) {
      char gasBuf[24];
      formatGasCandidate(*eff.gas, gasBuf, sizeof(gasBuf));
      snprintf(autoBannerBuf, sizeof(autoBannerBuf), "AUTO-BAILOUT -> %s", gasBuf);
    } else {
      snprintf(autoBannerBuf, sizeof(autoBannerBuf), "AUTO-BAILOUT: NO SAFE GAS");
    }
    autoBanner = autoBannerBuf;
  }

  // ---- PO2 block ----
  char po2Text[16];
  lv_color_t po2Color;
  const char *ppo2AlertText = nullptr;
  bool ppo2AlertIsCaution = false; // yellow "heads up", not a red hazard
  if (eff.isBailout) {
    if (eff.gas) {
      float p = eff.gas->fo2 * ata(state.depth);
      snprintf(po2Text, sizeof(po2Text), "%.2f", p);
      po2Color = (p < PPO2_HYPOXIA_MIN || p > PPO2_TOX_MAX) ? colRed() : colGreen();
      if (p > PPO2_TOX_MAX) {
        ppo2AlertText = "HIGH PO2 - O2 TOX RISK";
      } else if (p < PPO2_HYPOXIA_MIN) {
        ppo2AlertText = "LOW PO2 - HYPOXIA RISK";
      } else if (!eff.isAuto) {
        // Safe, but is it what pickBestBailoutGas() would actually pick
        // (richest safe option, with a safe helium mix taking precedence
        // below NARCOSIS_HE_PRIORITY_DEPTH_M)? Once actually in bailout
        // the gas choice is the diver's call — this is a nudge, not a
        // correction (and doesn't apply to the 3-cell-fail auto pick,
        // which deliberately chooses the *leanest* safe gas).
        const GasMix *best = pickBestBailoutGas(state.depth);
        if (best && best != eff.gas) {
          ppo2AlertText = "NOT BEST MIX";
          ppo2AlertIsCaution = true;
        }
      }
      lv_label_set_text(poPO2Label, "PO2 (calc)");
    } else {
      snprintf(po2Text, sizeof(po2Text), "--");
      po2Color = colRed();
      ppo2AlertText = "NO SAFE BAILOUT GAS";
      lv_label_set_text(poPO2Label, "PO2 (calc)");
    }
  } else {
    // getEffectiveSource() forces eff.isBailout (auto-bailout) the moment
    // every cell has failed and source==Loop, so reaching here already
    // guarantees at least one trusted cell for computeConsensus() to use.
    float consensus = 0;
    computeConsensus(&consensus);
    snprintf(po2Text, sizeof(po2Text), "%.2f", consensus);
    bool low = consensus < 0.5f;
    po2Color = low ? colRed() : colGreen();
    if (low) {
      ppo2AlertText = "LOW PO2 - CHECK LOOP";
    } else if (!pickBestBailoutGas(state.depth)) {
      // On the loop, state.bailoutGasIdx is just a standby hint, not a
      // live selection — it auto-tracks the best enabled gas (see
      // setSource()), so the only thing worth flagging here is having no
      // safe option *at all* among the enabled gases.
      ppo2AlertText = "NO SAFE BAILOUT GAS";
    }
    lv_label_set_text(poPO2Label, "PO2");
  }
  lv_label_set_text(poPO2Value, po2Text);
  lv_obj_set_style_text_color(poPO2Value, po2Color, 0);

  // ---- cells ----
  for (int i = 0; i < NUM_CELLS; i++) {
    char buf[8];
    lv_color_t c;
    formatCellValue(i, buf, sizeof(buf), &c);
    lv_label_set_text(poCellValue[i], buf);
    lv_obj_set_style_text_color(poCellValue[i], c, 0);
  }

  // ---- SRC / S.P., with edit highlight ----
  bool editingSp = state.uiMode == UiMode::SpEdit;
  bool editingGas = state.uiMode == UiMode::GasSelect;

  if (editingGas) {
    // Selecting doesn't block a risky gas (the diver may have their own
    // reasons), but colors it live so the choice is informed before they
    // commit: green is the richest safe option (what pickBestBailoutGas()
    // would auto-pick going into bailout), yellow is safe but not that,
    // red is outside the PPO2 band entirely. Shown as FO2/FHe (e.g.
    // "18/45") rather than the slot name/number — the composition is what
    // actually matters for the choice, not which numbered slot it's in.
    const GasMix &candidate = gasCandidate(state.gasCursor);
    float candP = candidate.fo2 * ata(state.depth);
    char gasBuf[24];
    formatGasCandidate(candidate, gasBuf, sizeof(gasBuf));
    lv_label_set_text_fmt(poSrcValue, "-> %s", gasBuf);
    bool candUnsafe = candP < PPO2_HYPOXIA_MIN || candP > PPO2_TOX_MAX;
    bool candBest = !candUnsafe && pickBestBailoutGas(state.depth) == &candidate;
    lv_obj_set_style_text_color(poSrcValue, candUnsafe ? colRed() : (candBest ? colGreen() : colYellow()), 0);
  } else if (eff.isBailout) {
    if (eff.gas) {
      char gasBuf[24];
      formatGasCandidate(*eff.gas, gasBuf, sizeof(gasBuf));
      lv_label_set_text_fmt(poSrcValue, "OC . %s", gasBuf);
    } else {
      lv_label_set_text(poSrcValue, "OC . --");
    }
    lv_obj_set_style_text_color(poSrcValue, colYellow(), 0);
  } else {
    lv_label_set_text(poSrcValue, "CC");
    lv_obj_set_style_text_color(poSrcValue, colCyan(), 0);
  }

  if (spDisplayBlanked(editingSp)) {
    // On bailout the loop's solenoid isn't driving to a target — S.P. isn't
    // active, so show it blank rather than the last CC value (which is
    // preserved untouched in state.spMode and comes back once back on the
    // loop; see pressMenu()'s bailout branch, which already keeps SpEdit
    // unreachable from here).
    lv_label_set_text(poSpLabel, "S.P.");
    lv_label_set_text(poSpValue, "--");
    lv_obj_set_style_text_color(poSpValue, colInkDim(), 0);
  } else {
    SpMode spShown = editingSp ? state.spCursor : state.spMode;
    lv_label_set_text_fmt(poSpLabel, "S.P. %s", spModeName(spShown));
    // lv_label_set_text_fmt's builtin sprintf has no %f support unless
    // LV_USE_FLOAT is on (it's off by default) — use libc snprintf instead.
    char buf[8];
    snprintf(buf, sizeof(buf), "%.1f", spValue(spShown));
    lv_label_set_text(poSpValue, buf);
    lv_obj_set_style_text_color(poSpValue, editingSp ? colCyan() : colGreen(), 0);
  }

  if (editingSp) {
    lv_obj_set_pos(poHighlightBox, 6, 144);
    lv_obj_set_size(poHighlightBox, 100, 44);
    lv_obj_remove_flag(poHighlightBox, LV_OBJ_FLAG_HIDDEN);
  } else if (editingGas) {
    lv_obj_set_pos(poHighlightBox, 6, 106);
    lv_obj_set_size(poHighlightBox, 130, 32);
    lv_obj_remove_flag(poHighlightBox, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(poHighlightBox, LV_OBJ_FLAG_HIDDEN);
  }

  if (state.wetSensor) lv_obj_remove_flag(poWetLabel, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(poWetLabel, LV_OBJ_FLAG_HIDDEN);

  // ---- right block ----
  {
    char buf[16];
    snprintf(buf, sizeof(buf), "%.1fm", state.depth);
    lv_label_set_text(poDepthValue, buf);
  }

  int ceilD = cache.ceilD;
  bool belowCeiling = ceilD > 0 && state.depth < ceilD;
  char decoVal[16];
  lv_color_t decoColor;
  if (ceilD == 0) {
    lv_label_set_text(poDecoLabel, "NDL");
    if (cache.ndl >= 0) snprintf(decoVal, sizeof(decoVal), "%d min", cache.ndl);
    else snprintf(decoVal, sizeof(decoVal), "240+");
    decoColor = colGreen();
  } else {
    lv_label_set_text_fmt(poDecoLabel, "STOP %dm", ceilD);
    if (cache.stopRemain >= 0) snprintf(decoVal, sizeof(decoVal), "%d min", cache.stopRemain);
    else snprintf(decoVal, sizeof(decoVal), "240+");
    decoColor = belowCeiling ? colRed() : colYellow();
  }
  lv_label_set_text(poDecoValue, decoVal);
  lv_obj_set_style_text_color(poDecoValue, decoColor, 0);

  if (cache.tts >= 0) lv_label_set_text_fmt(poTtsValue, "%d min", cache.tts);
  else lv_label_set_text(poTtsValue, "--");

  lv_label_set_text_fmt(poDiveTimeValue, "%d min", (int)state.diveTime);
  {
    // Same libc-vs-lv_label_set_text_fmt %f caveat as the S.P. value above.
    char buf[16];
    snprintf(buf, sizeof(buf), "MAX %.1fm", state.maxDepth);
    lv_label_set_text(poMaxDepthValue, buf);
  }

  // ---- bottom banner: auto-bailout > ceiling violation > PO2 alert > edit hint ----
  char ceilingMsg[40];
  const char *alertText = nullptr;
  bool alertIsCaution = false;
  if (autoBanner) {
    alertText = autoBanner;
  } else if (belowCeiling) {
    snprintf(ceilingMsg, sizeof(ceilingMsg), "BELOW CEILING - %dm REQUIRED", ceilD);
    alertText = ceilingMsg;
  } else if (ppo2AlertText) {
    alertText = ppo2AlertText;
    alertIsCaution = ppo2AlertIsCaution;
  }

  if (alertText) {
    lv_obj_set_style_bg_color(poBanner, alertIsCaution ? colYellow() : colRed(), 0);
    lv_obj_set_style_bg_opa(poBanner, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(poBannerLabel, lv_color_hex(0x170403), 0);
    lv_label_set_text(poBannerLabel, alertText);
    lv_obj_remove_flag(poBanner, LV_OBJ_FLAG_HIDDEN);
  } else if (editingSp || editingGas) {
    lv_obj_set_style_bg_color(poBanner, colCyan(), 0);
    lv_obj_set_style_bg_opa(poBanner, LV_OPA_20, 0);
    lv_obj_set_style_text_color(poBannerLabel, colCyan(), 0);
    lv_label_set_text(poBannerLabel, editingGas ? "MENU -> next gas   ACTION confirm" : "MENU -> next   ACTION confirm");
    lv_obj_remove_flag(poBanner, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(poBanner, LV_OBJ_FLAG_HIDDEN);
  }
}

} // namespace dc
