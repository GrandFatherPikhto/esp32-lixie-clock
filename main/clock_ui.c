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

/* ---------------------------------------------------------------------------
 * Animation timing (microseconds).
 *
 * The renderer is driven by a hardware timer at ~33 fps, so every animation
 * step is time-gated below to keep the speeds of the original 500 ms-per-frame
 * behaviour regardless of the actual frame rate.
 * ------------------------------------------------------------------------- */
#define PALETTE_TICK_US  (500000)   /* 500 ms: classic palette/boot rotation step */
#define SLOT_STEP_US     (250000)   /* 250 ms per digit in the slot scroll */
#define SLOT_DURATION_US (2500000)  /* 2.5 s for a full 0..9 slot scroll */

static uint16_t color_phase[COLOR_PAIRS];   /* rotation state per digit pair */
static int anim_step = 0;

/* Cross-fade state, kept per digit position. */
static int     fade_prev[CLOCK_DIGITS_MAX]; /* outgoing digit, or -1 */
static int     fade_cur[CLOCK_DIGITS_MAX];  /* incoming digit, or -1 */
static int64_t fade_start[CLOCK_DIGITS_MAX];/* when the current fade began */
static bool    fade_on[CLOCK_DIGITS_MAX];

/* Slot-machine state. */
static bool    slot_active;
static int     slot_step;
static int64_t slot_until_us;
static int64_t slot_next_us;
static int64_t slot_acc_us;

/* Time-based animation accumulators. */
static int64_t last_frame_us;
static int64_t palette_acc_us;
static int64_t boot_acc_us;

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

/* Advance the rotating palette phases by one "classic" 500 ms tick. This keeps
 * the Garland/Spectrum rotation speed identical to the original per-frame
 * increments at the old 500 ms frame rate. */
static void palette_step(int digits_active)
{
    if (app_config_get_color_mode() == 0) {          /* Garland */
        for (int i = 0; i < digits_active; i++) {
            color_phase[i / 2]++;
        }
    } else if (app_config_get_color_mode() == 3) {   /* Spectrum */
        color_phase[0]++;
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
 * Slot-machine scheduling
 * ------------------------------------------------------------------------- */

static void update_slot_machine(int64_t now, int64_t dt)
{
    uint32_t interval_min = app_config_get_slot_machine_interval();

    if (!slot_active) {
        if (interval_min == 0) {
            slot_next_us = 0;
            return;
        }
        if (slot_next_us == 0) {
            slot_next_us = now + (int64_t)interval_min * 60 * 1000000;
        } else if (now >= slot_next_us) {
            slot_active = true;
            slot_step = 0;
            slot_until_us = now + SLOT_DURATION_US;
            slot_acc_us = 0;
        }
        return;
    }

    slot_acc_us += dt;
    while (slot_acc_us >= SLOT_STEP_US) {
        slot_acc_us -= SLOT_STEP_US;
        slot_step = (slot_step + 1) % 10;
    }

    if (now >= slot_until_us) {
        slot_active = false;
        slot_next_us = (interval_min == 0)
                       ? 0
                       : now + (int64_t)interval_min * 60 * 1000000;
    }
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
    for (int i = 0; i < CLOCK_DIGITS_MAX; i++) {
        fade_prev[i] = -1;
        fade_cur[i] = -1;
        fade_start[i] = 0;
        fade_on[i] = false;
    }
    slot_active = false;
    slot_step = 0;
    slot_until_us = 0;
    slot_next_us = 0;
    slot_acc_us = 0;
    last_frame_us = esp_timer_get_time();
    palette_acc_us = 0;
    boot_acc_us = 0;
    ESP_LOGI(TAG, "Clock UI initialized (timezone applied)");
}

void clock_ui_frame(uint8_t *pixels, bool time_synced)
{
    int64_t now = esp_timer_get_time();
    int64_t dt = now - last_frame_us;
    if (dt < 0)       dt = 0;
    if (dt > 1000000) dt = 1000000;   /* clamp after long stalls */
    last_frame_us = now;

    const int digits_active = active_digits();

    memset(pixels, 0, CLOCK_LED_NUMBERS_MAX * 3);

    /* --- Boot animation: rolling digits 0..9 with rotating colour -------- */
    if (!time_synced) {
        boot_acc_us += dt;
        while (boot_acc_us >= PALETTE_TICK_US) {
            boot_acc_us -= PALETTE_TICK_US;
            anim_step++;
        }
        int digit = anim_step % 10;
        uint16_t hue = (uint16_t)((anim_step * 36) % 360);
        uint32_t r, g, b;
        core_display_hsv2rgb(hue, 100, 100, &r, &g, &b);
        uint8_t brightness = effective_brightness(app_config_get_brightness());
        for (int pos = 0; pos < digits_active; pos++) {
            core_display_set_digit(pixels, CLOCK_LED_NUMBERS_MAX, pos, digit,
                                   (uint8_t)r, (uint8_t)g, (uint8_t)b, brightness);
        }
        return;
    }

    /* --- Synced: advance the palette rotation and the slot-machine timer -- */
    palette_acc_us += dt;
    while (palette_acc_us >= PALETTE_TICK_US) {
        palette_acc_us -= PALETTE_TICK_US;
        palette_step(digits_active);
    }
    update_slot_machine(now, dt);

    /* Local time is computed from the configured UTC offset directly (pure
     * math in core_time, unit-tested on the host) instead of localtime_r. */
    int hour = 0, minute = 0, second = 0;
    core_time_utc_to_hms((int64_t)time(NULL), app_config_get_tz_offset(),
                         &hour, &minute, &second);

    const uint8_t  color_mode = app_config_get_color_mode();
    const uint16_t fixed_hue  = app_config_get_hue();

    /* Night mode: dim to night_low_brightness when enabled and the local hour
     * falls inside the configured night window. Only meaningful once the time
     * is known, so it is applied here, not in the boot animation. */
    uint8_t base_brightness = app_config_get_brightness();
    if (app_config_get_night_mode() &&
        core_time_is_night_hour(hour, app_config_get_night_start(),
                                app_config_get_night_end())) {
        base_brightness = app_config_get_night_low_brightness();
    }
    uint8_t brightness = effective_brightness(base_brightness);
    /* Cross-fade duration in microseconds; 0 = disabled (instant switching). */
    uint32_t fade_us = (uint32_t)app_config_get_cross_fade() * 1000;

    for (int pos = 0; pos < digits_active; pos++) {
        int pair = pos / 2;

        int target_digit;
        uint16_t hue;
        if (slot_active) {
            target_digit = slot_step;
            hue = (uint16_t)((slot_step * 36) % 360);
        } else {
            target_digit = core_display_digit_value(pos, hour, minute, second);
            hue = palette_hue(color_mode, pos, pair, hour, minute, second, fixed_hue);
        }

        /* Cross-fade: start (or restart) a transition when the target digit
         * changes. When cross_fade is disabled, fade_on stays false and the
         * change is applied instantly on the next frame. */
        if (fade_cur[pos] != target_digit) {
            fade_prev[pos] = fade_cur[pos];
            fade_cur[pos] = target_digit;
            fade_start[pos] = now;
            fade_on[pos] = (fade_us > 0);
        }

        uint32_t r, g, b;
        core_display_hsv2rgb(hue, 100, 100, &r, &g, &b);

        /* Fade the outgoing digit out while the incoming digit fades in
         * (both live on the same 10 LEDs of the position, so they blend). */
        if (fade_on[pos] && fade_prev[pos] >= 0) {
            int64_t elapsed = now - fade_start[pos];
            if (elapsed >= (int64_t)fade_us) {
                fade_on[pos] = false;
                core_display_set_digit(pixels, CLOCK_LED_NUMBERS_MAX, pos,
                                       fade_cur[pos], (uint8_t)r, (uint8_t)g,
                                       (uint8_t)b, brightness);
                continue;
            }
            uint32_t p = (uint32_t)((elapsed * 100) / fade_us); /* 0..99 */
            uint8_t b_prev = (uint8_t)((uint32_t)brightness * (100 - p) / 100);
            uint8_t b_cur  = (uint8_t)((uint32_t)brightness * p / 100);
            core_display_set_digit(pixels, CLOCK_LED_NUMBERS_MAX, pos, fade_prev[pos],
                                   (uint8_t)r, (uint8_t)g, (uint8_t)b, b_prev);
            core_display_set_digit(pixels, CLOCK_LED_NUMBERS_MAX, pos, fade_cur[pos],
                                   (uint8_t)r, (uint8_t)g, (uint8_t)b, b_cur);
            continue;
        }

        fade_on[pos] = false;
        core_display_set_digit(pixels, CLOCK_LED_NUMBERS_MAX, pos, fade_cur[pos],
                               (uint8_t)r, (uint8_t)g, (uint8_t)b, brightness);
    }
}
