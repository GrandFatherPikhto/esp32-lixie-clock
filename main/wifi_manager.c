#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_check.h"
#include "app_config.h"
#include "wifi_manager.h"

static const char *TAG = "wifi_mgr";

#define WIFI_CONNECT_BIT          BIT0
#define WIFI_CONNECT_TIMEOUT_MS   (20000)

static EventGroupHandle_t s_event_group = NULL;
static bool s_started = false;

static void event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "Wi-Fi disconnected, reconnecting...");
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_event_group, WIFI_CONNECT_BIT);
    }
}

esp_err_t wifi_manager_init(void)
{
    if (esp_netif_create_default_wifi_sta() == NULL) {
        ESP_LOGE(TAG, "failed to create default wifi sta netif");
        return ESP_FAIL;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&cfg), TAG, "wifi init");

    ESP_RETURN_ON_ERROR(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                   event_handler, NULL), TAG, "register wifi event");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                   event_handler, NULL), TAG, "register ip event");

    s_event_group = xEventGroupCreate();
    return ESP_OK;
}

esp_err_t wifi_manager_connect(void)
{
    if (wifi_manager_is_connected()) {
        return ESP_OK;   /* already up */
    }
    if (s_event_group == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    const char *ssid = app_config_get_ssid();
    if (ssid == NULL || ssid[0] == '\0') {
        ESP_LOGE(TAG, "Wi-Fi SSID is not configured. Use the serial console, e.g.: "
                      "set ssid=MyNetwork password=secret save");
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_started) {
        wifi_config_t wc = { 0 };
        strlcpy((char *)wc.sta.ssid, ssid, sizeof(wc.sta.ssid));
        strlcpy((char *)wc.sta.password, app_config_get_password(), sizeof(wc.sta.password));
        wc.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
        wc.sta.pmf_cfg.capable   = true;
        wc.sta.pmf_cfg.required  = false;

        ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "set sta mode");
        ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &wc), TAG, "set wifi config");
        ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "wifi start");
        s_started = true;
        ESP_LOGI(TAG, "Connecting to \"%s\"...", ssid);
    }

    /* The event handler keeps retrying in the background; just wait for an IP. */
    xEventGroupClearBits(s_event_group, WIFI_CONNECT_BIT);
    EventBits_t bits = xEventGroupWaitBits(s_event_group, WIFI_CONNECT_BIT,
                                           pdFALSE, pdFALSE, pdMS_TO_TICKS(WIFI_CONNECT_TIMEOUT_MS));
    if (bits & WIFI_CONNECT_BIT) {
        return ESP_OK;
    }
    ESP_LOGW(TAG, "Timed out waiting for an IP address");
    return ESP_ERR_TIMEOUT;
}

bool wifi_manager_is_connected(void)
{
    return s_started && s_event_group &&
           (xEventGroupGetBits(s_event_group) & WIFI_CONNECT_BIT);
}

void wifi_manager_disconnect(void)
{
    if (s_started) {
        esp_wifi_stop();
        s_started = false;
    }
}
