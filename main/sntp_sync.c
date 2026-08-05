#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "esp_sntp.h"
#include "app_config.h"
#include "wifi_manager.h"
#include "sync_time.h"

static const char *TAG = "sntp_sync";

/* Written by the sync task, read by the display task -> keep it volatile. */
static volatile bool sntp_synced = false;

static void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Notification of a time synchronization event");
}

/**
 * @brief Connect to Wi-Fi, start SNTP with the runtime settings and wait for
 * the first successful sync.
 *
 * @return true on success (sntp_synced set, Wi-Fi kept connected for periodic
 *         updates), false on failure (caller may retry; nothing is flagged).
 */
bool init_sntp(void)
{
    /* 1) Wi-Fi must be up (SSID/password come from the runtime config) */
    if (wifi_manager_connect() != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi connection failed, cannot sync time");
        return false;
    }

    /* 2) Configure SNTP from the runtime config */
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(app_config_get_ntp_server());
    config.sync_cb = time_sync_notification_cb;
    config.start = false;                 /* we start it explicitly below */
    if (app_config_get_sync_method() == 1) {
        config.smooth_sync = true;        /* gradual adjtime instead of a jump */
    }
    esp_netif_sntp_init(&config);

    /* 3) Sync interval: stored in seconds, API expects milliseconds */
    esp_sntp_set_sync_interval(app_config_get_sync_interval() * 1000);
    esp_netif_sntp_start();

    /* 4) Wait for the first successful sync (bounded, 15 x 2 s) */
    bool ok = false;
    for (int i = 0; i < 15; i++) {
        if (esp_netif_sntp_sync_wait(2000) == ESP_OK) {
            ok = true;
            break;
        }
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/15)", i + 1);
    }

    if (!ok) {
        ESP_LOGW(TAG, "Time synchronization failed; will retry later");
        esp_netif_sntp_deinit();
        return false;
    }

    /* 5) Only now mark the clock as synced */
    sntp_synced = true;
    ESP_LOGI(TAG, "Time synchronized successfully");
    return true;
}

bool time_is_synced(void)
{
    return sntp_synced;
}
