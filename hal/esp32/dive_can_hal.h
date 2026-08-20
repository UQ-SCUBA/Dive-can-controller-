#pragma once

// ESP32 TWAI (CAN controller peripheral) transport for the DiveCAN bus --
// receives raw frames and hands them to dc::onDiveCanFrame() (src/dive_can.h)
// for decoding/state updates. `round`-only for now (see the pin comment
// below); not wired into the `cyd` build.

void diveCanHalInit();
void diveCanHalPoll();
