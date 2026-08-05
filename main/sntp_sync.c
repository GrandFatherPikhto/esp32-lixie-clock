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

/* Start a fresh SNTP client with the runtime settings (server, sync interval,
 * sync method). The client is torn down with stop_sntp_client() after the
 * wait unless background resync is wanted (Wi-Fi power save off). */
static void start_sntp_client(void)
{
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(app_config_get_ntp_server());
    config.sync_cb = time_sync_notification_cb;
    config.start = false;                 /* we start it explicitly below */
    if (app_config_get_sync_method() == 1) {
        config.smooth_sync = true;        /* gradual adjtime instead of a jump */
    }
    esp_netif_sntp_init(&config);

    /* Sync interval: stored in seconds, API expects milliseconds */
    esp_sntp_set_sync_interval(app_config_get_sync_interval() * 1000);
    esp_netif_sntp_start();
}

static void stop_sntp_client(void)
{
    esp_netif_sntp_deinit();
}

/* Block until the next successful sync or the timeout elapses (15 x 2 s). */
static bool wait_for_sync(void)
{
    for (int i = 0; i < 15; i++) {
        if (esp_netif_sntp_sync_wait(2000) == ESP_OK) {
            return true;
        }
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/15)", i + 1);
    }
    return false;
}

/**
 * @brief Connect to Wi-Fi and synchronize time via SNTP (blocking).
 *
 * With Wi-Fi power save enabled this is a one-shot sync: a fresh SNTP client is
 * started, we wait for the first successful sync, and the client is torn down
 * so the caller can switch the radio off until the next resync. Without power
 * save the SNTP client is left running, so background SNTP keeps the time
 * updated while Wi-Fi stays connected (the classic behaviour).
 *
 * @return true on success (sntp_synced set), false on failure (caller may retry).
 */
bool init_sntp(void)
{
    /* 1) Wi-Fi must be up (SSID/password come from the runtime config) */
    if (wifi_manager_connect() != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi connection failed, cannot sync time");
        return false;
    }

    /* 2) Start a fresh SNTP client and wait for the first successful sync */
    start_sntp_client();
    bool ok = wait_for_sync();
    if (!ok) {
        ESP_LOGW(TAG, "Time synchronization failed; will retry later");
        stop_sntp_client();
        return false;
    }

    /* 3) Only now mark the clock as synced */
    sntp_synced = true;
    ESP_LOGI(TAG, "Time synchronized successfully");

    if (app_config_get_wifi_power_save()) {
        /* One-shot: stop the client so the radio can be switched off. */
        stop_sntp_client();
    }
    /* Else: leave the client running for background updates (Wi-Fi stays on). */
    return true;
}

bool time_is_synced(void)
{
    return sntp_synced;
}
