#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "led_display.h"
#include "clock_ui.h"
#include "sync_time.h"
#include "app_config.h"

static const char *TAG = "main";

// Задача синхронизации времени (однократная)
static void sync_task(void *arg)
{
    ESP_LOGI(TAG, "Starting time sync task");
    init_sntp();   // подключает Wi-Fi, получает время, устанавливает флаг
    ESP_LOGI(TAG, "Time sync completed, sync task will exit");
    vTaskDelete(NULL);   // задача завершается, Wi-Fi остаётся активным (если нужно)
}

// Задача отображения (работает всё время)
static void display_task(void *arg)
{
    // Инициализация UI (часовой пояс, сброс цветов)
    clock_ui_init();

    while (1) {
        if (time_is_synced()) {
            // Отображаем текущее время
            clock_ui_fill_time();
        } else {
            // Показываем анимацию загрузки
            clock_ui_fill_animation();
        }
        // Отправляем кадр на ленту
        led_display_send(led_strip_pixels, CLOCK_LED_NUMBERS * 3);

        // Задержка: 500 мс – для анимации достаточно, для часов тоже нормально
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Clock application started");

    // Инициализация RMT и LED-ленты
    ESP_ERROR_CHECK(led_display_init());

    // Создаём задачу синхронизации времени (приоритет 5)
    xTaskCreate(sync_task, "sync_task", 8192, NULL, 5, NULL);

    // Создаём задачу отображения (приоритет 4 – чуть ниже, чтобы синхронизация не мешала)
    xTaskCreate(display_task, "display_task", 4096, NULL, 4, NULL);

    // Удаляем главную задачу (app_main)
    vTaskDelete(NULL);
}