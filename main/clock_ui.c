#include "clock_ui.h"
#include "app_config.h"
#include "core_display.h"
#include "core_time.h"
#include <string.h>
#include <time.h>
#include "esp_log.h"

static const char *TAG = "clock_ui";

/* Number of color "pairs". With CLOCK_DIGITS_MAX digits we need at most
 * (MAX + 1) / 2 color phases (an odd last digit reuses the previous pair). */
#define COLOR_PAIRS ((CLOCK_DIGITS_MAX + 1) / 2)

static uint16_t color_phase[COLOR_PAIRS];   /* rotation state per digit pair */
static int anim_step = 0;

static int active_digits(void)
{
    int d = app_config_get_digits();
    if (d < 1)       d = 1;
    if (d > CLOCK_DIGITS_MAX) d = CLOCK_DIGITS_MAX;
    return d;
}

/* ---------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------- */

void clock_ui_init(void)
{
    app_config_apply_timezone();
    /* Restore the original per-pair palette: each pair of digits (hours,
     * minutes, seconds) starts at its own hue - red (0), green (120), blue
     * (240) - and then rotates smoothly from there. */
    for (int p = 0; p < COLOR_PAIRS; p++) {
        color_phase[p] = (uint16_t)(p * 120);
    }
    anim_step = 0;
    ESP_LOGI(TAG, "Clock UI initialized (timezone applied)");
}

void clock_ui_fill_time(uint8_t *pixels)
{
    const int digits_active = active_digits();

    memset(pixels, 0, CLOCK_LED_NUMBERS_MAX * 3);

    /* Local time is computed from the configured UTC offset directly (pure
     * math in core_time, unit-tested on the host) instead of localtime_r. */
    int hour = 0, minute = 0, second = 0;
    core_time_utc_to_hms((int64_t)time(NULL), app_config_get_tz_offset(),
                         &hour, &minute, &second);

    const uint8_t  brightness = app_config_get_brightness();
    const uint8_t  color_mode = app_config_get_color_mode();
    const uint16_t fixed_hue  = app_config_get_hue();

    for (int i = 0; i < digits_active; i++) {
        uint8_t d = core_display_digit_value(i, hour, minute, second);
        int pair = i / 2;
        uint16_t hue = (color_mode == 1) ? fixed_hue : (color_phase[pair] % 360);

        uint32_t r, g, b;
        core_display_hsv2rgb(hue, 100, 100, &r, &g, &b);
        core_display_set_digit(pixels, CLOCK_LED_NUMBERS_MAX, i, d,
                               (uint8_t)r, (uint8_t)g, (uint8_t)b, brightness);

        /* Original behavior: a pair's hue advances once per digit rendered in
         * it, so each pair (hours / minutes / seconds) keeps its own colour
         * and rotates at its own pace from its own base hue. */
        if (color_mode == 0) {
            color_phase[pair]++;
        }
    }
}

void clock_ui_fill_animation(uint8_t *pixels)
{
    const int digits_active = active_digits();
    uint8_t digit = (uint8_t)(anim_step % 10);
    uint16_t hue = (uint16_t)((anim_step * 36) % 360);

    uint32_t r, g, b;
    core_display_hsv2rgb(hue, 100, 100, &r, &g, &b);

    memset(pixels, 0, CLOCK_LED_NUMBERS_MAX * 3);
    for (int pos = 0; pos < digits_active; pos++) {
        core_display_set_digit(pixels, CLOCK_LED_NUMBERS_MAX, pos, digit,
                               (uint8_t)r, (uint8_t)g, (uint8_t)b,
                               app_config_get_brightness());
    }
    anim_step++;
}
