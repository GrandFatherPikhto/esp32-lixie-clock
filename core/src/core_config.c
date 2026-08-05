#include <string.h>
#include <stdlib.h>
#include "core_config.h"

void core_config_copy_str(char *dst, size_t dst_size, const char *src)
{
    if (dst == NULL || dst_size == 0) {
        return;
    }
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }
    size_t n = strlen(src);
    if (n >= dst_size) {
        n = dst_size - 1;
    }
    memcpy(dst, src, n);
    dst[n] = '\0';
}

int core_config_parse_int(const char *s, long *out)
{
    if (s == NULL || out == NULL || *s == '\0') {
        return -1;
    }
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (end == s || *end != '\0') {
        return -1;   /* no digits, or trailing junk */
    }
    *out = v;
    return 0;
}

int core_config_split_kv(const char *arg, char *key, size_t key_size,
                         char *value, size_t value_size)
{
    if (arg == NULL || key == NULL || value == NULL ||
        key_size == 0 || value_size == 0) {
        return -1;
    }
    const char *eq = strchr(arg, '=');
    if (eq == NULL || eq == arg) {
        return -1;   /* missing '=' or empty key */
    }
    size_t klen = (size_t)(eq - arg);
    if (klen >= key_size) {
        klen = key_size - 1;
    }
    memcpy(key, arg, klen);
    key[klen] = '\0';
    core_config_copy_str(value, value_size, eq + 1);
    return 0;
}

int core_config_set_value(clock_config_t *cfg, const char *key,
                          const char *value, int max_digits)
{
    if (cfg == NULL || key == NULL || value == NULL) {
        return CORE_CFG_ERR_INVALID;
    }
    long v;

    if (strcmp(key, "ntp_server") == 0) {
        core_config_copy_str(cfg->ntp_server, sizeof(cfg->ntp_server), value);
    } else if (strcmp(key, "ssid") == 0) {
        core_config_copy_str(cfg->ssid, sizeof(cfg->ssid), value);
    } else if (strcmp(key, "password") == 0) {
        core_config_copy_str(cfg->password, sizeof(cfg->password), value);
    } else if (strcmp(key, "sync_interval") == 0) {
        if (core_config_parse_int(value, &v) != 0 ||
            v < CORE_CFG_SYNC_INTERVAL_MIN || v > CORE_CFG_SYNC_INTERVAL_MAX) {
            return CORE_CFG_ERR_INVALID;
        }
        cfg->sync_interval_sec = (uint32_t)v;
    } else if (strcmp(key, "tz_offset") == 0) {
        if (core_config_parse_int(value, &v) != 0 ||
            v < CORE_CFG_TZ_OFFSET_MIN || v > CORE_CFG_TZ_OFFSET_MAX) {
            return CORE_CFG_ERR_INVALID;
        }
        cfg->tz_offset_min = (int32_t)v;
    } else if (strcmp(key, "brightness") == 0) {
        if (core_config_parse_int(value, &v) != 0 ||
            v < 0 || v > CORE_CFG_BRIGHTNESS_MAX) {
            return CORE_CFG_ERR_INVALID;
        }
        cfg->brightness = (uint8_t)v;
    } else if (strcmp(key, "digits") == 0) {
        if (core_config_parse_int(value, &v) != 0 ||
            v < 1 || (max_digits > 0 && v > max_digits)) {
            return CORE_CFG_ERR_INVALID;
        }
        cfg->digits = (uint8_t)v;
    } else if (strcmp(key, "gpio") == 0) {
        if (core_config_parse_int(value, &v) != 0 ||
            v < 0 || v > CORE_CFG_GPIO_MAX) {
            return CORE_CFG_ERR_INVALID;
        }
        cfg->gpio = (int8_t)v;
    } else if (strcmp(key, "color_mode") == 0) {
        if (core_config_parse_int(value, &v) != 0 ||
            v < 0 || v > CORE_CFG_COLOR_MODE_MAX) {
            return CORE_CFG_ERR_INVALID;
        }
        cfg->color_mode = (uint8_t)v;
    } else if (strcmp(key, "hue") == 0) {
        if (core_config_parse_int(value, &v) != 0 ||
            v < 0 || v > CORE_CFG_HUE_MAX) {
            return CORE_CFG_ERR_INVALID;
        }
        cfg->hue = (uint16_t)v;
    } else if (strcmp(key, "sync_method") == 0) {
        if (core_config_parse_int(value, &v) != 0 || (v != 0 && v != 1)) {
            return CORE_CFG_ERR_INVALID;
        }
        cfg->sync_method = (uint8_t)v;
    } else if (strcmp(key, "breathing") == 0) {
        if (core_config_parse_int(value, &v) != 0 || (v != 0 && v != 1)) {
            return CORE_CFG_ERR_INVALID;
        }
        cfg->breathing = (uint8_t)v;
    } else if (strcmp(key, "night_mode") == 0) {
        if (core_config_parse_int(value, &v) != 0 || (v != 0 && v != 1)) {
            return CORE_CFG_ERR_INVALID;
        }
        cfg->night_mode = (uint8_t)v;
    } else if (strcmp(key, "night_low_brightness") == 0) {
        if (core_config_parse_int(value, &v) != 0 ||
            v < 0 || v > CORE_CFG_NIGHT_BRIGHTNESS_MAX) {
            return CORE_CFG_ERR_INVALID;
        }
        cfg->night_low_brightness = (uint8_t)v;
    } else if (strcmp(key, "night_start") == 0) {
        if (core_config_parse_int(value, &v) != 0 ||
            v < 0 || v > CORE_CFG_NIGHT_HOUR_MAX) {
            return CORE_CFG_ERR_INVALID;
        }
        cfg->night_start = (uint8_t)v;
    } else if (strcmp(key, "night_end") == 0) {
        if (core_config_parse_int(value, &v) != 0 ||
            v < 0 || v > CORE_CFG_NIGHT_HOUR_MAX) {
            return CORE_CFG_ERR_INVALID;
        }
        cfg->night_end = (uint8_t)v;
    } else {
        return CORE_CFG_ERR_UNKNOWN;
    }
    return CORE_CFG_OK;
}
