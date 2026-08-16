#include <unity.h>
#include "lvgl.h"
#include "state.h"
#include "deco.h"
#include "gestures.h"

// Covers the CC-loop <-> bailout/OC setpoint behavior: S.P. should blank to
// "--" (spDisplayBlanked(), see deco.h) the moment the effective source
// becomes bailout -- manual or the automatic 3-cell-fail case -- and the
// underlying state.spMode must survive the round trip untouched, so the
// diver gets back exactly what they had dialed in before bailing out.

using namespace dc;

static void resetState() {
  state = State();
  stateInit();
}

void setUp(void) { resetState(); }
void tearDown(void) {}

// ---- setpoint blanking on bailout ----

void test_loop_mode_shows_setpoint(void) {
  TEST_ASSERT_FALSE(getEffectiveSource().isBailout);
  TEST_ASSERT_FALSE(spDisplayBlanked(false));
}

void test_manual_bailout_blanks_setpoint(void) {
  state.spMode = SpMode::Sp12;
  setSource(Source::Bailout);
  TEST_ASSERT_TRUE(getEffectiveSource().isBailout);
  TEST_ASSERT_TRUE(spDisplayBlanked(false));
  // bailing out must not touch the underlying setpoint itself
  TEST_ASSERT_EQUAL_INT((int)SpMode::Sp12, (int)state.spMode);
}

void test_setpoint_restored_after_returning_to_loop(void) {
  state.spMode = SpMode::Sp10;
  setSource(Source::Bailout);
  TEST_ASSERT_TRUE(spDisplayBlanked(false));
  setSource(Source::Loop);
  TEST_ASSERT_FALSE(spDisplayBlanked(false));
  TEST_ASSERT_EQUAL_INT((int)SpMode::Sp10, (int)state.spMode);
}

void test_setpoint_edit_before_bailout_survives_the_round_trip(void) {
  // Diver dials in 0.5 via the S.P. edit UI...
  state.uiMode = UiMode::SpEdit;
  state.spCursor = SpMode::Sp05;
  pressAction();
  TEST_ASSERT_EQUAL_INT((int)SpMode::Sp05, (int)state.spMode);

  // ...then bails out...
  setSource(Source::Bailout);
  TEST_ASSERT_TRUE(spDisplayBlanked(false));

  // ...and back onto the loop: same 0.5 they set before bailing, not a
  // default or a stale edit-cursor value.
  setSource(Source::Loop);
  TEST_ASSERT_FALSE(spDisplayBlanked(false));
  TEST_ASSERT_EQUAL_INT((int)SpMode::Sp05, (int)state.spMode);
}

void test_auto_bailout_on_triple_cell_failure_also_blanks_setpoint(void) {
  // No manual bailout here -- state.source stays Loop -- but with all 3
  // cells failed, getEffectiveSource() auto-bails, and S.P. should blank
  // exactly like the manual case: the loop's solenoid isn't driving to a
  // target either way.
  state.spMode = SpMode::Sp07;
  state.cells[0] = CellState::Fail;
  state.cells[1] = CellState::Fail;
  state.cells[2] = CellState::Fail;

  TEST_ASSERT_TRUE(state.source == Source::Loop);
  EffectiveSource eff = getEffectiveSource();
  TEST_ASSERT_TRUE(eff.isBailout);
  TEST_ASSERT_TRUE(eff.isAuto);
  TEST_ASSERT_TRUE(spDisplayBlanked(false));
  TEST_ASSERT_EQUAL_INT((int)SpMode::Sp07, (int)state.spMode);
}

void test_one_failed_cell_does_not_trigger_auto_bailout(void) {
  state.cells[0] = CellState::Fail;
  state.cells[1] = CellState::Ok;
  state.cells[2] = CellState::Voted;
  TEST_ASSERT_FALSE(getEffectiveSource().isBailout);
  TEST_ASSERT_FALSE(spDisplayBlanked(false));
}

void test_editing_setpoint_overrides_blank_even_mid_bailout(void) {
  // spDisplayBlanked() takes editingSp as an explicit argument rather than
  // reading state.uiMode itself, so this holds regardless of whether SpEdit
  // is actually reachable from bailout via the menu button.
  setSource(Source::Bailout);
  TEST_ASSERT_TRUE(getEffectiveSource().isBailout);
  TEST_ASSERT_FALSE(spDisplayBlanked(/*editingSp=*/true));
  TEST_ASSERT_TRUE(spDisplayBlanked(/*editingSp=*/false));
}

void test_setpoint_blanked_even_with_no_safe_bailout_gas(void) {
  // Disable every bailout candidate -- getEffectiveSource() reports isBailout
  // true with a null gas in this case (see its own comment on that), and
  // S.P. blanking has nothing to do with gas availability, so it should stay
  // blanked too.
  state.gasMixes[0].enabled = false;
  state.gasMixes[1].enabled = false;
  state.gasMixes[2].enabled = false;
  state.diluent.enabled = false;
  setSource(Source::Bailout);
  EffectiveSource eff = getEffectiveSource();
  TEST_ASSERT_TRUE(eff.isBailout);
  TEST_ASSERT_NULL(eff.gas);
  TEST_ASSERT_TRUE(spDisplayBlanked(false));
}

// ---- pressMenu() gating around the bailout transition ----

void test_menu_opens_gas_select_not_sp_edit_while_bailed_out(void) {
  setSource(Source::Bailout);
  state.uiMode = UiMode::Dive;
  pressMenu();
  TEST_ASSERT_EQUAL_INT((int)UiMode::GasSelect, (int)state.uiMode);
}

void test_menu_opens_sp_edit_while_on_loop(void) {
  state.uiMode = UiMode::Dive;
  pressMenu();
  TEST_ASSERT_EQUAL_INT((int)UiMode::SpEdit, (int)state.uiMode);
}

// ---- setSource()'s gas-pinning edge cases, which feed the "OC . <gas>"
// source label shown alongside the now-blanked S.P. ----

void test_bailout_pins_richest_safe_gas_at_the_moment_of_transition(void) {
  state.depth = 0.0f; // surface: MIX 3 (100% O2) is safe and richest here
  setSource(Source::Bailout);
  TEST_ASSERT_EQUAL_INT(2, state.bailoutGasIdx); // gasMixes[2] == "MIX 3"
}

void test_staying_in_bailout_does_not_repin_gas(void) {
  state.depth = 0.0f;
  setSource(Source::Bailout);
  TEST_ASSERT_EQUAL_INT(2, state.bailoutGasIdx);
  state.bailoutGasIdx = 0; // diver manually picks MIX 1 via GasSelect
  setSource(Source::Bailout); // already bailed out -- reaffirming shouldn't re-pick
  TEST_ASSERT_EQUAL_INT(0, state.bailoutGasIdx);
}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  lv_init(); // pressMenu()/pressAction() call lv_tick_get() internally
  UNITY_BEGIN();
  RUN_TEST(test_loop_mode_shows_setpoint);
  RUN_TEST(test_manual_bailout_blanks_setpoint);
  RUN_TEST(test_setpoint_restored_after_returning_to_loop);
  RUN_TEST(test_setpoint_edit_before_bailout_survives_the_round_trip);
  RUN_TEST(test_auto_bailout_on_triple_cell_failure_also_blanks_setpoint);
  RUN_TEST(test_one_failed_cell_does_not_trigger_auto_bailout);
  RUN_TEST(test_editing_setpoint_overrides_blank_even_mid_bailout);
  RUN_TEST(test_setpoint_blanked_even_with_no_safe_bailout_gas);
  RUN_TEST(test_menu_opens_gas_select_not_sp_edit_while_bailed_out);
  RUN_TEST(test_menu_opens_sp_edit_while_on_loop);
  RUN_TEST(test_bailout_pins_richest_safe_gas_at_the_moment_of_transition);
  RUN_TEST(test_staying_in_bailout_does_not_repin_gas);
  return UNITY_END();
}
