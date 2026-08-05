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
        /* This is the only RMT channel on the board, so it can use the full
         * hardware memory block instead of the default 64 symbols. A small
         * block forces the driver to refill it via ISR many times per frame
         * (~22x for 60 LEDs at 64 symbols); if a refill is delayed (e.g. by
         * the Wi-Fi stack, which stays connected by default), the WS2812
         * bitstream desyncs from that point onward, showing garbage colors
         * on the LEDs transmitted afterwards. A raised interrupt priority
         * plus the full memory block eliminates the desync (confirmed on
         * hardware). */
        .mem_block_symbols = 512,
        .resolution_hz = RMT_LED_STRIP_RESOLUTION_HZ,
        .trans_queue_depth = 4,
        .intr_priority = 3,
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
    if (pixels == NULL || size == 0 || size > CLOCK_LED_NUMBERS_MAX * 3) {
        ESP_LOGW(TAG, "Invalid frame: pixels=%p size=%u", (const void *)pixels,
                 (unsigned)size);
        return;
    }
    esp_err_t err = rmt_transmit(led_chan, led_encoder, pixels, size, &tx_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "rmt_transmit failed: %s", esp_err_to_name(err));
        return;
    }
    /* Bounded wait: never block the display task forever if RMT stalls, which
     * would freeze the animation on the last frame. */
    err = rmt_tx_wait_all_done(led_chan, pdMS_TO_TICKS(500));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "RMT tx not done in time (%s); skipping frame",
                 esp_err_to_name(err));
    }
}