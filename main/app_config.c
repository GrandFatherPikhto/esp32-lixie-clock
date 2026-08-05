#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "app_config.h"
#include "core_config.h"
#include "core_time.h"

static const char *TAG = "app_config";

#define NVS_NAMESPACE "clock"
#define NVS_KEY       "cfg"

/* Runtime configuration snapshot. The pure key=value parsing/validation lives
 * in core/ (core_config) so it can be unit-tested on the host without ESP-IDF;
 * this static instance is the NVS-backed blob used by the firmware. */
static clock_config_t cfg;

/* ---------------------------------------------------------------------------
 * Defaults (seeded from Kconfig)
 * ------------------------------------------------------------------------- */

static void set_defaults(void)
{
    memset(&cfg, 0, sizeof(cfg));
    core_config_copy_str(cfg.ntp_server, sizeof(cfg.ntp_server),
                         CONFIG_SNTP_TIME_SERVER);
    cfg.sync_interval_sec = CONFIG_SNTP_SYNC_INTERVAL_PERIOD;
    cfg.tz_offset_min     = CONFIG_TIMEZONE_OFFSET_MINUTES;
    cfg.brightness        = CONFIG_CLOCK_BRIGHTNESS_DEFAULT;
    cfg.digits            = CONFIG_NUM_DIGITS;
    cfg.gpio              = CONFIG_LED_GPIO;
    cfg.color_mode        = CONFIG_COLOR_MODE_DEFAULT;
    cfg.hue               = CONFIG_COLOR_HUE_DEFAULT;
    cfg.breathing         = 0;   /* breathing is off by default */
#ifdef CONFIG_SNTP_TIME_SYNC_METHOD_SMOOTH
    cfg.sync_method       = 1;
#else
    cfg.sync_method       = 0;
#endif
    /* ssid / password stay empty: they must be provided via the console */
}

/* ---------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------- */

void app_config_init(void)
{
    set_defaults();

    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) == ESP_OK) {
        size_t len = sizeof(cfg);
        if (nvs_get_blob(handle, NVS_KEY, &cfg, &len) != ESP_OK || len != sizeof(cfg)) {
            /* Missing or corrupt -> fall back to defaults */
            set_defaults();
        }
        nvs_close(handle);
    }

    app_config_apply_timezone();
    ESP_LOGI(TAG, "Config loaded (digits=%u, gpio=%d, brightness=%u%%)",
             cfg.digits, cfg.gpio, cfg.brightness);
}

void app_config_apply_timezone(void)
{
    /* POSIX TZ encodes "local = UTC - offset", so the sign is inverted;
     * core_time_format_tz() performs the conversion (unit-tested in core). */
    char tz[40];
    core_time_format_tz(cfg.tz_offset_min, tz, sizeof(tz));
    setenv("TZ", tz, 1);
    tzset();
    ESP_LOGI(TAG, "Timezone applied: %s (offset %ld min)", tz, (long)cfg.tz_offset_min);
}

esp_err_t app_config_set(const char *key, const char *value)
{
    if (key == NULL || value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    /* All parsing / range-checking lives in the host-testable core library. */
    int rc = core_config_set_value(&cfg, key, value, CLOCK_DIGITS_MAX);
    if (rc == CORE_CFG_ERR_UNKNOWN) {
        return ESP_ERR_NOT_FOUND;
    }
    if (rc == CORE_CFG_ERR_INVALID) {
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

esp_err_t app_config_save(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return err;
    }
    err = nvs_set_blob(handle, NVS_KEY, &cfg, sizeof(cfg));
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Config saved to NVS");
    } else {
        ESP_LOGE(TAG, "Failed to save config: %s", esp_err_to_name(err));
    }
    return err;
}

void app_config_reset(void)
{
    set_defaults();
    app_config_save();
    ESP_LOGI(TAG, "Config reset to defaults");
}

void app_config_dump(void)
{
    printf("ntp_server      = %s\n", cfg.ntp_server);
    printf("sync_interval   = %lu\n", (unsigned long)cfg.sync_interval_sec);
    printf("tz_offset       = %ld\n", (long)cfg.tz_offset_min);
    printf("ssid            = %s\n", cfg.ssid);
    printf("password        = %s\n", cfg.password);
    printf("brightness      = %u\n", cfg.brightness);
    printf("digits          = %u\n", cfg.digits);
    printf("gpio            = %d\n", cfg.gpio);
    printf("color_mode      = %u\n", cfg.color_mode);
    printf("hue             = %u\n", cfg.hue);
    printf("sync_method     = %u\n", cfg.sync_method);
    printf("breathing       = %u\n", cfg.breathing);
}

/* ---------------------------------------------------------------------------
 * Getters
 * ------------------------------------------------------------------------- */

const char *app_config_get_ntp_server(void)  { return cfg.ntp_server; }
uint32_t    app_config_get_sync_interval(void) { return cfg.sync_interval_sec; }
int32_t     app_config_get_tz_offset(void)   { return cfg.tz_offset_min; }
const char *app_config_get_ssid(void)        { return cfg.ssid; }
const char *app_config_get_password(void)    { return cfg.password; }
uint8_t     app_config_get_brightness(void)  { return cfg.brightness; }
uint8_t     app_config_get_digits(void)      { return cfg.digits; }
int8_t      app_config_get_gpio(void)        { return cfg.gpio; }
uint8_t     app_config_get_color_mode(void)  { return cfg.color_mode; }
uint16_t    app_config_get_hue(void)         { return cfg.hue; }
uint8_t     app_config_get_sync_method(void) { return cfg.sync_method; }
uint8_t     app_config_get_breathing(void)   { return cfg.breathing; }
