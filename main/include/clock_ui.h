#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the UI module: apply the configured timezone and reset the
 * renderer state (palette phases, cross-fade, slot machine).
 */
void clock_ui_init(void);

/**
 * @brief Render one animation frame into @p pixels (GRB,
 * CLOCK_LED_NUMBERS_MAX*3 bytes).
 *
 * Stateful renderer driven by a hardware timer (~33 fps). Before the time is
 * synced it shows the boot animation (digits 0-9 chase with rotating colour);
 * afterwards it shows the current time. When `cross_fade` > 0, digit changes
 * fade over that many milliseconds (0 = instant); when `slot_machine_interval`
 * > 0, a periodic 0-9 "slot machine" roll plays across all digits. All
 * animation speeds are time-based, so the frame rate does not affect them.
 * Night-mode dimming is applied once the time is known.
 *
 * @param pixels      Output pixel buffer (GRB, CLOCK_LED_NUMBERS_MAX*3 bytes).
 * @param time_synced Whether the NTP time is synced (boot animation vs clock).
 */
void clock_ui_frame(uint8_t *pixels, bool time_synced);

#ifdef __cplusplus
}
#endif
