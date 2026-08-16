#ifndef APP_HAL_H
#define APP_HAL_H

#ifdef __cplusplus
extern "C" {
#endif


/**
 * This function runs once and typically includes:
 * - Setting up display drivers.
 * - Configuring LVGL display and input devices
 */
void hal_setup(void);

/**
 * This function is continuously executed and typically includes:
 * - Updating LVGL's internal state & UI.
 */
void hal_loop(void);

/**
 * Called once dc::state.sleeping flips true (see gestures.cpp's
 * hold-Action-10s gesture, driven from app.cpp's tick). Persists the
 * current tissue loading and a wake-elapsed-time reference into RTC memory
 * (survives deep sleep, unlike normal RAM), puts the display to sleep, and
 * deep-sleeps the chip — wakes on the next touch (XPT2046 IRQ). Never
 * returns.
 */
void hal_enter_sleep(void);

/**
 * Call once from setup(), after dc::appInit() has already run its default
 * stateInit(). If this boot was woken from our own hal_enter_sleep() deep
 * sleep, restores the persisted tissues and off-gasses them (surface air)
 * for however long the chip was actually powered down. No-op on a normal
 * power-on/reset.
 */
void hal_restore_from_sleep(void);


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*APP_HAL_H*/
