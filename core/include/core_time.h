#ifndef CORE_TIME_H
#define CORE_TIME_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Format a UTC offset in minutes as a POSIX TZ string.
 *
 * POSIX encodes "local = UTC - offset", so the sign is inverted:
 *   180  (UTC+3)   -> "UTC-3"
 *  -300  (UTC-5)   -> "UTC+5"
 *   330  (UTC+5:30)-> "UTC-5:30"
 */
void core_time_format_tz(int32_t offset_min, char *buf, size_t len);

/**
 * @brief Convert UTC seconds since the epoch to local h/m/s using a fixed
 * minute offset (no DST). Wraps around the 24-hour day.
 * @return 0 on success, -1 if any output pointer is NULL.
 */
int core_time_utc_to_hms(int64_t utc_seconds, int32_t tz_offset_min,
                         int *hour, int *minute, int *second);

/**
 * @brief Check whether an hour of day falls inside a [start, end) night window.
 *
 * Wraps across midnight when start > end (e.g. 23-07 covers 23:00..06:59).
 * When start == end the window is empty and the function returns 0.
 * @param hour  0..23, the local hour to test.
 * @param start 0..23, night window start hour.
 * @param end   0..23, night window end hour (exclusive).
 * @return 1 if hour is inside the window, 0 otherwise.
 */
int core_time_is_night_hour(int hour, int start, int end);

#ifdef __cplusplus
}
#endif
#endif /* CORE_TIME_H */
