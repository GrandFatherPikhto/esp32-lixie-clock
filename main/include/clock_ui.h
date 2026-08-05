#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the UI module: apply the configured timezone, reset
 * animation/color state.
 */
void clock_ui_init(void);

/**
 * @brief Render the current time into @p pixels (GRB, CLOCK_LED_NUMBERS_MAX*3
 * bytes). Uses the runtime digit count, brightness and color settings.
 */
void clock_ui_fill_time(uint8_t *pixels);

/**
 * @brief Render the boot animation (digits 0-9 chase with rotating color) into
 * @p pixels, honoring the runtime digit count and brightness.
 */
void clock_ui_fill_animation(uint8_t *pixels);

#ifdef __cplusplus
}
#endif
