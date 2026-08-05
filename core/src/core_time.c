#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include "core_time.h"

void core_time_format_tz(int32_t offset_min, char *buf, size_t len)
{
    if (buf == NULL || len == 0) {
        return;
    }
    char sign = (offset_min < 0) ? '+' : '-';
    unsigned abs_min = (offset_min < 0)
                           ? (unsigned)(-(int64_t)offset_min)
                           : (unsigned)offset_min;
    unsigned hh = abs_min / 60;
    unsigned mm = abs_min % 60;
    if (mm) {
        snprintf(buf, len, "UTC%c%u:%02u", sign, hh, mm);
    } else {
        snprintf(buf, len, "UTC%c%u", sign, hh);
    }
}

int core_time_utc_to_hms(int64_t utc_seconds, int32_t tz_offset_min,
                         int *hour, int *minute, int *second)
{
    if (hour == NULL || minute == NULL || second == NULL) {
        return -1;
    }
    int64_t local = utc_seconds + (int64_t)tz_offset_min * 60;
    int64_t day = local % 86400;
    if (day < 0) {
        day += 86400;
    }
    *hour   = (int)(day / 3600);
    *minute = (int)((day % 3600) / 60);
    *second = (int)(day % 60);
    return 0;
}
