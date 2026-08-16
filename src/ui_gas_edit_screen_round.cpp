#include "ui_gas_edit_screen.h"
#include "state.h"
#include "ui_common.h"
#include "device_profile.h"
#include <cstdio>

// Round counterpart to ui_gas_edit_screen.cpp -- same fields/logic, rows
// recentered and tightened so the widest rows (name/FO2/He/ON-OFF, ~280px)
// stay inside the round safe chord; short header/footer lines get to sit
// closer to the narrower chord near the top/bottom poles. Rough first pass,
// same spirit as ui_dive_screen_round.cpp's own header comment.

namespace dc {

static const int ROW_Y[NUM_GAS_MIXES] = {70, 108, 146};

static lv_obj_t *root;
static lv_obj_t *slotLabel[NUM_GAS_MIXES];
static lv_obj_t *nameLabel[NUM_GAS_MIXES];
static lv_obj_t *fo2Label[NUM_GAS_MIXES];
static lv_obj_t *fheLabel[NUM_GAS_MIXES];
static lv_obj_t *enabledLabel[NUM_GAS_MIXES];
static lv_obj_t *fieldHighlight;
static lv_obj_t *lastStopValue;

constexpr int DILUENT_ROW_Y = 224;
static lv_obj_t *diluentNameLabel;
static lv_obj_t *diluentFo2Label;
static lv_obj_t *diluentFheLabel;
static lv_obj_t *diluentTagLabel;

constexpr int LAST_STOP_ROW_Y = 186;

// Column x positions -- narrower than the rect screen's (8/26/130/204/276)
// since the round safe chord at the MIX rows' height (y=70, the tightest of
// the three) is only ~285px, not ~320px. Gaps sized from actual glyph
// widths at the font sizes used below (montserrat_18's adv_w table), not
// eyeballed -- the previous pass eyeballed a 60px FO2/He gap that "FO2 100%"
// (~88px wide at this font) blew straight through:
//   "MIX 2"/"MIX 3" ~50px, "FO2 100%" ~88px, "He 100%" ~76px, "OFF" ~38px
// Rightmost edge (COL_EN_X + "OFF") lands at ~322, right at that ~285px
// budget's outer edge (40 + ~282) -- tight but in bounds.
constexpr int COL_SLOT_X = 40;
constexpr int COL_NAME_X = 59;
constexpr int COL_FO2_X = 114;
constexpr int COL_FHE_X = 207;
constexpr int COL_EN_X = 288;
// "LAST STOP" (~100px at montserrat_18) doesn't fit in the COL_NAME_X ->
// COL_FO2_X gap sized for "MIX 1" -- its value gets its own column, clear of
// the label, rather than forcing every other column wider to make room for
// a label only this one row uses.
constexpr int LAST_STOP_VALUE_X = 150;

void uiGasEditScreenCreate(lv_obj_t *parent) {
  root = lv_obj_create(parent);
  lv_obj_remove_style_all(root);
  lv_obj_set_pos(root, 0, 0);
  lv_obj_set_size(root, DEVICE_W, DEVICE_H);
  lv_obj_set_style_bg_color(root, colBg(), 0);
  lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
  lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(root, LV_OBJ_FLAG_HIDDEN);

  lv_obj_t *title = lv_label_create(root);
  lv_obj_set_width(title, DEVICE_W);
  lv_obj_set_pos(title, 0, 14);
  lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(title, colCyan(), 0);
  lv_label_set_text(title, "GAS EDIT");

  lv_obj_t *hint = lv_label_create(root);
  lv_obj_set_width(hint, DEVICE_W);
  lv_obj_set_pos(hint, 0, 34);
  lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(hint, colInkDim(), 0);
  lv_label_set_text(hint, "hold MENU to exit");

  for (int i = 0; i < NUM_GAS_MIXES; i++) {
    int y = ROW_Y[i];
    slotLabel[i] = mkLabel(root, COL_SLOT_X, y, &lv_font_montserrat_18, colInkDim());
    nameLabel[i] = mkLabel(root, COL_NAME_X, y, &lv_font_montserrat_18, colInk());
    fo2Label[i] = mkLabel(root, COL_FO2_X, y, &lv_font_montserrat_18, colInk());
    fheLabel[i] = mkLabel(root, COL_FHE_X, y, &lv_font_montserrat_18, colInk());
    enabledLabel[i] = mkLabel(root, COL_EN_X, y, &lv_font_montserrat_16, colGreen());
  }

  diluentNameLabel = mkLabel(root, COL_SLOT_X, DILUENT_ROW_Y, &lv_font_montserrat_18, colYellow());
  lv_label_set_text(diluentNameLabel, "DIL");
  diluentFo2Label = mkLabel(root, COL_FO2_X, DILUENT_ROW_Y, &lv_font_montserrat_18, colInk());
  diluentFheLabel = mkLabel(root, COL_FHE_X, DILUENT_ROW_Y, &lv_font_montserrat_18, colInk());
  diluentTagLabel = mkLabel(root, COL_EN_X, DILUENT_ROW_Y, &lv_font_montserrat_14, colInkDim());
  lv_label_set_text(diluentTagLabel, "loop");

  lv_obj_t *lastStopLabel = mkLabel(root, COL_SLOT_X, LAST_STOP_ROW_Y, &lv_font_montserrat_18, colInk());
  lv_label_set_text(lastStopLabel, "LAST STOP");
  lastStopValue = mkLabel(root, LAST_STOP_VALUE_X, LAST_STOP_ROW_Y, &lv_font_montserrat_18, colInk());

  fieldHighlight = lv_obj_create(root);
  lv_obj_remove_style_all(fieldHighlight);
  lv_obj_set_style_bg_opa(fieldHighlight, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_color(fieldHighlight, colCyan(), 0);
  lv_obj_set_style_border_width(fieldHighlight, 2, 0);
  lv_obj_set_style_border_side(fieldHighlight, LV_BORDER_SIDE_BOTTOM, 0);

  lv_obj_t *footer = lv_label_create(root);
  lv_obj_set_width(footer, DEVICE_W - 40);
  lv_obj_set_pos(footer, 20, 260);
  lv_obj_set_style_text_align(footer, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(footer, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(footer, colCyan(), 0);
  lv_label_set_text(footer, "MENU -> next field   ACTION +1% / toggle");
}

lv_obj_t *uiGasEditScreenRoot() { return root; }

// Underlines the given label with fieldHighlight, sized to its actual
// rendered width/height rather than a guessed constant -- avoids the box
// clipping off-screen when the label text (e.g. "ON" vs "OFF") changes width.
static void showFieldUnderline(lv_obj_t *field) {
  lv_obj_set_pos(fieldHighlight, lv_obj_get_x(field), lv_obj_get_y(field));
  lv_obj_set_size(fieldHighlight, lv_obj_get_width(field), lv_obj_get_height(field) + 4);
  lv_obj_remove_flag(fieldHighlight, LV_OBJ_FLAG_HIDDEN);
}

void uiGasEditScreenUpdate() {
  for (int i = 0; i < NUM_GAS_MIXES; i++) {
    const GasMix &g = state.gasMixes[i];
    bool dim = !g.enabled;
    bool isActiveBailout = i == state.bailoutGasIdx;

    lv_label_set_text_fmt(slotLabel[i], isActiveBailout ? "%d*" : "%d", i + 1);
    lv_obj_set_style_text_color(slotLabel[i], isActiveBailout ? colYellow() : colInkDim(), 0);

    lv_label_set_text(nameLabel[i], g.label);
    lv_obj_set_style_text_color(nameLabel[i], dim ? colInkDim() : colInk(), 0);

    bool fo2Sel = !state.gasEditLastStopSelected && state.gasEditSlot == i && state.gasEditField == GasEditField::Fo2;
    bool fheSel = !state.gasEditLastStopSelected && state.gasEditSlot == i && state.gasEditField == GasEditField::Fhe;
    bool enSel = !state.gasEditLastStopSelected && state.gasEditSlot == i && state.gasEditField == GasEditField::Enabled;

    lv_label_set_text_fmt(fo2Label[i], "FO2 %d%%", (int)(g.fo2 * 100.0f + 0.5f));
    lv_obj_set_style_text_color(fo2Label[i], dim ? colInkDim() : (fo2Sel ? colCyan() : colInk()), 0);

    lv_label_set_text_fmt(fheLabel[i], "He %d%%", (int)(g.fhe * 100.0f + 0.5f));
    lv_obj_set_style_text_color(fheLabel[i], dim ? colInkDim() : (fheSel ? colCyan() : colInk()), 0);

    lv_label_set_text(enabledLabel[i], g.enabled ? "ON" : "OFF");
    lv_obj_set_style_text_color(enabledLabel[i], enSel ? colCyan() : (g.enabled ? colGreen() : colRed()), 0);

    if (fo2Sel || fheSel || enSel) {
      showFieldUnderline(fo2Sel ? fo2Label[i] : fheSel ? fheLabel[i] : enabledLabel[i]);
    }
  }

  {
    const GasMix &g = state.diluent;
    bool fo2Sel = state.gasEditDiluentSelected && state.gasEditField == GasEditField::Fo2;
    bool fheSel = state.gasEditDiluentSelected && state.gasEditField == GasEditField::Fhe;
    bool isActiveBailout = state.bailoutGasIdx == NUM_GAS_MIXES;

    lv_label_set_text(diluentNameLabel, isActiveBailout ? "DIL*" : "DIL");

    lv_label_set_text_fmt(diluentFo2Label, "FO2 %d%%", (int)(g.fo2 * 100.0f + 0.5f));
    lv_obj_set_style_text_color(diluentFo2Label, fo2Sel ? colCyan() : colInk(), 0);

    lv_label_set_text_fmt(diluentFheLabel, "He %d%%", (int)(g.fhe * 100.0f + 0.5f));
    lv_obj_set_style_text_color(diluentFheLabel, fheSel ? colCyan() : colInk(), 0);

    if (fo2Sel || fheSel) {
      showFieldUnderline(fo2Sel ? diluentFo2Label : diluentFheLabel);
    }
  }

  lv_label_set_text_fmt(lastStopValue, "%dm", state.lastStopDepth);
  lv_obj_set_style_text_color(lastStopValue, state.gasEditLastStopSelected ? colCyan() : colInk(), 0);
  if (state.gasEditLastStopSelected) {
    showFieldUnderline(lastStopValue);
  }
}

} // namespace dc
