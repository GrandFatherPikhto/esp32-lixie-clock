#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Буфер пикселей (порядок GRB, размер = CLOCK_LED_NUMBERS * 3)
 * Доступен для чтения извне, чтобы отправлять через led_display_send()
 */
extern uint8_t led_strip_pixels[];

/**
 * @brief Инициализация модуля: установка часового пояса, сброс состояния
 */
void clock_ui_init(void);

/**
 * @brief Заполнить буфер пикселей для отображения текущего времени
 * @note Использует системное время (time(), localtime_r)
 */
void clock_ui_fill_time(void);

/**
 * @brief Заполнить буфер пикселей для анимации загрузки (бегущие цифры 0-9 с изменением цвета)
 */
void clock_ui_fill_animation(void);

#ifdef __cplusplus
}
#endif