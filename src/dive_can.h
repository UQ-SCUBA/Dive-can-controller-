#pragma once
#include <cstdint>

// DiveCAN bus message parsing + bus-health tracking. Protocol reference:
// docs/DiveCAN_Protocol_Reference.md (compiled from
// github.com/QuickRecon/DiveCAN -- the same protocol Aren Leishman's
// DiveCANHead firmware, what runs on the DiveCAN Controller Jr board,
// implements). Platform-independent: the ESP32 TWAI driver (see
// hal/esp32/dive_can_hal.cpp) just hands raw received frames to
// onDiveCanFrame() below; this file owns decoding them and updating
// dc::state.

namespace dc {

// Extended (29-bit) CAN ID layout, matching the protocol reference:
//   bits 28-24: channel (fixed 0x0D for this bus)
//   bits 23-16: message type
//   bits 15-8:  params / destination device ID
//   bits 7-0:   source device ID
constexpr uint32_t DIVECAN_CHANNEL = 0x0D;
constexpr uint8_t DIVECAN_MSG_PPO2 = 0x04;
constexpr uint8_t DIVECAN_MSG_CELL_STATUS = 0xCA;

// Milliseconds without a relevant message before the bus is treated as
// lost (see isCanBusLost()). No documented broadcast rate to derive this
// from -- generous relative to a typical 10-50Hz CAN update rate, picked
// to tune once real bus timing can be observed on hardware.
constexpr uint32_t DIVECAN_LOST_TIMEOUT_MS = 2000;

// Decodes and applies one received DiveCAN frame to dc::state (per-cell
// PPO2/status, consensus) and refreshes the bus's last-seen timestamp.
// Frames on a different channel, or whose message type isn't one this
// handset consumes, are ignored (not an error -- the bus carries plenty of
// message types out of scope for v1, see the protocol reference).
void onDiveCanFrame(uint32_t extId, const uint8_t *data, uint8_t len);

// Call once from a platform HAL that actually attempts to bring up a real
// DiveCAN transport (see hal/esp32/dive_can_hal.cpp's diveCanHalInit()) --
// starts isCanBusLost() tracking staleness at all. Platforms that never
// call this (the emulator, and `cyd` for now, neither of which have a CAN
// transceiver wired) leave isCanBusLost() permanently false instead of
// permanently true, preserving the old setpoint-tracking stand-in
// (cellBaseValue()/computeConsensus()'s fallback) everywhere except real
// hardware that's actually trying to talk to a bus.
void markCanBusActive();

// True once DIVECAN_LOST_TIMEOUT_MS has passed since the last relevant
// frame -- including once markCanBusActive() has been called but no frame
// has arrived yet (a bus that's never come up counts as "lost", not
// "unknown"). Always false until markCanBusActive() is called at all. See
// deco.cpp's activeInertSource() for how this feeds tissue loading, and
// ui_dive_screen_round.cpp for the cell/PPO2 red flash it drives.
bool isCanBusLost();

} // namespace dc
