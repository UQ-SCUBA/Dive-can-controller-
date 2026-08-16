#include "ui_heartbeat.h"
#include "ui_common.h"

namespace dc {

constexpr int HEARTBEAT_CX = 300;
constexpr int HEARTBEAT_CY = 190;

// Punctuation-only spinner, advancing one frame per second -- a liveness
// cue that the UI loop is still running (distinct from the esp32 HAL's
// hardware watchdog), independent of dtRealSec's actual tick rate.
static const char *const SPINNER_FRAMES[] = {"!", "@", "#", "$", "%", "^", "&", "*"};
constexpr int NUM_SPINNER_FRAMES = 8;
constexpr float SPINNER_SEC_PER_FRAME = 1.0f;

static lv_obj_t *label;
static int frameIdx = 0;
static float secSinceFrame = 0.0f;

void uiHeartbeatCreate(lv_obj_t *parent) {
  // Offset is half the font's rough glyph box so the spinner's rotation
  // center lands on (HEARTBEAT_CX, HEARTBEAT_CY), matching where the old
  // pulsing ring/dot were centered.
  label = mkLabel(parent, HEARTBEAT_CX - 4, HEARTBEAT_CY - 9, &lv_font_montserrat_14, colWhite());
  lv_label_set_text(label, SPINNER_FRAMES[0]);
}

void uiHeartbeatUpdate(float dtRealSec) {
  secSinceFrame += dtRealSec;
  if (secSinceFrame >= SPINNER_SEC_PER_FRAME) {
    secSinceFrame -= SPINNER_SEC_PER_FRAME; // keep remainder, not reset to 0 -- avoids cadence drift under jitter
    frameIdx = (frameIdx + 1) % NUM_SPINNER_FRAMES;
    lv_label_set_text(label, SPINNER_FRAMES[frameIdx]);
  }
}

} // namespace dc
