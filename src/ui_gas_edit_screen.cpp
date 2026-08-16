#include "ui_gas_edit_screen.h"
#include "state.h"
#include "ui_common.h"
#include <cstdio>

namespace dc {

static const int ROW_Y[NUM_GAS_MIXES] = {36, 74, 112};

static lv_obj_t *root;
static lv_obj_t *slotLabel[NUM_GAS_MIXES];
static lv_obj_t *nameLabel[NUM_GAS_MIXES];
static lv_obj_t *fo2Label[NUM_GAS_MIXES];
static lv_obj_t *fheLabel[NUM_GAS_MIXES];
static lv_obj_t *enabledLabel[NUM_GAS_MIXES];
static lv_obj_t *fieldHighlight;
static lv_obj_t *lastStopValue;

constexpr int DILUENT_ROW_Y = 150;
static lv_obj_t *diluentNameLabel;
static lv_obj_t *diluentFo2Label;
static lv_obj_t *diluentFheLabel;
static lv_obj_t *diluentTagLabel;

constexpr int LAST_STOP_ROW_Y = 188;

void uiGasEditScreenCreate(lv_obj_t *parent) {
  root = lv_obj_create(parent);
  lv_obj_remove_style_all(root);
  lv_obj_set_pos(root, 0, 0);
  lv_obj_set_size(root, 320, 240);
  lv_obj_set_style_bg_color(root, colBg(), 0);
  lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
  lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(root, LV_OBJ_FLAG_HIDDEN);

  lv_obj_t *title = mkLabel(root, 8, 4, &lv_font_montserrat_16, colCyan());
  lv_label_set_text(title, "GAS EDIT");

  lv_obj_t *hint = mkLabel(root, 170, 6, &lv_font_montserrat_12, colInkDim());
  lv_obj_set_width(hint, 142);
  lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_RIGHT, 0);
  lv_label_set_text(hint, "hold MENU to exit");

  for (int i = 0; i < NUM_GAS_MIXES; i++) {
    int y = ROW_Y[i];
    slotLabel[i] = mkLabel(root, 8, y, &lv_font_montserrat_16, colInkDim());
    nameLabel[i] = mkLabel(root, 26, y, &lv_font_montserrat_16, colInk());
    fo2Label[i] = mkLabel(root, 130, y, &lv_font_montserrat_16, colInk());
    fheLabel[i] = mkLabel(root, 204, y, &lv_font_montserrat_16, colInk());
    enabledLabel[i] = mkLabel(root, 276, y, &lv_font_montserrat_14, colGreen());
  }

  diluentNameLabel = mkLabel(root, 8, DILUENT_ROW_Y, &lv_font_montserrat_16, colYellow());
  lv_label_set_text(diluentNameLabel, "DIL");
  diluentFo2Label = mkLabel(root, 130, DILUENT_ROW_Y, &lv_font_montserrat_16, colInk());
  diluentFheLabel = mkLabel(root, 204, DILUENT_ROW_Y, &lv_font_montserrat_16, colInk());
  diluentTagLabel = mkLabel(root, 276, DILUENT_ROW_Y, &lv_font_montserrat_12, colInkDim());
  lv_label_set_text(diluentTagLabel, "loop");

  lv_obj_t *lastStopLabel = mkLabel(root, 8, LAST_STOP_ROW_Y, &lv_font_montserrat_16, colInk());
  lv_label_set_text(lastStopLabel, "LAST STOP");
  lastStopValue = mkLabel(root, 130, LAST_STOP_ROW_Y, &lv_font_montserrat_16, colInk());

  fieldHighlight = lv_obj_create(root);
  lv_obj_remove_style_all(fieldHighlight);
  lv_obj_set_style_bg_opa(fieldHighlight, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_color(fieldHighlight, colCyan(), 0);
  lv_obj_set_style_border_width(fieldHighlight, 2, 0);
  lv_obj_set_style_border_side(fieldHighlight, LV_BORDER_SIDE_BOTTOM, 0);

  lv_obj_t *footer = mkLabel(root, 40, 220, &lv_font_montserrat_12, colCyan());
  lv_obj_set_width(footer, 240);
  lv_obj_set_style_text_align(footer, LV_TEXT_ALIGN_CENTER, 0);
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
