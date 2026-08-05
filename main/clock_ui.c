#include "clock_ui.h"
#include "app_config.h"
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "esp_log.h"

static const char *TAG = "clock_ui";

/* Number of color "pairs". With CLOCK_DIGITS_MAX digits we need at most
 * (MAX + 1) / 2 color phases (an odd last digit reuses the previous pair). */
#define COLOR_PAIRS ((CLOCK_DIGITS_MAX + 1) / 2)

static uint16_t color_phase[COLOR_PAIRS];   /* rotation state per digit pair */
static int anim_step = 0;

/* ---------------------------------------------------------------------------
 * Helpers
 * ------------------------------------------------------------------------- */

/**
 * @brief HSV -> RGB (0-255).
 */
static void hsv2rgb(uint32_t h, uint32_t s, uint32_t v,
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

/**
 * @brief Light one LED (position * 10 + digit) applying brightness (0-100).
 * @p pixels must be CLOCK_LED_NUMBERS_MAX * 3 bytes (GRB order for WS2812).
 */
static void set_clock_digit(uint8_t *pixels, int position, int digit,
                            uint8_t r, uint8_t g, uint8_t b, uint8_t brightness)
{
    int index = position * 10 + digit;
    if (index < 0 || index >= CLOCK_LED_NUMBERS_MAX) {
        return;   /* bounds-checked: safe for any runtime digit count */
    }
    uint8_t R = (uint8_t)(((uint32_t)r * brightness) / 100);
    uint8_t G = (uint8_t)(((uint32_t)g * brightness) / 100);
    uint8_t B = (uint8_t)(((uint32_t)b * brightness) / 100);
    pixels[index * 3 + 0] = G;   /* WS2812 expects GRB */
    pixels[index * 3 + 1] = R;
    pixels[index * 3 + 2] = B;
}

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
    memset(color_phase, 0, sizeof(color_phase));
    anim_step = 0;
    ESP_LOGI(TAG, "Clock UI initialized (timezone applied)");
}

void clock_ui_fill_time(uint8_t *pixels)
{
    const int digits_active = active_digits();

    memset(pixels, 0, CLOCK_LED_NUMBERS_MAX * 3);

    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    /* Digit values: position 0 = tens of hours ... position 5 = ones of seconds */
    uint8_t vals[6];
    vals[0] = timeinfo.tm_hour / 10;
    vals[1] = timeinfo.tm_hour % 10;
    vals[2] = timeinfo.tm_min / 10;
    vals[3] = timeinfo.tm_min % 10;
    vals[4] = timeinfo.tm_sec / 10;
    vals[5] = timeinfo.tm_sec % 10;

    const uint8_t  brightness  = app_config_get_brightness();
    const uint8_t  color_mode  = app_config_get_color_mode();
    const uint16_t fixed_hue   = app_config_get_hue();

    for (int i = 0; i < digits_active; i++) {
        uint8_t d = (i < 6) ? vals[i] : 0;   /* extra leading digits show 0 */
        uint16_t hue = (color_mode == 1) ? fixed_hue : (color_phase[i / 2] % 360);

        uint32_t r, g, b;
        hsv2rgb(hue, 100, 100, &r, &g, &b);
        set_clock_digit(pixels, i, d, (uint8_t)r, (uint8_t)g, (uint8_t)b, brightness);
    }

    /* Advance the hue of every active pair once per frame */
    if (color_mode == 0) {
        int pairs = (digits_active + 1) / 2;
        if (pairs > COLOR_PAIRS) pairs = COLOR_PAIRS;
        for (int p = 0; p < pairs; p++) {
            color_phase[p]++;
        }
    }
}

void clock_ui_fill_animation(uint8_t *pixels)
{
    const int digits_active = active_digits();
    uint8_t digit = (uint8_t)(anim_step % 10);
    uint16_t hue = (uint16_t)((anim_step * 36) % 360);

    uint32_t r, g, b;
    hsv2rgb(hue, 100, 100, &r, &g, &b);

    memset(pixels, 0, CLOCK_LED_NUMBERS_MAX * 3);
    for (int pos = 0; pos < digits_active; pos++) {
        set_clock_digit(pixels, pos, digit, (uint8_t)r, (uint8_t)g, (uint8_t)b,
                        app_config_get_brightness());
    }
    anim_step++;
}
