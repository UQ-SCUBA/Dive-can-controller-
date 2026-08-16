#include "ui_dive_screen.h"
#include "state.h"
#include "deco.h"
#include "ui_common.h"
#include "device_profile.h"
#include "fonts/font_dseg7.h"
#include <cstdio>
#include <cstring>

// Round (360x360 GC9B72) counterpart to ui_dive_screen.cpp, swapped in via
// platformio.ini's build_src_filter for the round/emulator_round envs. Same
// header, same dc::state/dc::cache-derived logic (alert priority, gas
// formatting, edit highlight) as the rect version — only the widget choice
// and coordinates differ, since the round safe-area (usable width shrinks
// fast near the top/bottom poles) doesn't suit the rect layout's corner-
// anchored two-column design.
//
// Coordinates below are a first pass, tuned by eye against
// `pio run -e emulator_round` rather than hand-derived — same "close but not
// pixel-identical, polish later" spirit as the rect screen's own header
// comment.

namespace dc {

// ---- PPO2 dial geometry (top third) ----
// The dial's own object is a full 360x360 square (lv_scale requires
// width==height for round mode / needle support) positioned at (0,0);
// background is fully transparent so only the arc/ticks/needle paint, and
// the shallow top-centered sweep below keeps the visible ring inside
// roughly the top third even though the object spans the whole screen.
constexpr int32_t PPO2_MIN_CBAR = 30;  // 0.30 bar, *100 -- lv_scale range is integer
constexpr int32_t PPO2_MAX_CBAR = 170; // 1.70 bar
constexpr int32_t PPO2_RED_LOW_MAX_CBAR = 50;   // red zone: 0.30-0.50
constexpr int32_t PPO2_RED_HIGH_MIN_CBAR = 140; // red zone: 1.40-1.70 (extends to PPO2_MAX_CBAR)

// ---- low-PO2 headline warning cycle (value <-> "LOW PPO2") ----
constexpr float PPO2_FLASH_THRESHOLD = 0.35f;
constexpr uint32_t PPO2_FLASH_OFF_MS = 500;     // "LOW PPO2" shown for this long
constexpr uint32_t PPO2_FLASH_PERIOD_MS = 1000; // ...then the value for the remaining 0.5s

constexpr int32_t PPO2_SCALE_ANGLE_RANGE = 150; // sweeps low end -> high end
// Centered symmetrically on 270 deg (12 o'clock, in lv_scale's clockwise-
// from-3-o'clock convention) -- rotation + angle_range/2 = 270. An earlier
// version solved for a rotation that pinned the 1.0 bar setpoint exactly at
// 12 o'clock instead, but that made the two ends sweep unequal distances
// from top (0.3 further than 1.6), which read as the whole ring/label
// assembly sitting off-center even though it's concentric with the screen.
// Symmetric is simpler and looks centered, at the cost of 1.0 no longer
// landing exactly at top (dead center is the range's numeric midpoint,
// 0.95 bar, instead).
constexpr int32_t PPO2_SCALE_ROTATION = 270 - PPO2_SCALE_ANGLE_RANGE / 2;

// ---- depth readout position (shared between uiDiveScreenCreate() and
// uiDiveScreenUpdate(), which repositions the decimal digit every tick) ----
constexpr int DEPTH_VALUE_X = 20;
constexpr int DEPTH_VALUE_Y = 206;
// The decimal digit sits right after the whole part, bottom-aligned with
// it -- font_dseg7_40 and font_dseg7_20 both have base_line = 0 (no
// descender padding), so offsetting by the line-height difference (40-20)
// lines up their bottoms exactly.
constexpr int DEPTH_FRAC_Y = DEPTH_VALUE_Y + (40 - 20);

// ---- segmented "build up" bargraph (80s-digital-dashboard style) ----
// Replaces both the old smooth colored zone arc and the needle: instead of
// a single pointer, discrete blocks light up from the low end of the scale
// up to the current reading, each colored by its own zone (red/green/red);
// nothing is drawn for a value the reading hasn't reached yet. lv_scale
// itself (below) is kept only for the tick marks + number labels -- its own
// main arc is made fully transparent.
constexpr int SEGMENT_COUNT = 14;          // (1.70-0.30)/0.10 -- one "real" (independently colored)
                                            // segment per 0.1 bar.
// Narrower sub-bars per real segment, purely for a denser look -- doesn't
// track finer-grained lit/unlit state, see poSegmentColState below (one
// entry per *zone*, not per sub-bar).
//
// This used to be SEGMENT_COUNT separate lv_arc objects (one per sub-bar,
// up to 70 total at SEGMENT_SUBDIV=5). Every one of those shared an
// identical ~300x300 bounding box (lv_arc's box has to stay that big
// regardless of how narrow the visible slice is, since that box is also
// what determines its radius) -- LVGL's core has to bounding-box-test/skip
// every object sharing a dirty region on *any* redraw touching it, not just
// the bargraph's own updates, so more objects there meant slower redraws
// for the hold-progress bar, the flashing PPO2 label, anything nearby, even
// with lv_obj_set_style_arc_color() itself gated to only fire on an actual
// zone-color change (measured on real hardware: ~1.8s/tick at 70 objects,
// ~190ms at 14, still visibly less responsive at 70 than at 14 even with
// the gate in place). Replaced below with a single custom-drawn lv_obj
// (poRing) that paints every bar itself via lv_draw_arc() in one draw
// pass -- one object, one bounding box, so the per-redraw object-count cost
// above no longer applies, and the actual bar count is now free to set
// purely for looks.
constexpr int SEGMENT_SUBDIV = 2;
constexpr int TOTAL_BARS = SEGMENT_COUNT * SEGMENT_SUBDIV;
// lv_draw_arc() truncates every angle to a whole degree at draw time
// regardless of how precisely it's computed (lv_value_precise_t is int32_t
// unless LV_USE_FLOAT is on, which it isn't here), so bar width and gap are
// chosen here as plain whole degrees and every one of the TOTAL_BARS bars
// is laid out as a single flat, evenly-stepped sequence (not derived per
// real segment) -- that's what makes every gap in the ring, whether between
// two bars in the same 0.1-bar segment or crossing into the next one, come
// out identical.
constexpr int BAR_DEG = 4;                 // rendered width of one bar
constexpr int BAR_GAP_DEG = 1;             // gap between any two adjacent bars, intra- or inter-segment
constexpr float SEGMENT_RADIUS = 150.0f;
constexpr int SEGMENT_ARC_WIDTH = 18;
// One real segment's total angular width (all its sub-bars + the gaps
// between them) -- used both to size the ring's start offset below and by
// the "nudge 2/3 of a segment counter-clockwise" adjustment in that same
// expression (eyeballed against the tick number labels on real hardware).
constexpr float ONE_SEGMENT_DEG = SEGMENT_SUBDIV * BAR_DEG + (SEGMENT_SUBDIV - 1) * BAR_GAP_DEG;
// Symmetric leftover between the bars' total angular span and the full
// PPO2_SCALE_ANGLE_RANGE, applied as a plain float offset (not per-bar) so
// it doesn't disturb the whole-degree bar/gap spacing above once each bar's
// angles get truncated at draw time. File-scope (not local to
// uiDiveScreenCreate()) since ringDrawEventCb() needs it too.
constexpr float RING_START_DEG = PPO2_SCALE_ROTATION +
    (PPO2_SCALE_ANGLE_RANGE - (TOTAL_BARS * BAR_DEG + (TOTAL_BARS - 1) * BAR_GAP_DEG)) / 2.0f -
    ONE_SEGMENT_DEG * 2.0f / 3.0f;

// colInkDim() (0x3f6b5c) at 1/3 brightness -- each channel /3 -- for
// segments the reading hasn't reached yet.
inline lv_color_t colSegmentUnlit() { return lv_color_hex(0x15241f); }

static lv_obj_t *poScale;
static lv_obj_t *poRing; // single custom-drawn widget -- see SEGMENT_SUBDIV's comment above
// Each zone's current color state (0=unlit/1=red/2=green) -- read directly
// by ringDrawEventCb() at paint time, so this doubles as both "what to
// draw" and (compared against on the next tick) "did anything actually
// change" -- see poRingDirty below.
static int poSegmentColState[SEGMENT_COUNT];
// Set whenever uiDiveScreenUpdate() changes a zone's color state; checked
// (and cleared) right after triggering the one lv_obj_invalidate(poRing)
// that redraw needs -- keeps the ring untouched, same as before, on ticks
// where the reading hasn't moved into a new zone.
static bool poRingDirty = false;

static void ringDrawEventCb(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_DRAW_MAIN) return;
  lv_layer_t *layer = lv_event_get_layer(e);
  lv_obj_t *obj = (lv_obj_t *)lv_event_get_current_target(e);

  lv_area_t coords;
  lv_obj_get_coords(obj, &coords);
  lv_point_t center = {(coords.x1 + coords.x2 + 1) / 2, (coords.y1 + coords.y2 + 1) / 2};

  lv_draw_arc_dsc_t dsc;
  lv_draw_arc_dsc_init(&dsc);
  dsc.center = center;
  dsc.radius = (uint16_t)SEGMENT_RADIUS;
  dsc.width = SEGMENT_ARC_WIDTH;
  dsc.rounded = false; // flat ends read as "blocks", not a smooth pointer

  for (int i = 0; i < SEGMENT_COUNT; i++) {
    switch (poSegmentColState[i]) {
      case 1: dsc.color = colRed(); break;
      case 2: dsc.color = colGreen(); break;
      default: dsc.color = colSegmentUnlit(); break;
    }
    for (int j = 0; j < SEGMENT_SUBDIV; j++) {
      int k = i * SEGMENT_SUBDIV + j;
      float angleStart = RING_START_DEG + k * (BAR_DEG + BAR_GAP_DEG);
      dsc.start_angle = angleStart;
      dsc.end_angle = angleStart + BAR_DEG;
      lv_draw_arc(layer, &dsc);
    }
  }
}

static lv_obj_t *poPO2Value;
// Alternates with poPO2Value below PPO2_FLASH_THRESHOLD -- normal-font
// overlay since font_dseg7_46 (poPO2Value's font) is digits/./+/-/space
// only, same reasoning as poCellFailLabel below.
static lv_obj_t *poPO2LowLabel;
static lv_obj_t *poCellLabel[NUM_CELLS];
static lv_obj_t *poCellValue[NUM_CELLS];
// A failed cell's formatCellValue() text is "FAIL" -- letters, which the
// 7-segment font (digits/./+/-/space only) can't render -- so a failed
// cell swaps to this normal-font overlay instead of trying to show "FAIL"
// in poCellValue itself.
static lv_obj_t *poCellFailLabel[NUM_CELLS];
static lv_obj_t *poSrcValue;
static lv_obj_t *poSpValue;
static lv_obj_t *poWetLabel;
static lv_obj_t *poDepthValue; // whole part + "." (e.g. "24."), full-size 7-segment font
static lv_obj_t *poDepthFrac;  // decimal digit, 1/3 size, positioned after poDepthValue each tick
static lv_obj_t *poTtsValue; // mirrors poDepthValue's label-above-value layout, on the right
static lv_obj_t *poDecoValue; // combined "NDL 8 min" / "STOP 6m  12 min" line
static lv_obj_t *poStatsLine; // combined "34 min | MAX 24m" line (TTS split out above)
static lv_obj_t *poHighlightBox;
static lv_obj_t *poBanner;
static lv_obj_t *poBannerLabel;

// Centered-text convenience — the round layout is built around the vertical
// axis rather than the rect screen's left/right two-column split, since the
// safe chord is centered, not corner-anchored.
static lv_obj_t *mkCenterLabel(lv_obj_t *parent, int y, int width, const lv_font_t *font, lv_color_t color) {
  lv_obj_t *l = lv_label_create(parent);
  lv_obj_set_width(l, width);
  lv_obj_set_pos(l, (DEVICE_W - width) / 2, y);
  lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, color, 0);
  return l;
}

// Neither lv_label_set_text()/_set_text_fmt() nor lv_obj_set_style_*()
// compare against the current value before acting -- both unconditionally
// do their work (text: realloc + memcpy the label's buffer; style props:
// lv_obj_set_local_style_prop(), see lv_obj_style.c) and both end in an
// unconditional lv_obj_invalidate(), even when the new value is identical
// to what's already there (verified by reading both call chains -- an
// earlier version of this comment assumed style setters had a built-in
// equality check; they don't). uiDiveScreenUpdate() sets ~12 labels' text
// *and* color every 20ms tick regardless of whether either actually
// changed, and poScale's tick/number-label widget spans the *entire*
// screen (lv_scale requires width==height for round mode, see its own
// comment), so every one of those unconditional invalidations forces LVGL
// to also reconsider that large, expensive-to-redraw widget -- measured at
// several hundred ms/tick on real hardware, confirmed by disabling
// uiDiveScreenUpdate() entirely and watching the cost disappear. Gating on
// an actual content change (same discipline as poSegmentColState above)
// fixes it at the source, same as the arc ring did.
static void setLabelText(lv_obj_t *label, const char *text) {
  if (strcmp(lv_label_get_text(label), text) != 0) lv_label_set_text(label, text);
}

static void setLabelColor(lv_obj_t *label, lv_color_t color) {
  if (!lv_color_eq(lv_obj_get_style_text_color(label, 0), color)) {
    lv_obj_set_style_text_color(label, color, 0);
  }
}

static void setBgColor(lv_obj_t *obj, lv_color_t color) {
  if (!lv_color_eq(lv_obj_get_style_bg_color(obj, 0), color)) {
    lv_obj_set_style_bg_color(obj, color, 0);
  }
}

static void setBgOpa(lv_obj_t *obj, lv_opa_t opa) {
  if (lv_obj_get_style_bg_opa(obj, 0) != opa) {
    lv_obj_set_style_bg_opa(obj, opa, 0);
  }
}

void uiDiveScreenCreate(lv_obj_t *parent) {
  lv_obj_set_style_bg_color(parent, colBg(), 0);
  lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(parent, 0, 0);
  lv_obj_set_style_pad_all(parent, 0, 0);
  lv_obj_set_style_radius(parent, 0, 0);

  // ---- PPO2 dial: tick marks + number labels only (lv_scale) ----
  // The smooth main arc is fully transparent -- the segmented bargraph
  // below is what actually shows red/green/red now -- but lv_scale still
  // owns the tick/label geometry (ROUND_OUTER puts them outside the arc
  // radius, right near the edge, per the numbers-on-the-outside request).
  poScale = lv_scale_create(parent);
  lv_obj_set_pos(poScale, 0, 0);
  lv_obj_set_size(poScale, DEVICE_W, DEVICE_H);
  lv_obj_set_style_bg_opa(poScale, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(poScale, 0, 0);
  // Symmetric padding shrinks lv_scale's own tick/label radius (the object
  // stays 360x360, so its center -- and therefore the segmented ring and SP
  // marker below, which are positioned independently around that same
  // (180,180) -- doesn't move) leaving enough outward room for the now-
  // bigger ROUND_OUTER labels to land inside the true edge instead of
  // running off it. Tuned against emulator_round screenshots, not derived.
  lv_obj_set_style_pad_all(poScale, 33, LV_PART_MAIN);
  lv_scale_set_mode(poScale, LV_SCALE_MODE_ROUND_OUTER);
  lv_scale_set_rotation(poScale, PPO2_SCALE_ROTATION);
  lv_scale_set_angle_range(poScale, PPO2_SCALE_ANGLE_RANGE);
  lv_scale_set_range(poScale, PPO2_MIN_CBAR, PPO2_MAX_CBAR);
  // Ticks every 0.1 bar (0.30-1.70 in 14 steps -> 15 ticks), every one of
  // them a "major" tick (eligible for a text_src label) -- the requested
  // label set (0.3, 0.5, 0.7, 1.0, 1.2, 1.5, 1.7) isn't evenly spaced (0.2
  // and 0.3 gaps mixed), so it can't be produced via major_tick_every's
  // fixed stride. Blank ("") entries below suppress the ticks in between;
  // since tick MARKS are already invisible (arc/line opa transparent,
  // below), a blank label leaves nothing visible there at all.
  lv_scale_set_total_tick_count(poScale, 15);
  lv_scale_set_major_tick_every(poScale, 1);
  static const char *ppo2TickLabels[] = {
      "0.3", "", "0.5", "", "0.7", "", "", "1.0", "", "", "1.3", "", "1.5", "", "1.7", nullptr};
  lv_scale_set_text_src(poScale, ppo2TickLabels);
  lv_obj_set_style_arc_opa(poScale, LV_OPA_TRANSP, LV_PART_MAIN); // hide the smooth arc entirely
  lv_obj_set_style_text_color(poScale, lv_color_white(), LV_PART_INDICATOR);
  lv_obj_set_style_text_font(poScale, &lv_font_montserrat_20, LV_PART_INDICATOR); // ~1.5x the previous (default ~14px) size
  // Tick marks hidden (opacity, not width/length) so the number labels --
  // whose position lv_scale computes from the tick length -- don't shift.
  lv_obj_set_style_line_opa(poScale, LV_OPA_TRANSP, LV_PART_INDICATOR); // major tick marks
  lv_obj_set_style_line_opa(poScale, LV_OPA_TRANSP, LV_PART_ITEMS);     // minor tick marks

  // Segmented bargraph: a single custom-drawn widget (see ringDrawEventCb()
  // and SEGMENT_SUBDIV's comment above) rather than one lv_arc per bar --
  // TOTAL_BARS bars, each BAR_DEG wide with BAR_GAP_DEG between it and the
  // next, painted fresh every time the widget redraws (see poRingDirty).
  //
  // Parented to `parent`, not poScale: poScale has padding (above) to
  // shrink its own tick/label radius, and LVGL positions children relative
  // to a parent's *padded* content box, not its raw top-left -- so a child
  // of poScale would inherit a diagonal offset the ticks themselves don't
  // have (lv_scale's own tick/label center calc compensates for its padding
  // internally; a plain child position doesn't). Parenting here to the
  // padding-free `parent` instead keeps (180,180) meaning the same absolute
  // point for both.
  poRing = lv_obj_create(parent);
  lv_obj_remove_style_all(poRing);
  lv_obj_remove_flag(poRing, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_opa(poRing, LV_OPA_TRANSP, 0); // nothing painted but the bars themselves
  lv_obj_set_size(poRing, (int)(SEGMENT_RADIUS * 2), (int)(SEGMENT_RADIUS * 2));
  lv_obj_set_pos(poRing, (int)(180 - SEGMENT_RADIUS), (int)(180 - SEGMENT_RADIUS));
  lv_obj_add_event_cb(poRing, ringDrawEventCb, LV_EVENT_DRAW_MAIN, nullptr);
  for (int i = 0; i < SEGMENT_COUNT; i++) poSegmentColState[i] = 0; // matches colSegmentUnlit()

  // ---- readout inside the dial's empty bowl ----
  poPO2Value = mkCenterLabel(parent, 64, DEVICE_W, &font_dseg7_46, colGreen()); // 7-segment style
  lv_label_set_text(poPO2Value, "0.70");

  // Roughly centered on poPO2Value's line (46px 7-segment vs. this 28px
  // label) -- close enough for an alternating warning, not pixel-matched.
  poPO2LowLabel = mkCenterLabel(parent, 78, DEVICE_W, &lv_font_montserrat_28, colRed());
  lv_label_set_text(poPO2LowLabel, "LOW PPO2");
  lv_obj_add_flag(poPO2LowLabel, LV_OBJ_FLAG_HIDDEN);

  // ---- middle band: cells / SRC / S.P. / depth / deco ----
  static const char *cellNames[NUM_CELLS] = {"C1", "C2", "C3"};
  constexpr int CELL_COL_W = 74; // narrower columns -> the 3 cells sit closer together
  for (int i = 0; i < NUM_CELLS; i++) {
    int x = (DEVICE_W - 3 * CELL_COL_W) / 2 + i * CELL_COL_W;
    poCellLabel[i] = lv_label_create(parent);
    lv_obj_set_width(poCellLabel[i], CELL_COL_W);
    lv_obj_set_pos(poCellLabel[i], x, 126);
    lv_obj_set_style_text_align(poCellLabel[i], LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(poCellLabel[i], &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(poCellLabel[i], lv_color_white(), 0);
    lv_label_set_text(poCellLabel[i], cellNames[i]);

    poCellValue[i] = lv_label_create(parent);
    lv_obj_set_width(poCellValue[i], CELL_COL_W);
    lv_obj_set_pos(poCellValue[i], x, 140);
    lv_obj_set_style_text_align(poCellValue[i], LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(poCellValue[i], &font_dseg7_24, 0); // 7-segment style
    lv_label_set_text(poCellValue[i], "0.70");

    poCellFailLabel[i] = lv_label_create(parent);
    lv_obj_set_width(poCellFailLabel[i], CELL_COL_W);
    lv_obj_set_pos(poCellFailLabel[i], x, 140);
    lv_obj_set_style_text_align(poCellFailLabel[i], LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(poCellFailLabel[i], &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(poCellFailLabel[i], colRed(), 0);
    lv_label_set_text(poCellFailLabel[i], "FAIL");
    lv_obj_add_flag(poCellFailLabel[i], LV_OBJ_FLAG_HIDDEN);
  }

  poSrcValue = mkCenterLabel(parent, 172, DEVICE_W, &lv_font_montserrat_24, colCyan()); // 1.5x the previous montserrat_16; y=172 lines up with the DEPTH/TTS labels
  lv_label_set_text(poSrcValue, "CC");

  poWetLabel = mkCenterLabel(parent, 204, DEVICE_W, &lv_font_montserrat_12, colCyan());
  lv_label_set_text(poWetLabel, LV_SYMBOL_TINT " WET");
  lv_obj_add_flag(poWetLabel, LV_OBJ_FLAG_HIDDEN);

  poSpValue = mkCenterLabel(parent, 206, DEVICE_W, &lv_font_montserrat_30, lv_color_white()); // 1.5x the previous montserrat_20; y=206 lines up with the depth/TTS numeric values
  lv_label_set_text(poSpValue, "0.7"); // "S.P." prefix removed -- centered under CC/OC on its own

  // highlight box, reused for both S.P. and SRC edit states
  poHighlightBox = lv_obj_create(parent);
  lv_obj_remove_style_all(poHighlightBox);
  lv_obj_set_style_bg_opa(poHighlightBox, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_color(poHighlightBox, colCyan(), 0);
  lv_obj_set_style_border_width(poHighlightBox, 2, 0);
  lv_obj_set_style_radius(poHighlightBox, 4, 0);
  lv_obj_add_flag(poHighlightBox, LV_OBJ_FLAG_HIDDEN);

  // Below this point the safe chord narrows fast toward the bottom pole —
  // positions here are checked against the true 360-diameter inscribed
  // circle (center 180,180, r=180), not just eyeballed: at y=336 (the
  // bottom-most row) the safe half-width is only ~90px. Verified by
  // rendering and measuring actual glyph extents against that circle
  // (see the emulator_round screenshots), not hand-derived.
  // Left-anchored (not centered) close to the left edge, wide enough for 3
  // digits before the decimal (e.g. "123.4"); value sits roughly where the
  // label used to be, with the label bumped up above it.
  constexpr int DEPTH_LABEL_W = 170;
  // Measured against a rendered "24.0"/"DEPTH" pair: shifts the label left
  // of the value's own box so the "P" (the label's visual center, being the
  // middle letter of 5) lands over the last digit before the decimal
  // instead of over the whole value string's center.
  constexpr int DEPTH_LABEL_X = DEPTH_VALUE_X - 52;
  lv_obj_t *depthLabel = lv_label_create(parent);
  lv_obj_set_width(depthLabel, DEPTH_LABEL_W);
  lv_obj_set_pos(depthLabel, DEPTH_LABEL_X, 172);
  lv_obj_set_style_text_align(depthLabel, LV_TEXT_ALIGN_CENTER, 0); // centered over the box the value sits in, not left-anchored like the value itself
  lv_obj_set_style_text_font(depthLabel, &lv_font_montserrat_24, 0); // 2x the previous montserrat_12
  lv_obj_set_style_text_color(depthLabel, lv_color_white(), 0);
  lv_label_set_text(depthLabel, "DEPTH");

  // Whole part + "." (e.g. "24.") at full size, content-width so its
  // rendered pixel width can be measured each tick (below) to place the
  // decimal digit right after it -- DSEG7's "." has ~zero advance width, so
  // the box only grows with the digit count, not the point itself.
  poDepthValue = lv_label_create(parent);
  lv_obj_set_width(poDepthValue, LV_SIZE_CONTENT);
  lv_obj_set_height(poDepthValue, LV_SIZE_CONTENT);
  lv_obj_set_pos(poDepthValue, DEPTH_VALUE_X, DEPTH_VALUE_Y);
  lv_obj_set_style_text_font(poDepthValue, &font_dseg7_40, 0); // 7-segment style
  lv_obj_set_style_text_color(poDepthValue, colInk(), 0);
  lv_label_set_text(poDepthValue, "0.");

  // Decimal digit at half size, bottom-aligned with the whole part (both
  // fonts have base_line = 0, i.e. no descender padding, so their line
  // heights' bottoms coincide when their tops are offset by the line-height
  // difference) -- makes it read as a fractional digit at a glance rather
  // than just another same-size digit.
  poDepthFrac = lv_label_create(parent);
  lv_obj_set_width(poDepthFrac, LV_SIZE_CONTENT);
  lv_obj_set_height(poDepthFrac, LV_SIZE_CONTENT);
  lv_obj_set_style_text_font(poDepthFrac, &font_dseg7_20, 0);
  lv_obj_set_style_text_color(poDepthFrac, colInk(), 0);
  lv_label_set_text(poDepthFrac, "0");

  // TTS: same label-above-value layout/fonts as depth, mirrored onto the
  // right side (right-anchored near the edge instead of left-anchored, and
  // right-aligned text instead of left-aligned, so it grows toward center
  // the same way depth does from the other side).
  constexpr int TTS_RIGHT_X = DEVICE_W - DEPTH_VALUE_X; // 340
  // Measured against a rendered "TTS"/"205" pair: shifts the label right so
  // the 2nd T (its visual center, being the middle of the two adjacent T's)
  // lands over the value's tens digit instead of the label's own center.
  constexpr int TTS_LABEL_X = (TTS_RIGHT_X - DEPTH_LABEL_W) + 49;
  lv_obj_t *ttsLabel = lv_label_create(parent);
  lv_obj_set_width(ttsLabel, DEPTH_LABEL_W);
  lv_obj_set_pos(ttsLabel, TTS_LABEL_X, 172);
  lv_obj_set_style_text_align(ttsLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(ttsLabel, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(ttsLabel, lv_color_white(), 0);
  lv_label_set_text(ttsLabel, "TTS");

  poTtsValue = lv_label_create(parent);
  lv_obj_set_width(poTtsValue, DEPTH_LABEL_W);
  lv_obj_set_pos(poTtsValue, TTS_RIGHT_X - DEPTH_LABEL_W, 206);
  lv_obj_set_style_text_align(poTtsValue, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_style_text_font(poTtsValue, &font_dseg7_40, 0); // 7-segment style
  lv_obj_set_style_text_color(poTtsValue, colInk(), 0);
  lv_label_set_text(poTtsValue, "--");

  // NDL/STOP label+value combined onto one line (rather than the rect
  // screen's two stacked lines) to leave room for the stats/banner row
  // below without crowding the bottom pole.
  poDecoValue = mkCenterLabel(parent, 255, DEVICE_W, &lv_font_montserrat_24, colGreen());
  lv_label_set_text(poDecoValue, "NDL 0 min");

  // Dive time / max depth combined onto one compact line (TTS now has its
  // own block above instead of sharing this line).
  poStatsLine = mkCenterLabel(parent, 306, DEVICE_W, &lv_font_montserrat_18, colInk()); // 1.5x the previous montserrat_12
  lv_label_set_text(poStatsLine, "0 min  MAX 0.0m");

  // Alert/hint banner shares the same row as poStatsLine (hidden whenever
  // the banner is shown, in uiDiveScreenUpdate()) rather than stacking
  // below it — there isn't enough safe width left for a second row at
  // this height for a full sentence like "BELOW CEILING - 9m REQUIRED".
  poBanner = lv_obj_create(parent);
  lv_obj_remove_style_all(poBanner);
  constexpr int BANNER_W = 210;
  lv_obj_set_pos(poBanner, (DEVICE_W - BANNER_W) / 2, 300);
  lv_obj_set_size(poBanner, BANNER_W, 20);
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

  // ---- PPO2 dial + headline value ----
  float ppo2 = 0.0f;
  char po2Text[16];
  lv_color_t po2Color;
  const char *ppo2AlertText = nullptr;
  bool ppo2AlertIsCaution = false;
  if (eff.isBailout) {
    if (eff.gas) {
      ppo2 = eff.gas->fo2 * ata(state.depth);
      snprintf(po2Text, sizeof(po2Text), "%.2f", ppo2);
      po2Color = (ppo2 < PPO2_HYPOXIA_MIN || ppo2 > PPO2_TOX_MAX) ? colRed() : colGreen();
      if (ppo2 > PPO2_TOX_MAX) {
        ppo2AlertText = "HIGH PO2 - O2 TOX RISK";
      } else if (ppo2 < PPO2_HYPOXIA_MIN) {
        ppo2AlertText = "LOW PO2 - HYPOXIA RISK";
      } else if (!eff.isAuto) {
        const GasMix *best = pickBestBailoutGas(state.depth);
        if (best && best != eff.gas) {
          ppo2AlertText = "NOT BEST MIX";
          ppo2AlertIsCaution = true;
        }
      }
    } else {
      ppo2 = 0.0f;
      snprintf(po2Text, sizeof(po2Text), "--");
      po2Color = colRed();
      ppo2AlertText = "NO SAFE BAILOUT GAS";
    }
  } else {
    float consensus = 0;
    computeConsensus(&consensus);
    ppo2 = consensus;
    snprintf(po2Text, sizeof(po2Text), "%.2f", ppo2);
    bool low = ppo2 < 0.5f;
    po2Color = low ? colRed() : colGreen();
    if (low) {
      ppo2AlertText = "LOW PO2 - CHECK LOOP";
    } else if (!pickBestBailoutGas(state.depth)) {
      ppo2AlertText = "NO SAFE BAILOUT GAS";
    }
  }
  setLabelColor(poPO2Value, po2Color);

  // Below PPO2_FLASH_THRESHOLD, cycle the headline between the numeric
  // reading and a "LOW PPO2" warning (rather than blinking the value off/
  // on) -- keeps cycling for as long as the condition holds, reverting to
  // steady numeric display the moment ppo2 climbs back above the
  // threshold. The segmented bargraph is left alone -- only the headline
  // cycles.
  bool showValue = true;
  if (ppo2 <= PPO2_FLASH_THRESHOLD) {
    showValue = (lv_tick_get() % PPO2_FLASH_PERIOD_MS) >= PPO2_FLASH_OFF_MS;
  }
  if (showValue) {
    setLabelText(poPO2Value, po2Text);
    lv_obj_remove_flag(poPO2Value, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(poPO2LowLabel, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(poPO2Value, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(poPO2LowLabel, LV_OBJ_FLAG_HIDDEN);
  }

  int32_t ppo2Cbar = (int32_t)(ppo2 * 100.0f + 0.5f);
  if (ppo2Cbar < PPO2_MIN_CBAR) ppo2Cbar = PPO2_MIN_CBAR;
  if (ppo2Cbar > PPO2_MAX_CBAR) ppo2Cbar = PPO2_MAX_CBAR;

  // Segmented build-up: light every block from the low end up to the
  // current reading, colored by its own zone; anything beyond the reading
  // stays dim/"unlit". Updates poSegmentColState (what ringDrawEventCb()
  // paints) and invalidates poRing at most once, only if a zone's state
  // actually changed -- same "untouched on an unchanging reading" discipline
  // as the old per-object version, just scoped to one widget instead of
  // SEGMENT_COUNT of them.
  {
    constexpr int32_t cbarRange = PPO2_MAX_CBAR - PPO2_MIN_CBAR;
    for (int i = 0; i < SEGMENT_COUNT; i++) {
      int32_t segStartCbar = PPO2_MIN_CBAR + i * cbarRange / SEGMENT_COUNT;
      int colState;
      if (ppo2Cbar < segStartCbar) {
        colState = 0;
      } else if (segStartCbar < PPO2_RED_LOW_MAX_CBAR || segStartCbar >= PPO2_RED_HIGH_MIN_CBAR) {
        colState = 1;
      } else {
        colState = 2;
      }
      if (colState == poSegmentColState[i]) continue; // unchanged since last tick -- nothing to redraw
      poSegmentColState[i] = colState;
      poRingDirty = true;
    }
    if (poRingDirty) {
      lv_obj_invalidate(poRing);
      poRingDirty = false;
    }
  }

  bool editingSp = state.uiMode == UiMode::SpEdit;
  bool editingGas = state.uiMode == UiMode::GasSelect;
  SpMode spShown = editingSp ? state.spCursor : state.spMode;

  // ---- cells ----
  for (int i = 0; i < NUM_CELLS; i++) {
    if (state.cells[i] == CellState::Fail) {
      // formatCellValue() would return literal "FAIL" text here, which the
      // 7-segment font can't render (letters) -- show the normal-font
      // overlay instead and skip the numeric label entirely.
      lv_obj_add_flag(poCellValue[i], LV_OBJ_FLAG_HIDDEN);
      lv_obj_remove_flag(poCellFailLabel[i], LV_OBJ_FLAG_HIDDEN);
    } else {
      char buf[8];
      lv_color_t c;
      formatCellValue(i, buf, sizeof(buf), &c);
      setLabelText(poCellValue[i], buf);
      setLabelColor(poCellValue[i], c);
      lv_obj_remove_flag(poCellValue[i], LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(poCellFailLabel[i], LV_OBJ_FLAG_HIDDEN);
    }
  }

  // ---- SRC / S.P., with edit highlight ----
  if (editingGas) {
    const GasMix &candidate = gasCandidate(state.gasCursor);
    float candP = candidate.fo2 * ata(state.depth);
    char gasBuf[24];
    formatGasCandidate(candidate, gasBuf, sizeof(gasBuf));
    char srcBuf[32];
    snprintf(srcBuf, sizeof(srcBuf), "-> %s", gasBuf);
    setLabelText(poSrcValue, srcBuf);
    bool candUnsafe = candP < PPO2_HYPOXIA_MIN || candP > PPO2_TOX_MAX;
    bool candBest = !candUnsafe && pickBestBailoutGas(state.depth) == &candidate;
    setLabelColor(poSrcValue, candUnsafe ? colRed() : (candBest ? colGreen() : colYellow()));
  } else if (eff.isBailout) {
    if (eff.gas) {
      char gasBuf[24];
      formatGasCandidate(*eff.gas, gasBuf, sizeof(gasBuf));
      char srcBuf[32];
      snprintf(srcBuf, sizeof(srcBuf), "OC . %s", gasBuf);
      setLabelText(poSrcValue, srcBuf);
    } else {
      setLabelText(poSrcValue, "OC . --");
    }
    setLabelColor(poSrcValue, colYellow());
  } else {
    setLabelText(poSrcValue, "CC");
    setLabelColor(poSrcValue, colCyan());
  }

  if (spDisplayBlanked(editingSp)) {
    // On bailout the loop's solenoid isn't driving to a target — S.P. isn't
    // active, so show it blank rather than the last CC value (which is
    // preserved untouched in state.spMode and comes back once back on the
    // loop; see pressMenu()'s bailout branch, which already keeps SpEdit
    // unreachable from here).
    setLabelText(poSpValue, "--");
    setLabelColor(poSpValue, colInkDim());
  } else {
    setLabelText(poSpValue, spModeName(spShown));
    setLabelColor(poSpValue, editingSp ? colCyan() : lv_color_white());
  }

  if (editingSp) {
    // Box follows poSpValue's y=206/montserrat_30. Width measured against a
    // rendered "1.0" (~29px, all four SpMode strings are the same
    // digit-dot-digit shape so render the same width) plus padding, now
    // that the "S.P. " prefix (and the wider box it needed) is gone.
    constexpr int SP_HIGHLIGHT_W = 60;
    lv_obj_set_pos(poHighlightBox, (DEVICE_W - SP_HIGHLIGHT_W) / 2, 202);
    lv_obj_set_size(poHighlightBox, SP_HIGHLIGHT_W, 40);
    lv_obj_remove_flag(poHighlightBox, LV_OBJ_FLAG_HIDDEN);
  } else if (editingGas) {
    // Box follows poSrcValue's y=172/montserrat_24 (moved up from y=184).
    // Width measured against the widest actual candidate string, "-> DIL
    // 21/0" (the diluent's "DIL %d/%d" is longer than any numbered mix's
    // plain "%d/%d") -- 118px rendered, plus a little padding.
    constexpr int GAS_HIGHLIGHT_W = 140;
    lv_obj_set_pos(poHighlightBox, (DEVICE_W - GAS_HIGHLIGHT_W) / 2, 168);
    lv_obj_set_size(poHighlightBox, GAS_HIGHLIGHT_W, 36);
    lv_obj_remove_flag(poHighlightBox, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(poHighlightBox, LV_OBJ_FLAG_HIDDEN);
  }

  if (state.wetSensor) lv_obj_remove_flag(poWetLabel, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(poWetLabel, LV_OBJ_FLAG_HIDDEN);

  // ---- depth / TTS / deco ----
  {
    // Split so the decimal digit can be rendered at 1/3 size (see
    // uiDiveScreenCreate()) -- snprintf's rounding is done on the whole
    // "%.1f" so e.g. 23.96 still shows "24.0", not "23.9"/"10" mismatched.
    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f", state.depth);
    char *dot = strchr(buf, '.');
    char frac = dot && dot[1] ? dot[1] : '0';
    if (dot) dot[1] = '\0'; // truncate buf to "<whole>."
    setLabelText(poDepthValue, buf);
    char fracBuf[2] = {frac, '\0'};
    setLabelText(poDepthFrac, fracBuf);
    lv_obj_update_layout(poDepthValue); // finalize content-width before measuring it below
    // Only reposition when the whole-part's rendered width actually
    // changed (e.g. "9." -> "10.") -- lv_obj_set_pos() invalidates both the
    // old and new rects unconditionally, which is wasted work on the very
    // common case of the digit count staying the same tick to tick, and
    // this label sits inside the segmented ring's shared bounding region
    // (see poSegmentColState's comment above).
    static int lastDepthFracX = -1;
    int depthFracX = DEPTH_VALUE_X + lv_obj_get_width(poDepthValue);
    if (depthFracX != lastDepthFracX) {
      lastDepthFracX = depthFracX;
      lv_obj_set_pos(poDepthFrac, depthFracX, DEPTH_FRAC_Y);
    }
  }
  if (cache.tts >= 0) {
    char ttsBuf[8];
    snprintf(ttsBuf, sizeof(ttsBuf), "%d", cache.tts);
    setLabelText(poTtsValue, ttsBuf);
  } else {
    setLabelText(poTtsValue, "--");
  }

  int ceilD = cache.ceilD;
  bool belowCeiling = ceilD > 0 && state.depth < ceilD;
  char decoVal[24];
  lv_color_t decoColor;
  if (ceilD == 0) {
    if (cache.ndl >= 0) snprintf(decoVal, sizeof(decoVal), "NDL %d min", cache.ndl);
    else snprintf(decoVal, sizeof(decoVal), "NDL 240+");
    decoColor = colGreen();
  } else {
    if (cache.stopRemain >= 0) snprintf(decoVal, sizeof(decoVal), "STOP %dm  %d min", ceilD, cache.stopRemain);
    else snprintf(decoVal, sizeof(decoVal), "STOP %dm  240+", ceilD);
    decoColor = belowCeiling ? colRed() : colYellow();
  }
  setLabelText(poDecoValue, decoVal);
  setLabelColor(poDecoValue, decoColor);

  // ---- bottom banner: auto-bailout > ceiling violation > PO2 alert > edit hint ----
  // Shares its row with poStatsLine (only one of the two is visible at a
  // time) -- there isn't enough safe width this close to the bottom pole
  // for a second row, and an active alert/edit-hint is more important than
  // the secondary TTS/dive-time/max-depth stats anyway.
  char ceilingMsg[40];
  const char *alertText = nullptr;
  bool alertIsCaution = false;
  if (autoBanner) {
    alertText = autoBanner;
  } else if (belowCeiling) {
    // belowCeiling means state.depth < ceilD -- shallower than the deco
    // ceiling requires, i.e. ascended too far -- so the fix is to descend
    // back down to it, not "go up" (that was backwards).
    snprintf(ceilingMsg, sizeof(ceilingMsg), "CEILING %dm DESCEND NOW", ceilD);
    alertText = ceilingMsg;
  } else if (ppo2AlertText) {
    alertText = ppo2AlertText;
    alertIsCaution = ppo2AlertIsCaution;
  }

  if (alertText) {
    setBgColor(poBanner, alertIsCaution ? colYellow() : colRed());
    setBgOpa(poBanner, LV_OPA_COVER);
    setLabelColor(poBannerLabel, lv_color_hex(0x170403));
    setLabelText(poBannerLabel, alertText);
    lv_obj_remove_flag(poBanner, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(poStatsLine, LV_OBJ_FLAG_HIDDEN);
  } else if (editingSp || editingGas) {
    setBgColor(poBanner, colCyan());
    setBgOpa(poBanner, LV_OPA_20);
    setLabelColor(poBannerLabel, colCyan());
    setLabelText(poBannerLabel, editingGas ? "MENU=next gas  ACTION=ok" : "MENU=next  ACTION=ok");
    lv_obj_remove_flag(poBanner, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(poStatsLine, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(poBanner, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(poStatsLine, LV_OBJ_FLAG_HIDDEN);

    char statsBuf[32];
    snprintf(statsBuf, sizeof(statsBuf), "%d min  MAX %.1fm", (int)state.diveTime, state.maxDepth);
    setLabelText(poStatsLine, statsBuf);
  }
}

} // namespace dc
