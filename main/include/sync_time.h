#ifndef SYNC_TIME_H_
#define SYNC_TIME_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Connect to Wi-Fi and synchronize time via SNTP (blocking).
 * @return true on success, false on failure (caller may retry).
 */
bool init_sntp(void);

/**
 * @brief true once the system time has been successfully synchronized.
 * Until this returns true, the display shows the boot animation.
 */
bool time_is_synced(void);

#ifdef __cplusplus
}
#endif

#endif /* SYNC_TIME_H_ */
