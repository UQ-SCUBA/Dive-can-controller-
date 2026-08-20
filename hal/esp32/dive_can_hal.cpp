#include "dive_can_hal.h"

// round-only for now: this file is picked up by every ESP32 target's
// wildcard build_src_filter (+<../hal/esp32>), including `cyd`, which has
// no CAN transceiver wired and whose GPIO4/5 usage (if any) hasn't been
// checked against the pins below. Guarding the whole file rather than just
// the calls into it keeps that assumption from leaking into an unrelated
// board -- diveCanHalInit()/diveCanHalPoll() simply don't exist there.
#if defined(DEVICE_SHAPE_ROUND)

#include "driver/twai.h"

#include "../../src/dive_can.h"

// GPIO4/GPIO5 -- ESP32's TWAI peripheral needs an external CAN transceiver
// (e.g. SN65HVD230) between these pins and the actual CANH/CANL lines; TWAI
// itself is just the controller logic. Free on this board's wiring so far
// (SPI/backlight live on 13/14/16/17/18/23, Menu/Action on 25/26 -- see
// displays/LGFX_GC9B72_360.hpp), but -- like MENU/ACTION's GPIO25/26
// originally were -- still a placeholder, not yet matched to real wiring.
constexpr gpio_num_t CAN_TX_GPIO = GPIO_NUM_4;
constexpr gpio_num_t CAN_RX_GPIO = GPIO_NUM_5;

void diveCanHalInit() {
  // Unconditional, even if the install below fails -- an attempted-but-
  // failed bus should read as "lost" (see dc::isCanBusLost()), not as the
  // no-CAN-hardware-here default the emulator/cyd leave it at.
  dc::markCanBusActive();

  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_GPIO, CAN_RX_GPIO, TWAI_MODE_NORMAL);
  twai_timing_config_t t_config = TWAI_TIMING_CONFIG_125KBITS(); // DiveCAN's bus speed -- see docs/DiveCAN_Protocol_Reference.md
  // Accept every frame and filter by message type in software
  // (dc::onDiveCanFrame()) rather than in the hardware acceptance filter --
  // ESP32's extended-ID filter can't cleanly express "either of these two
  // message types, any source/dest" in a single mask, and the bus is low
  // enough traffic (125kbps, max 9 devices per the protocol reference) that
  // software filtering costs nothing worth optimizing for.
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  // Failure here (bad pins, driver already installed) just leaves TWAI
  // uninstalled -- diveCanHalPoll()'s twai_receive() calls below then fail
  // harmlessly forever, which reads as an always-lost bus (see
  // dc::isCanBusLost()), the correct failure mode rather than a crash.
  if (twai_driver_install(&g_config, &t_config, &f_config) != ESP_OK) return;
  twai_start();
}

void diveCanHalPoll() {
  twai_message_t msg;
  // 0 ticks: never block -- called every hal_loop() iteration alongside
  // display/button servicing, not from a dedicated task.
  while (twai_receive(&msg, 0) == ESP_OK) {
    if (!msg.extd || msg.rtr) continue; // DiveCAN uses 29-bit extended data frames only
    dc::onDiveCanFrame(msg.identifier, msg.data, msg.data_length_code);
  }
}

#endif // DEVICE_SHAPE_ROUND
