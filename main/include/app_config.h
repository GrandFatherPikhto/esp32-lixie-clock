#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Compile-time constants (buffer sizing / hardware defaults)
 * ------------------------------------------------------------------------- */

#define RMT_LED_STRIP_RESOLUTION_HZ  10000000   // 10 MHz

/* Upper bound of the pixel buffer. All arrays are sized to this value, and the
 * active digit count is clamped to it at runtime. Fixes out-of-bounds access
 * when the configured digit count is not 6. */
#define CLOCK_DIGITS_MAX             CONFIG_NUM_DIGITS_MAX
#define CLOCK_LED_NUMBERS_MAX        (CLOCK_DIGITS_MAX * 10)

/* ---------------------------------------------------------------------------
 * Runtime configuration API (NVS-backed, configurable over the serial console)
 *
 * Defaults come from Kconfig (CONFIG_*) and are stored in NVS after the first
 * `save`. All getters read from RAM; `app_config_set()` updates RAM and
 * `app_config_save()` persists it.
 * ------------------------------------------------------------------------- */

/** @brief Load config from NVS (or apply defaults). Call once at startup. */
void app_config_init(void);

/** @brief Apply the configured timezone to the C library (setenv TZ + tzset). */
void app_config_apply_timezone(void);

/**
 * @brief Set a single runtime parameter by name.
 * @param key   One of: ntp_server, sync_interval, tz_offset, ssid, password,
 *              brightness, digits, gpio, color_mode, hue, sync_method
 * @param value String value (numbers are parsed and range-checked).
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG on bad value,
 *         ESP_ERR_NOT_FOUND for unknown key.
 */
esp_err_t app_config_set(const char *key, const char *value);

/** @brief Persist the current config to NVS. */
esp_err_t app_config_save(void);

/** @brief Restore defaults and persist them. */
void app_config_reset(void);

/** @brief Print all config values (used by the console `get` command). */
void app_config_dump(void);

const char *app_config_get_ntp_server(void);    /* NTP hostname            */
uint32_t    app_config_get_sync_interval(void); /* seconds, >= 30          */
int32_t     app_config_get_tz_offset(void);     /* minutes from UTC        */
const char *app_config_get_ssid(void);          /* Wi-Fi SSID              */
const char *app_config_get_password(void);      /* Wi-Fi password          */
uint8_t     app_config_get_brightness(void);    /* 0..100 percent          */
uint8_t     app_config_get_digits(void);        /* 1..CLOCK_DIGITS_MAX     */
int8_t      app_config_get_gpio(void);          /* LED strip GPIO          */
uint8_t     app_config_get_color_mode(void);    /* 0=rotate, 1=fixed       */
uint16_t    app_config_get_hue(void);           /* 0..359                  */
uint8_t     app_config_get_sync_method(void);   /* 0=immediate, 1=smooth   */

#ifdef __cplusplus
}
#endif

#endif /* APP_CONFIG_H */
