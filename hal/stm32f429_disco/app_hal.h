#ifndef DRIVER_H
#define DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif


void hal_setup(void);
void hal_loop(void);

// No deep-sleep support wired up on this target — see app_hal.h in
// hal/sdl2 for why this hook exists (esp32 is the only target that
// actually implements it). No-op here.
void hal_enter_sleep(void);


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*DRIVER_H*/
