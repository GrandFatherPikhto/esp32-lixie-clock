/* Host (native) unit tests for the core/ library, using Unity.
 *
 * These tests exercise the exact pure-logic functions used by the firmware
 * (core_config parsing/validation, core_time offset math, core_display digit
 * rendering) with no ESP-IDF dependencies. Build & run on Linux/WSL2 via:
 *     tests/c/run_tests.sh
 */

#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include "unity.h"
#include "core_config.h"
#include "core_time.h"
#include "core_display.h"

void setUp(void) {}
void tearDown(void) {}

/* ----------------------------- core_config ------------------------------ */

static void test_parse_int(void)
{
    long v;
    TEST_ASSERT_EQUAL_INT(0, core_config_parse_int("0", &v));
    TEST_ASSERT_EQUAL_INT(0, v);
    TEST_ASSERT_EQUAL_INT(0, core_config_parse_int("123", &v));
    TEST_ASSERT_EQUAL_INT(123, v);
    TEST_ASSERT_EQUAL_INT(0, core_config_parse_int("-5", &v));
    TEST_ASSERT_EQUAL_INT(-5, v);
    TEST_ASSERT_EQUAL_INT(0, core_config_parse_int(" 42", &v));
    TEST_ASSERT_EQUAL_INT(42, v);
    TEST_ASSERT_NOT_EQUAL(0, core_config_parse_int("12x", &v));  /* trailing junk */
    TEST_ASSERT_NOT_EQUAL(0, core_config_parse_int("", &v));
    TEST_ASSERT_NOT_EQUAL(0, core_config_parse_int("abc", &v));
    TEST_ASSERT_NOT_EQUAL(0, core_config_parse_int("1e3", &v));
}

static void test_copy_str(void)
{
    char buf[8];
    core_config_copy_str(buf, sizeof(buf), "hi");
    TEST_ASSERT_EQUAL_STRING("hi", buf);
    core_config_copy_str(buf, sizeof(buf), "1234567890");
    TEST_ASSERT_EQUAL_STRING_LEN("1234567", buf, 7);
    TEST_ASSERT_EQUAL_CHAR('\0', buf[7]);
    core_config_copy_str(buf, sizeof(buf), NULL);
    TEST_ASSERT_EQUAL_STRING("", buf);
}

static void test_set_string_keys(void)
{
    clock_config_t c;
    memset(&c, 0, sizeof(c));
    TEST_ASSERT_EQUAL_INT(CORE_CFG_OK, core_config_set_value(&c, "ssid", "MyWiFi", 6));
    TEST_ASSERT_EQUAL_STRING("MyWiFi", c.ssid);
    TEST_ASSERT_EQUAL_INT(CORE_CFG_OK, core_config_set_value(&c, "password", "s3cr3t", 6));
    TEST_ASSERT_EQUAL_STRING("s3cr3t", c.password);
    TEST_ASSERT_EQUAL_INT(CORE_CFG_OK, core_config_set_value(&c, "ntp_server", "pool.ntp.org", 6));
    TEST_ASSERT_EQUAL_STRING("pool.ntp.org", c.ntp_server);
}

static void test_set_numeric_keys(void)
{
    clock_config_t c;
    memset(&c, 0, sizeof(c));
    TEST_ASSERT_EQUAL_INT(CORE_CFG_OK, core_config_set_value(&c, "sync_interval", "3600", 6));
    TEST_ASSERT_EQUAL_UINT32(3600, c.sync_interval_sec);
    TEST_ASSERT_EQUAL_INT(CORE_CFG_OK, core_config_set_value(&c, "tz_offset", "-300", 6));
    TEST_ASSERT_EQUAL_INT(-300, c.tz_offset_min);
    TEST_ASSERT_EQUAL_INT(CORE_CFG_OK, core_config_set_value(&c, "brightness", "42", 6));
    TEST_ASSERT_EQUAL_UINT8(42, c.brightness);
    TEST_ASSERT_EQUAL_INT(CORE_CFG_OK, core_config_set_value(&c, "digits", "4", 6));
    TEST_ASSERT_EQUAL_UINT8(4, c.digits);
    TEST_ASSERT_EQUAL_INT(CORE_CFG_OK, core_config_set_value(&c, "gpio", "15", 6));
    TEST_ASSERT_EQUAL_INT(15, c.gpio);
    TEST_ASSERT_EQUAL_INT(CORE_CFG_OK, core_config_set_value(&c, "color_mode", "1", 6));
    TEST_ASSERT_EQUAL_UINT8(1, c.color_mode);
    TEST_ASSERT_EQUAL_INT(CORE_CFG_OK, core_config_set_value(&c, "hue", "200", 6));
    TEST_ASSERT_EQUAL_UINT16(200, c.hue);
    TEST_ASSERT_EQUAL_INT(CORE_CFG_OK, core_config_set_value(&c, "sync_method", "1", 6));
    TEST_ASSERT_EQUAL_UINT8(1, c.sync_method);
    TEST_ASSERT_EQUAL_INT(CORE_CFG_OK, core_config_set_value(&c, "breathing", "1", 6));
    TEST_ASSERT_EQUAL_UINT8(1, c.breathing);
}

static void test_range_validation(void)
{
    clock_config_t c;
    memset(&c, 0, sizeof(c));
    TEST_ASSERT_EQUAL_INT(CORE_CFG_ERR_INVALID, core_config_set_value(&c, "brightness", "-1", 6));
    TEST_ASSERT_EQUAL_INT(CORE_CFG_ERR_INVALID, core_config_set_value(&c, "brightness", "101", 6));
    TEST_ASSERT_EQUAL_INT(CORE_CFG_ERR_INVALID, core_config_set_value(&c, "sync_interval", "29", 6));
    TEST_ASSERT_EQUAL_INT(CORE_CFG_ERR_INVALID, core_config_set_value(&c, "sync_interval", "604801", 6));
    TEST_ASSERT_EQUAL_INT(CORE_CFG_ERR_INVALID, core_config_set_value(&c, "tz_offset", "-841", 6));
    TEST_ASSERT_EQUAL_INT(CORE_CFG_ERR_INVALID, core_config_set_value(&c, "tz_offset", "841", 6));
    TEST_ASSERT_EQUAL_INT(CORE_CFG_ERR_INVALID, core_config_set_value(&c, "color_mode", "6", 6));
    TEST_ASSERT_EQUAL_INT(CORE_CFG_ERR_INVALID, core_config_set_value(&c, "sync_method", "3", 6));
    TEST_ASSERT_EQUAL_INT(CORE_CFG_ERR_INVALID, core_config_set_value(&c, "breathing", "2", 6));
    TEST_ASSERT_EQUAL_INT(CORE_CFG_ERR_INVALID, core_config_set_value(&c, "hue", "360", 6));
    TEST_ASSERT_EQUAL_INT(CORE_CFG_ERR_INVALID, core_config_set_value(&c, "gpio", "40", 6));
    TEST_ASSERT_EQUAL_INT(CORE_CFG_ERR_INVALID, core_config_set_value(&c, "digits", "0", 6));
    TEST_ASSERT_EQUAL_INT(CORE_CFG_ERR_INVALID, core_config_set_value(&c, "digits", "7", 6));
    TEST_ASSERT_EQUAL_INT(CORE_CFG_ERR_INVALID, core_config_set_value(&c, "digits", "abc", 6));
    TEST_ASSERT_EQUAL_INT(CORE_CFG_ERR_INVALID, core_config_set_value(&c, "sync_interval", "1e3", 6));
}

static void test_range_boundaries_ok(void)
{
    clock_config_t c;
    memset(&c, 0, sizeof(c));
    TEST_ASSERT_EQUAL_INT(CORE_CFG_OK, core_config_set_value(&c, "digits", "6", 6));
    TEST_ASSERT_EQUAL_INT(CORE_CFG_OK, core_config_set_value(&c, "sync_interval", "30", 6));
    TEST_ASSERT_EQUAL_INT(CORE_CFG_OK, core_config_set_value(&c, "sync_interval", "604800", 6));
    TEST_ASSERT_EQUAL_INT(CORE_CFG_OK, core_config_set_value(&c, "tz_offset", "-840", 6));
    TEST_ASSERT_EQUAL_INT(CORE_CFG_OK, core_config_set_value(&c, "tz_offset", "840", 6));
    TEST_ASSERT_EQUAL_INT(CORE_CFG_OK, core_config_set_value(&c, "brightness", "0", 6));
    TEST_ASSERT_EQUAL_INT(CORE_CFG_OK, core_config_set_value(&c, "brightness", "100", 6));
    TEST_ASSERT_EQUAL_INT(CORE_CFG_OK, core_config_set_value(&c, "hue", "0", 6));
    TEST_ASSERT_EQUAL_INT(CORE_CFG_OK, core_config_set_value(&c, "hue", "359", 6));
    TEST_ASSERT_EQUAL_INT(CORE_CFG_OK, core_config_set_value(&c, "color_mode", "5", 6));
    TEST_ASSERT_EQUAL_INT(CORE_CFG_OK, core_config_set_value(&c, "breathing", "0", 6));
    TEST_ASSERT_EQUAL_INT(CORE_CFG_OK, core_config_set_value(&c, "breathing", "1", 6));
}

static void test_unknown_key_and_null(void)
{
    clock_config_t c;
    memset(&c, 0, sizeof(c));
    TEST_ASSERT_EQUAL_INT(CORE_CFG_ERR_UNKNOWN, core_config_set_value(&c, "bogus", "1", 6));
    TEST_ASSERT_EQUAL_INT(CORE_CFG_ERR_INVALID, core_config_set_value(&c, "ssid", NULL, 6));
    TEST_ASSERT_EQUAL_INT(CORE_CFG_ERR_INVALID, core_config_set_value(NULL, "ssid", "x", 6));
}

static void test_split_kv(void)
{
    char k[32], v[32];
    TEST_ASSERT_EQUAL_INT(0, core_config_split_kv("ssid=MyWiFi", k, sizeof(k), v, sizeof(v)));
    TEST_ASSERT_EQUAL_STRING("ssid", k);
    TEST_ASSERT_EQUAL_STRING("MyWiFi", v);
    TEST_ASSERT_EQUAL_INT(0, core_config_split_kv("key=", k, sizeof(k), v, sizeof(v)));
    TEST_ASSERT_EQUAL_STRING("key", k);
    TEST_ASSERT_EQUAL_STRING("", v);
    TEST_ASSERT_NOT_EQUAL(0, core_config_split_kv("noequals", k, sizeof(k), v, sizeof(v)));
    TEST_ASSERT_NOT_EQUAL(0, core_config_split_kv("=value", k, sizeof(k), v, sizeof(v)));
    TEST_ASSERT_NOT_EQUAL(0, core_config_split_kv("", k, sizeof(k), v, sizeof(v)));
}

/* ----------------------------- core_time -------------------------------- */

static void test_format_tz(void)
{
    char buf[32];
    core_time_format_tz(180, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("UTC-3", buf);
    core_time_format_tz(-300, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("UTC+5", buf);
    core_time_format_tz(330, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("UTC-5:30", buf);
    core_time_format_tz(0, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("UTC-0", buf);
    core_time_format_tz(-540, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("UTC+9", buf);
}

static void test_utc_to_hms(void)
{
    int h, m, s;
    TEST_ASSERT_EQUAL_INT(0, core_time_utc_to_hms(0, 0, &h, &m, &s));
    TEST_ASSERT_EQUAL_INT(0, h);
    TEST_ASSERT_EQUAL_INT(0, m);
    TEST_ASSERT_EQUAL_INT(0, s);

    /* 00:00 UTC + 3h -> 03:00 local */
    TEST_ASSERT_EQUAL_INT(0, core_time_utc_to_hms(0, 180, &h, &m, &s));
    TEST_ASSERT_EQUAL_INT(3, h);
    TEST_ASSERT_EQUAL_INT(0, m);

    /* 00:00 UTC - 5h -> previous day 19:00 local */
    TEST_ASSERT_EQUAL_INT(0, core_time_utc_to_hms(0, -300, &h, &m, &s));
    TEST_ASSERT_EQUAL_INT(19, h);
    TEST_ASSERT_EQUAL_INT(0, m);

    /* 21:45 UTC + 3h -> 00:45 local (next-day wrap) */
    TEST_ASSERT_EQUAL_INT(0, core_time_utc_to_hms(21 * 3600 + 45 * 60, 180, &h, &m, &s));
    TEST_ASSERT_EQUAL_INT(0, h);
    TEST_ASSERT_EQUAL_INT(45, m);
    TEST_ASSERT_EQUAL_INT(0, s);

    /* 12:34:56 UTC - 5h -> 07:34:56 local */
    TEST_ASSERT_EQUAL_INT(0, core_time_utc_to_hms(12 * 3600 + 34 * 60 + 56, -300, &h, &m, &s));
    TEST_ASSERT_EQUAL_INT(7, h);
    TEST_ASSERT_EQUAL_INT(34, m);
    TEST_ASSERT_EQUAL_INT(56, s);

    TEST_ASSERT_EQUAL_INT(-1, core_time_utc_to_hms(0, 0, NULL, &m, &s));
}

/* ---------------------------- core_display ------------------------------ */

static void test_digit_value(void)
{
    /* 12:34:56 -> digits 1 2 3 4 5 6 */
    TEST_ASSERT_EQUAL_UINT8(1, core_display_digit_value(0, 12, 34, 56));
    TEST_ASSERT_EQUAL_UINT8(2, core_display_digit_value(1, 12, 34, 56));
    TEST_ASSERT_EQUAL_UINT8(3, core_display_digit_value(2, 12, 34, 56));
    TEST_ASSERT_EQUAL_UINT8(4, core_display_digit_value(3, 12, 34, 56));
    TEST_ASSERT_EQUAL_UINT8(5, core_display_digit_value(4, 12, 34, 56));
    TEST_ASSERT_EQUAL_UINT8(6, core_display_digit_value(5, 12, 34, 56));
    /* leading zero for hour < 10 */
    TEST_ASSERT_EQUAL_UINT8(0, core_display_digit_value(0, 7, 5, 9));
    TEST_ASSERT_EQUAL_UINT8(7, core_display_digit_value(1, 7, 5, 9));
    /* out of range -> 0 */
    TEST_ASSERT_EQUAL_UINT8(0, core_display_digit_value(6, 12, 34, 56));
    TEST_ASSERT_EQUAL_UINT8(0, core_display_digit_value(-1, 12, 34, 56));
}

static void test_set_digit_grb_and_brightness(void)
{
    uint8_t px[60 * 3];
    memset(px, 0, sizeof(px));

    /* position 0, digit 5 -> LED index 5; white @100% -> GRB 255,255,255 */
    core_display_set_digit(px, 60, 0, 5, 255, 255, 255, 100);
    TEST_ASSERT_EQUAL_UINT8(255, px[5 * 3 + 0]);
    TEST_ASSERT_EQUAL_UINT8(255, px[5 * 3 + 1]);
    TEST_ASSERT_EQUAL_UINT8(255, px[5 * 3 + 2]);

    /* white @50% -> 127 */
    memset(px, 0, sizeof(px));
    core_display_set_digit(px, 60, 0, 5, 255, 255, 255, 50);
    TEST_ASSERT_EQUAL_UINT8(127, px[5 * 3 + 0]);

    /* pure red goes into the GRB "R" slot (index*3+1) */
    memset(px, 0, sizeof(px));
    core_display_set_digit(px, 60, 0, 5, 255, 0, 0, 100);
    TEST_ASSERT_EQUAL_UINT8(0, px[5 * 3 + 0]);
    TEST_ASSERT_EQUAL_UINT8(255, px[5 * 3 + 1]);
    TEST_ASSERT_EQUAL_UINT8(0, px[5 * 3 + 2]);
}

static void test_set_digit_bounds(void)
{
    uint8_t small[10 * 3];
    memset(small, 0xAA, sizeof(small));
    /* position 5, digit 5 -> index 55 >= 10 -> must not write */
    core_display_set_digit(small, 10, 5, 5, 255, 255, 255, 100);
    /* negative position -> index < 0 -> must not write */
    core_display_set_digit(small, 10, -1, 1, 255, 255, 255, 100);
    /* NULL buffer -> safe no-op */
    core_display_set_digit(NULL, 10, 0, 1, 255, 255, 255, 100);
    for (size_t i = 0; i < sizeof(small); i++) {
        TEST_ASSERT_EQUAL_UINT8(0xAA, small[i]);
    }
}

static void test_hsv2rgb(void)
{
    uint32_t r, g, b;
    core_display_hsv2rgb(0, 100, 100, &r, &g, &b);
    TEST_ASSERT_EQUAL_UINT32(255, r);
    TEST_ASSERT_EQUAL_UINT32(0, g);
    TEST_ASSERT_EQUAL_UINT32(0, b);

    core_display_hsv2rgb(120, 100, 100, &r, &g, &b);
    TEST_ASSERT_EQUAL_UINT32(0, r);
    TEST_ASSERT_EQUAL_UINT32(255, g);
    TEST_ASSERT_EQUAL_UINT32(0, b);

    core_display_hsv2rgb(240, 100, 100, &r, &g, &b);
    TEST_ASSERT_EQUAL_UINT32(0, r);
    TEST_ASSERT_EQUAL_UINT32(0, g);
    TEST_ASSERT_EQUAL_UINT32(255, b);

    /* half brightness: h=0, v=50 -> 127 */
    core_display_hsv2rgb(0, 100, 50, &r, &g, &b);
    TEST_ASSERT_EQUAL_UINT32(127, r);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_parse_int);
    RUN_TEST(test_copy_str);
    RUN_TEST(test_set_string_keys);
    RUN_TEST(test_set_numeric_keys);
    RUN_TEST(test_range_validation);
    RUN_TEST(test_range_boundaries_ok);
    RUN_TEST(test_unknown_key_and_null);
    RUN_TEST(test_split_kv);
    RUN_TEST(test_format_tz);
    RUN_TEST(test_utc_to_hms);
    RUN_TEST(test_digit_value);
    RUN_TEST(test_set_digit_grb_and_brightness);
    RUN_TEST(test_set_digit_bounds);
    RUN_TEST(test_hsv2rgb);
    return UNITY_END();
}
