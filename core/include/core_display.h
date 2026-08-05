#ifndef CORE_DISPLAY_H
#define CORE_DISPLAY_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Convert HSV (h in [0,360), s/v in [0,100]) to RGB (0-255).
 */
void core_display_hsv2rgb(uint32_t h, uint32_t s, uint32_t v,
                          uint32_t *r, uint32_t *g, uint32_t *b);

/**
 * @brief Light one LED at `position * 10 + digit` in a GRB pixel buffer.
 * Applies brightness scaling (0-100) and bounds-checks against @p num_leds;
 * out-of-range writes are silently ignored (prevents out-of-bounds bugs).
 */
void core_display_set_digit(uint8_t *pixels, int num_leds, int position,
                            int digit, uint8_t r, uint8_t g, uint8_t b,
                            uint8_t brightness);

/**
 * @brief Digit value (0-9) at @p position for the given h/m/s.
 * Positions >= 6 (extra leading digits) return 0.
 */
uint8_t core_display_digit_value(int position, int hour, int minute, int second);

#ifdef __cplusplus
}
#endif
#endif /* CORE_DISPLAY_H */
