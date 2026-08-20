#include "dive_can.h"
#include "state.h"
#include "lvgl.h"

namespace dc {

static uint32_t lastMsgAtMs = 0; // 0 = never received
static bool busActive = false;   // set by markCanBusActive() -- see dive_can.h

void markCanBusActive() { busActive = true; }

bool isCanBusLost() {
  if (!busActive) return false; // no platform HAL is even attempting a real bus here
  if (lastMsgAtMs == 0) return true; // attempted, but never received -- lost, not "unknown"
  return (lv_tick_get() - lastMsgAtMs) > DIVECAN_LOST_TIMEOUT_MS;
}

namespace {

struct DiveCanId {
  uint8_t channel;
  uint8_t msgType;
  uint8_t params;
  uint8_t source;
};

DiveCanId decodeId(uint32_t extId) {
  DiveCanId id;
  id.channel = (uint8_t)((extId >> 24) & 0x1F);
  id.msgType = (uint8_t)((extId >> 16) & 0xFF);
  id.params = (uint8_t)((extId >> 8) & 0xFF);
  id.source = (uint8_t)(extId & 0xFF);
  return id;
}

// PPO2 -- 0xD040004-style, 4 bytes: byte0 always 0, bytes1-3 per-cell PPO2
// (*100), 0xFF = that cell's sensor has failed.
void applyPpo2(const uint8_t *data, uint8_t len) {
  if (len < 4) return;
  for (int i = 0; i < NUM_CELLS; i++) {
    uint8_t raw = data[1 + i];
    if (raw == 0xFF) {
      state.cells[i] = CellState::Fail;
    } else {
      state.cellPpo2Bar[i] = raw / 100.0f;
      // A Cell Status message can still mark this cell Voted (excluded)
      // even while its PPO2 reads fine -- only clear an existing Fail
      // here, don't stomp on Voted (applyCellStatus() below owns that
      // distinction).
      if (state.cells[i] == CellState::Fail) state.cells[i] = CellState::Ok;
    }
  }
}

// Cell Status -- 0xDCA0004-style, 2 bytes: byte0 bit-mask (bit i set = cell
// i trusted/voted in), byte1 = consensus PPO2 (*100) -- what the deco
// engine should actually use (see docs/DiveCAN_Protocol_Reference.md), not
// a client-side re-average of the three cells.
void applyCellStatus(const uint8_t *data, uint8_t len) {
  if (len < 2) return;
  for (int i = 0; i < NUM_CELLS; i++) {
    if (state.cells[i] == CellState::Fail) continue; // PPO2 message owns Fail
    bool trusted = (data[0] >> i) & 0x1;
    state.cells[i] = trusted ? CellState::Ok : CellState::Voted;
  }
  state.consensusPpo2Bar = data[1] / 100.0f;
  state.consensusPpo2Valid = true;
}

} // namespace

void onDiveCanFrame(uint32_t extId, const uint8_t *data, uint8_t len) {
  DiveCanId id = decodeId(extId);
  if (id.channel != DIVECAN_CHANNEL) return;

  switch (id.msgType) {
    case DIVECAN_MSG_PPO2:
      applyPpo2(data, len);
      break;
    case DIVECAN_MSG_CELL_STATUS:
      applyCellStatus(data, len);
      break;
    default:
      return; // not a message type this handset consumes yet -- see protocol reference
  }
  lastMsgAtMs = lv_tick_get();
}

} // namespace dc
