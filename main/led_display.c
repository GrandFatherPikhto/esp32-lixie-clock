#include "led_display.h"
#include "app_config.h"
#include "driver/rmt_tx.h"
#include "led_strip_encoder.h"
#include "esp_check.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"


static const char *TAG = "led_display";

static rmt_channel_handle_t led_chan = NULL;
static rmt_encoder_handle_t led_encoder = NULL;
static rmt_transmit_config_t tx_config = {
    .loop_count = 0,
};

esp_err_t led_display_init(void)
{
    ESP_LOGI(TAG, "Create RMT TX channel");
    rmt_tx_channel_config_t tx_chan_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = app_config_get_gpio(),   /* runtime-configurable pin */
        .mem_block_symbols = 64,
        .resolution_hz = RMT_LED_STRIP_RESOLUTION_HZ,
        .trans_queue_depth = 4,
    };
    ESP_RETURN_ON_ERROR(rmt_new_tx_channel(&tx_chan_config, &led_chan), TAG, "Failed to create RMT TX channel");

    ESP_LOGI(TAG, "Install led strip encoder");
    led_strip_encoder_config_t encoder_config = {
        .resolution = RMT_LED_STRIP_RESOLUTION_HZ,
    };
    ESP_RETURN_ON_ERROR(rmt_new_led_strip_encoder(&encoder_config, &led_encoder), TAG, "Failed to create led strip encoder");

    ESP_LOGI(TAG, "Enable RMT TX channel");
    ESP_RETURN_ON_ERROR(rmt_enable(led_chan), TAG, "Failed to enable RMT TX channel");

    return ESP_OK;
}

void led_display_send(const uint8_t *pixels, size_t size)
{
    if (led_chan == NULL || led_encoder == NULL) {
        ESP_LOGE(TAG, "LED display not initialized");
        return;
    }
    ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, pixels, size, &tx_config));
    ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
}