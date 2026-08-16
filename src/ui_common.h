#pragma once
#include "lvgl.h"
#include "state.h"
#include "deco.h"
#include <cstdio>

// Small helpers shared by every ui_*.cpp screen/overlay module — the
// palette mirrors the mockup's COL object, and mkLabel() is the
// create+position+style boilerplate each screen was repeating verbatim.

namespace dc {

inline lv_color_t colBg() { return lv_color_hex(0x04100c); }
inline lv_color_t colGreen() { return lv_color_hex(0x35e07a); }
inline lv_color_t colGreenDim() { return lv_color_hex(0x1f8a52); }
inline lv_color_t colYellow() { return lv_color_hex(0xffd23f); }
inline lv_color_t colRed() { return lv_color_hex(0xff3b30); }
inline lv_color_t colInk() { return lv_color_hex(0xbfe6d6); }
inline lv_color_t colInkDim() { return lv_color_hex(0x3f6b5c); }
inline lv_color_t colCyan() { return lv_color_hex(0x3fd6c2); }
inline lv_color_t colWhite() { return lv_color_hex(0xffffff); }

inline lv_obj_t *mkLabel(lv_obj_t *parent, int x, int y, const lv_font_t *font, lv_color_t color) {
  lv_obj_t *l = lv_label_create(parent);
  lv_obj_set_pos(l, x, y);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, color, 0);
  return l;
}

// Shared by every dive-screen layout (rect and round) so the two don't drift
// on what's ultimately the same state->text/color mapping logic.

inline void formatCellValue(int idx, char *buf, size_t n, lv_color_t *outColor) {
  CellState s = state.cells[idx];
  if (s == CellState::Fail) {
    snprintf(buf, n, "FAIL");
    *outColor = colRed();
  } else if (s == CellState::Voted) {
    snprintf(buf, n, "0.52");
    *outColor = colYellow();
  } else {
    snprintf(buf, n, "%.2f", cellBaseValue(idx));
    *outColor = colGreen();
  }
}

inline const char *spModeName(SpMode m) {
  switch (m) {
    case SpMode::Sp05: return "0.5";
    case SpMode::Sp07: return "0.7";
    case SpMode::Sp10: return "1.0";
    case SpMode::Sp12: return "1.2";
  }
  return "0.7";
}

// Formats a bailout candidate as its FO2/FHe composition (e.g. "18/45",
// "50/0") rather than its slot label/number — the composition is what
// actually matters for a bailout decision, not which numbered slot it's
// in. The diluent still gets its name prefixed since, unlike the numbered
// mixes, that name itself carries information the composition doesn't.
inline void formatGasCandidate(const GasMix &g, char *buf, size_t n) {
  int fo2Pct = (int)(g.fo2 * 100.0f + 0.5f);
  int fhePct = (int)(g.fhe * 100.0f + 0.5f);
  if (gasCandidateIndex(&g) < NUM_GAS_MIXES) snprintf(buf, n, "%d/%d", fo2Pct, fhePct);
  else snprintf(buf, n, "DIL %d/%d", fo2Pct, fhePct);
}

} // namespace dc
