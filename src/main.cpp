#include "lvgl.h"
#include "app_hal.h"
#include "app.h"

#ifdef ARDUINO
#include <Arduino.h>

void setup() {
  lv_init();
  hal_setup();
  dc::appInit();
  hal_restore_from_sleep();
}

void loop() {
  hal_loop(); /* Do not use while loop in this function */
}

#else

#include "input_sim_debug.h"

int main(void)
{
  lv_init();

  hal_setup();

  dc::appInit();
  dc::inputSimDebugCreate();

  hal_loop();
}

#endif /*ARDUINO*/
