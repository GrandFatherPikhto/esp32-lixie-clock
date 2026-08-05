#ifndef CORE_CONFIG_H
#define CORE_CONFIG_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Buffer sizes for the string config keys. */
#define CORE_CFG_NTP_SERVER_LEN   64
#define CORE_CFG_SSID_LEN         32
#define CORE_CFG_PASSWORD_LEN     64

/* Numeric ranges enforced by core_config_set_value(). */
#define CORE_CFG_SYNC_INTERVAL_MIN   30
#define CORE_CFG_SYNC_INTERVAL_MAX   604800
#define CORE_CFG_TZ_OFFSET_MIN       (-840)
#define CORE_CFG_TZ_OFFSET_MAX        840
#define CORE_CFG_BRIGHTNESS_MAX       100
#define CORE_CFG_GPIO_MAX             39
#define CORE_CFG_HUE_MAX              359
/* color_mode: 0=Garland(orig), 1=Mono, 2=Triad, 3=Spectrum, 4=Prism, 5=Chronos */
#define CORE_CFG_COLOR_MODE_MAX       5
/* Night mode: master switch + window/brightness ranges. */
#define CORE_CFG_NIGHT_MODE_MAX           1
#define CORE_CFG_NIGHT_BRIGHTNESS_MAX     100
#define CORE_CFG_NIGHT_HOUR_MAX           23
/* Cross-fade + slot machine: display behaviour switches. */
#define CORE_CFG_CROSS_FADE_MAX           2000
#define CORE_CFG_SLOT_INTERVAL_MIN        0
#define CORE_CFG_SLOT_INTERVAL_MAX        1440

/* Return codes for core_config_set_value(). */
#define CORE_CFG_OK             0
#define CORE_CFG_ERR_INVALID   (-1)   /* known key, bad value */
#define CORE_CFG_ERR_UNKNOWN   (-2)   /* unknown key */

/**
 * Runtime configuration snapshot. Pure data with no ESP-IDF dependency, so the
 * same structure is used by the firmware (as the NVS blob) and by the host
 * unit tests in tests/c/.
 */
typedef struct {
    char     ntp_server[CORE_CFG_NTP_SERVER_LEN];
    char     ssid[CORE_CFG_SSID_LEN];
    char     password[CORE_CFG_PASSWORD_LEN];
    uint32_t sync_interval_sec;
    int32_t  tz_offset_min;
    uint8_t  brightness;    /* 0..100 */
    uint8_t  digits;        /* 1..max_digits */
    int8_t   gpio;
    uint8_t  color_mode;    /* 0..CORE_CFG_COLOR_MODE_MAX (see header) */
    uint16_t hue;           /* 0..359 */
    uint8_t  sync_method;   /* 0=immediate, 1=smooth */
    uint8_t  breathing;     /* 0=off, 1=pulsing brightness */
    uint8_t  night_mode;            /* 0=off, 1=night dimming enabled */
    uint8_t  night_low_brightness;  /* 0..100, brightness used during night hours */
    uint8_t  night_start;           /* 0..23, night window start hour */
    uint8_t  night_end;             /* 0..23, night window end hour (exclusive) */
    uint16_t cross_fade;            /* fade duration in ms; 0 = off (instant) */
    uint32_t slot_machine_interval; /* 0=off, else minutes between slot rolls */
} clock_config_t;

/**
 * @brief Set cfg[key] from a string value with type conversion + range checks.
 * @param max_digits Upper bound for the `digits` key (pass 0 to skip).
 * @return CORE_CFG_OK, CORE_CFG_ERR_INVALID (bad value) or
 *         CORE_CFG_ERR_UNKNOWN (unknown key).
 */
int core_config_set_value(clock_config_t *cfg, const char *key,
                          const char *value, int max_digits);

/** @brief Copy a string into a fixed buffer; always NUL-terminates. */
void core_config_copy_str(char *dst, size_t dst_size, const char *src);

/** @brief Strict decimal parse (whole string consumed). 0 on success. */
int core_config_parse_int(const char *s, long *out);

/** @brief Split "key=value" into separate buffers. 0 on success. */
int core_config_split_kv(const char *arg, char *key, size_t key_size,
                         char *value, size_t value_size);

#ifdef __cplusplus
}
#endif
#endif /* CORE_CONFIG_H */
