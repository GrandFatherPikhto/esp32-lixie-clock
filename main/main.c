#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "led_display.h"
#include "clock_ui.h"
#include "sync_time.h"
#include "app_config.h"
#include "wifi_manager.h"
#include "config_console.h"

static const char *TAG = "main";

/* The pixel buffer is owned by the display task (no global mutable state). */
static uint8_t led_strip_pixels[CLOCK_LED_NUMBERS_MAX * 3];

#define SYNC_RETRY_DELAY_MS   (10000)

/* ---------------------------------------------------------------------------
 * Tasks
 * ------------------------------------------------------------------------- */

/* Retries until the first successful time sync, then exits. */
static void sync_task(void *arg)
{
    ESP_LOGI(TAG, "Starting time sync task");
    while (1) {
        if (init_sntp()) {
            ESP_LOGI(TAG, "Time sync completed, sync task will exit");
            vTaskDelete(NULL);
        }
        ESP_LOGW(TAG, "Time sync failed, retrying in %d s", SYNC_RETRY_DELAY_MS / 1000);
        vTaskDelay(pdMS_TO_TICKS(SYNC_RETRY_DELAY_MS));
    }
}

/* Runs forever: shows the boot animation until the time is synced, then the
 * current time. */
static void display_task(void *arg)
{
    clock_ui_init();

    while (1) {
        if (time_is_synced()) {
            clock_ui_fill_time(led_strip_pixels);
        } else {
            clock_ui_fill_animation(led_strip_pixels);
        }
        led_display_send(led_strip_pixels, app_config_get_digits() * 10 * 3);

        /* 500 ms: plenty for the animation, fine for the clock */
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/* ---------------------------------------------------------------------------
 * Startup
 * ------------------------------------------------------------------------- */

/* NVS must be initialized early, with recovery for first boot / partition
 * changes (standard ESP-IDF pattern). */
static void nvs_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Clock application started");

    nvs_init();
    app_config_init();            /* load runtime config (NVS-backed) */

    /* Network stack init belongs here, not inside a task */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(led_display_init());      /* uses runtime LED GPIO */
    ESP_ERROR_CHECK(wifi_manager_init());
    ESP_ERROR_CHECK(config_console_init());   /* serial config REPL on UART0 */

    /* Priority 5: sync gets the CPU when it needs it. */
    xTaskCreate(sync_task, "sync_task", 8192, NULL, 5, NULL);
    /* Priority 4: display. */
    xTaskCreate(display_task, "display_task", 4096, NULL, 4, NULL);
}
