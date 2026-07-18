#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Инициализация RMT-канала и энкодера для управления LED-лентой
 * @return ESP_OK при успехе, иначе код ошибки
 */
esp_err_t led_display_init(void);

/**
 * @brief Отправить буфер пикселей на ленту
 * @param pixels Указатель на массив байт (порядок GRB, размер = CLOCK_LED_NUMBERS * 3)
 * @param size   Размер буфера в байтах
 */
void led_display_send(const uint8_t *pixels, size_t size);

#ifdef __cplusplus
}
#endif