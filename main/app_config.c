#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "app_config.h"

static const char *TAG = "app_config";

#define NVS_NAMESPACE "clock"
#define NVS_KEY       "cfg"

/* Persistent configuration blob (kept compact and stable) */
typedef struct {
    char     ntp_server[64];
    char     ssid[32];
    char     password[64];
    uint32_t sync_interval_sec;
    int32_t  tz_offset_min;
    uint8_t  brightness;    /* 0..100 */
    uint8_t  digits;        /* 1..CLOCK_DIGITS_MAX */
    int8_t   gpio;
    uint8_t  color_mode;    /* 0=rotate, 1=fixed */
    uint16_t hue;           /* 0..359 */
    uint8_t  sync_method;   /* 0=immediate, 1=smooth */
    uint8_t  _pad[3];
} app_config_t;

static app_config_t cfg;

/* ---------------------------------------------------------------------------
 * Defaults (seeded from Kconfig)
 * ------------------------------------------------------------------------- */

static void set_defaults(void)
{
    memset(&cfg, 0, sizeof(cfg));
    strlcpy(cfg.ntp_server, CONFIG_SNTP_TIME_SERVER, sizeof(cfg.ntp_server));
    cfg.sync_interval_sec = CONFIG_SNTP_SYNC_INTERVAL_PERIOD;
    cfg.tz_offset_min     = CONFIG_TIMEZONE_OFFSET_MINUTES;
    cfg.brightness        = CONFIG_CLOCK_BRIGHTNESS_DEFAULT;
    cfg.digits            = CONFIG_NUM_DIGITS;
    cfg.gpio              = CONFIG_LED_GPIO;
    cfg.color_mode        = CONFIG_COLOR_MODE_DEFAULT;
    cfg.hue               = CONFIG_COLOR_HUE_DEFAULT;
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
    /* POSIX TZ encodes "local = UTC - offset", so the sign is inverted:
     * UTC+3 (Moscow) becomes "UTC-3", UTC-5 becomes "UTC+5". */
    char tz[40];
    char sign = (cfg.tz_offset_min < 0) ? '+' : '-';
    unsigned abs_min = (cfg.tz_offset_min < 0) ? (unsigned)(-cfg.tz_offset_min)
                                               : (unsigned)cfg.tz_offset_min;
    unsigned hh = abs_min / 60;
    unsigned mm = abs_min % 60;
    if (mm) {
        snprintf(tz, sizeof(tz), "UTC%c%u:%02u", sign, hh, mm);
    } else {
        snprintf(tz, sizeof(tz), "UTC%c%u", sign, hh);
    }
    setenv("TZ", tz, 1);
    tzset();
    ESP_LOGI(TAG, "Timezone applied: %s (offset %ld min)", tz, (long)cfg.tz_offset_min);
}

esp_err_t app_config_set(const char *key, const char *value)
{
    if (key == NULL || value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    long v;
    if (strcmp(key, "ntp_server") == 0) {
        strlcpy(cfg.ntp_server, value, sizeof(cfg.ntp_server));
    } else if (strcmp(key, "ssid") == 0) {
        strlcpy(cfg.ssid, value, sizeof(cfg.ssid));
    } else if (strcmp(key, "password") == 0) {
        strlcpy(cfg.password, value, sizeof(cfg.password));
    } else if (strcmp(key, "sync_interval") == 0) {
        v = strtol(value, NULL, 10);
        if (v < 30 || v > 604800) return ESP_ERR_INVALID_ARG;
        cfg.sync_interval_sec = (uint32_t)v;
    } else if (strcmp(key, "tz_offset") == 0) {
        v = strtol(value, NULL, 10);
        if (v < -840 || v > 840) return ESP_ERR_INVALID_ARG;
        cfg.tz_offset_min = (int32_t)v;
    } else if (strcmp(key, "brightness") == 0) {
        v = strtol(value, NULL, 10);
        if (v < 0 || v > 100) return ESP_ERR_INVALID_ARG;
        cfg.brightness = (uint8_t)v;
    } else if (strcmp(key, "digits") == 0) {
        v = strtol(value, NULL, 10);
        if (v < 1 || v > CLOCK_DIGITS_MAX) return ESP_ERR_INVALID_ARG;
        cfg.digits = (uint8_t)v;
    } else if (strcmp(key, "gpio") == 0) {
        v = strtol(value, NULL, 10);
        if (v < 0 || v > 39) return ESP_ERR_INVALID_ARG;
        cfg.gpio = (int8_t)v;
    } else if (strcmp(key, "color_mode") == 0) {
        v = strtol(value, NULL, 10);
        if (v != 0 && v != 1) return ESP_ERR_INVALID_ARG;
        cfg.color_mode = (uint8_t)v;
    } else if (strcmp(key, "hue") == 0) {
        v = strtol(value, NULL, 10);
        if (v < 0 || v > 359) return ESP_ERR_INVALID_ARG;
        cfg.hue = (uint16_t)v;
    } else if (strcmp(key, "sync_method") == 0) {
        v = strtol(value, NULL, 10);
        if (v != 0 && v != 1) return ESP_ERR_INVALID_ARG;
        cfg.sync_method = (uint8_t)v;
    } else {
        return ESP_ERR_NOT_FOUND;
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
