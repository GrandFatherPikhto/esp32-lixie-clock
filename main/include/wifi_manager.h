#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the Wi-Fi subsystem (STA netif, driver, event handlers).
 * Call once after esp_netif_init() / esp_event_loop_create_default().
 */
esp_err_t wifi_manager_init(void);

/**
 * @brief Connect to the Wi-Fi AP from the runtime config and wait for an IP.
 *
 * Blocking; returns ESP_OK once an IP is obtained, ESP_ERR_TIMEOUT if the
 * connection could not be established within the timeout, or ESP_ERR_INVALID_ARG
 * if no SSID is configured. Safe to call repeatedly (retries in the background).
 */
esp_err_t wifi_manager_connect(void);

/** @brief true if the STA is connected and has an IP address. */
bool wifi_manager_is_connected(void);

/** @brief Stop Wi-Fi (optional; mostly for power saving experiments). */
void wifi_manager_disconnect(void);

#ifdef __cplusplus
}
#endif
