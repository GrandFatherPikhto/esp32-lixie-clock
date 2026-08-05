#include <stdint.h>
#include <stddef.h>
#include "core_display.h"

void core_display_hsv2rgb(uint32_t h, uint32_t s, uint32_t v,
                          uint32_t *r, uint32_t *g, uint32_t *b)
{
    h %= 360;
    uint32_t rgb_max = (uint32_t)(v * 2.55f);
    uint32_t rgb_min = (uint32_t)(rgb_max * (100 - s) / 100.0f);

    uint32_t i = h / 60;
    uint32_t diff = h % 60;
    uint32_t rgb_adj = (rgb_max - rgb_min) * diff / 60;

    switch (i) {
    case 0: *r = rgb_max; *g = rgb_min + rgb_adj; *b = rgb_min; break;
    case 1: *r = rgb_max - rgb_adj; *g = rgb_max; *b = rgb_min; break;
    case 2: *r = rgb_min; *g = rgb_max; *b = rgb_min + rgb_adj; break;
    case 3: *r = rgb_min; *g = rgb_max - rgb_adj; *b = rgb_max; break;
    case 4: *r = rgb_min + rgb_adj; *g = rgb_min; *b = rgb_max; break;
    default: *r = rgb_max; *g = rgb_min; *b = rgb_max - rgb_adj; break;
    }
}

void core_display_set_digit(uint8_t *pixels, int num_leds, int position,
                            int digit, uint8_t r, uint8_t g, uint8_t b,
                            uint8_t brightness)
{
    if (pixels == NULL || num_leds <= 0) {
        return;
    }
    int index = position * 10 + digit;
    if (index < 0 || index >= num_leds) {
        return;   /* bounds-checked: safe for any runtime digit count */
    }
    uint8_t R = (uint8_t)(((uint32_t)r * brightness) / 100);
    uint8_t G = (uint8_t)(((uint32_t)g * brightness) / 100);
    uint8_t B = (uint8_t)(((uint32_t)b * brightness) / 100);
    pixels[index * 3 + 0] = G;   /* WS2812 expects GRB */
    pixels[index * 3 + 1] = R;
    pixels[index * 3 + 2] = B;
}

uint8_t core_display_digit_value(int position, int hour, int minute, int second)
{
    int values[6] = {
        hour / 10, hour % 10,
        minute / 10, minute % 10,
        second / 10, second % 10,
    };
    if (position < 0 || position >= 6) {
        return 0;
    }
    return (uint8_t)values[position];
}
