#include "clock_ui.h"
#include "app_config.h"
#include "core_display.h"
#include "core_time.h"
#include <string.h>
#include <time.h>
#include <math.h>
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "clock_ui";

/* Number of color "pairs". With CLOCK_DIGITS_MAX digits we need at most
 * (MAX + 1) / 2 color phases (an odd last digit reuses the previous pair). */
#define COLOR_PAIRS ((CLOCK_DIGITS_MAX + 1) / 2)

/* Breathing effect period (one full on->off->on cycle) in microseconds. */
#define BREATH_PERIOD_US (4000000)

/* Per-digit hue step for the "Prism" palette (one colour per digit position). */
#define PRISM_HUE_STEP 60

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
 * Palettes
 *
 * color_mode:
 *   0 = Garland (original) - pairs at red/green/blue, each rotating at its own pace
 *   1 = Mono          - all digits one fixed hue
 *   2 = Triad         - fixed per-pair colours (hue, hue+120, hue+240)
 *   3 = Spectrum      - rainbow sweep: one hue shared by all pairs, slowly rotating
 *   4 = Prism         - one static colour per digit position
 *   5 = Chronos       - hue derived from the actual time value (h/m/s)
 * ------------------------------------------------------------------------- */

static uint16_t palette_hue(uint8_t mode, int position, int pair,
                            int hour, int minute, int second, uint16_t fixed_hue)
{
    switch (mode) {
    case 0: /* Garland */
        return (uint16_t)(color_phase[pair] % 360);
    case 1: /* Mono */
        return fixed_hue;
    case 2: /* Triad */
        return (uint16_t)((fixed_hue + pair * 120) % 360);
    case 3: /* Spectrum */
        return (uint16_t)(color_phase[0] % 360);
    case 4: /* Prism */
        return (uint16_t)((position * PRISM_HUE_STEP) % 360);
    default: /* 5 = Chronos */
        if (pair == 0) return (uint16_t)((hour % 24) * 15);
        if (pair == 1) return (uint16_t)((minute % 60) * 6);
        return (uint16_t)((second % 60) * 6);
    }
}

/* Pulsing brightness for the "breathing" effect (combines with any palette). */
static uint8_t effective_brightness(uint8_t base)
{
    if (!app_config_get_breathing()) {
        return base;
    }
    int64_t us = esp_timer_get_time();
    double phase = (double)(us % BREATH_PERIOD_US) / BREATH_PERIOD_US
                 * 2.0 * 3.14159265358979323846;
    double factor = 0.5 + 0.5 * sin(phase);   /* 0.0 .. 1.0 */
    return (uint8_t)((uint32_t)base * factor);
}

/* ---------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------- */

void clock_ui_init(void)
{
    app_config_apply_timezone();
    /* Garland (the default palette): each pair of digits (hours, minutes,
     * seconds) starts at its own hue - red (0), green (120), blue (240) - and
     * then rotates smoothly from there. */
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

    const uint8_t  color_mode  = app_config_get_color_mode();
    const uint16_t fixed_hue   = app_config_get_hue();
    /* Night mode: dim to night_low_brightness when enabled and the local hour
     * falls inside the configured night window. Only meaningful once the time
     * is known, so it is applied here (time path), not in the boot animation. */
    uint8_t base_brightness = app_config_get_brightness();
    if (app_config_get_night_mode() &&
        core_time_is_night_hour(hour, app_config_get_night_start(),
                                app_config_get_night_end())) {
        base_brightness = app_config_get_night_low_brightness();
    }
    const uint8_t brightness = effective_brightness(base_brightness);

    for (int i = 0; i < digits_active; i++) {
        uint8_t d = core_display_digit_value(i, hour, minute, second);
        int pair = i / 2;
        uint16_t hue = palette_hue(color_mode, i, pair, hour, minute, second, fixed_hue);

        uint32_t r, g, b;
        core_display_hsv2rgb(hue, 100, 100, &r, &g, &b);
        core_display_set_digit(pixels, CLOCK_LED_NUMBERS_MAX, i, d,
                               (uint8_t)r, (uint8_t)g, (uint8_t)b, brightness);

        /* Garland: a pair's hue advances once per digit rendered in it, so
         * each pair rotates at its own pace from its own base hue. */
        if (color_mode == 0) {
            color_phase[pair]++;
        }
    }

    /* Spectrum: sweep the shared hue once per frame. */
    if (color_mode == 3) {
        color_phase[0]++;
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
                               effective_brightness(app_config_get_brightness()));
    }
    anim_step++;
}
