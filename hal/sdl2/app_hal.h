#ifndef DRIVER_H
#define DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif


void hal_setup(void);
void hal_loop(void);

// No real low-power sleep on the desktop sim — app.cpp still blanks the
// device screen and gestures.cpp still off-gasses tissues for elapsed
// (sim-process) time on wake. This is just the hook real hardware (esp32)
// uses to actually cut power; here it's a no-op.
void hal_enter_sleep(void);


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*DRIVER_H*/
