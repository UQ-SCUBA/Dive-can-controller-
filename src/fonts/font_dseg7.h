#pragma once
#include "lvgl.h"

// Seven-segment-style digit fonts for the round dive screen's numeric
// readouts (PPO2, cells, depth, TTS, ceiling depth, deco stop time) --
// generated from DSEG7-Classic-Bold (SIL Open Font License, see
// DSEG-LICENSE.txt in this directory) via lv_font_conv, one size per
// existing montserrat size it replaces. Glyph set is deliberately narrow:
// space, +, -, ., 0-9 -- these fonts are for numbers only, never labels.

extern const lv_font_t font_dseg7_20; // half of font_dseg7_40, for depth's decimal digit
extern const lv_font_t font_dseg7_24; // replaces montserrat_24 (cells, deco numbers)
extern const lv_font_t font_dseg7_40; // replaces montserrat_40 (depth, TTS)
extern const lv_font_t font_dseg7_46; // replaces montserrat_46 (PPO2)
