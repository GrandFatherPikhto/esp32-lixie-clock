#include "clock_ui.h"
#include "app_config.h"
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "esp_log.h"

static const char *TAG = "clock_ui";

// Определение массива пикселей (доступен extern)
uint8_t led_strip_pixels[CLOCK_LED_NUMBERS * 3];

// Вспомогательные массивы
static int digits[CLOCK_DIGITS] = { 0 };
static uint16_t colors[CLOCK_DIGITS / 2] = { 0, 120, 240 };

// Статический счётчик для анимации
static int anim_step = 0;

/* --------------------------------------------------------------
 * Вспомогательные функции (преобразование HSV->RGB, установка цифры)
 * -------------------------------------------------------------- */

/**
 * @brief Преобразование HSV (оттенок, насыщенность, яркость) в RGB
 * @param h Оттенок 0-359
 * @param s Насыщенность 0-100
 * @param v Яркость 0-100
 * @param r,g,b Выходные значения RGB (0-255)
 */
static void hsv2rgb(uint32_t h, uint32_t s, uint32_t v,
                    uint32_t *r, uint32_t *g, uint32_t *b)
{
    h %= 360;
    uint32_t rgb_max = v * 2.55f;          // яркость в диапазон 0-255
    uint32_t rgb_min = rgb_max * (100 - s) / 100.0f;

    uint32_t i = h / 60;
    uint32_t diff = h % 60;
    uint32_t rgb_adj = (rgb_max - rgb_min) * diff / 60;

    switch (i) {
        case 0: *r = rgb_max; *g = rgb_min + rgb_adj; *b = rgb_min; break;
        case 1: *r = rgb_max - rgb_adj; *g = rgb_max; *b = rgb_min; break;
        case 2: *r = rgb_min; *g = rgb_max; *b = rgb_min + rgb_adj; break;
        case 3: *r = rgb_min; *g = rgb_max - rgb_adj; *b = rgb_max; break;
        case 4: *r = rgb_min + rgb_adj; *g = rgb_min; *b = rgb_max; break;
        default: *r = rgb_max; *g = rgb_min; *b = rgb_max - rgb_adj; break;
    }
}

/**
 * @brief Установить пиксели для одной цифры в одном разряде
 * @param pixels      Указатель на буфер пикселей
 * @param position    Номер разряда (0-5: 0=десятки часов, 5=единицы секунд)
 * @param digit       Цифра (0-9)
 * @param r,g,b       Цвет (0-255)
 */
static void set_clock_digit(uint8_t *pixels, int position, int digit,
                            uint8_t r, uint8_t g, uint8_t b)
{
    int index = position * 10 + digit;
    if (index < CLOCK_LED_NUMBERS) {
        pixels[index * 3 + 0] = g;   // WS2812 порядок GRB
        pixels[index * 3 + 1] = r;
        pixels[index * 3 + 2] = b;
    }
}

/* --------------------------------------------------------------
 * Публичные функции
 * -------------------------------------------------------------- */

void clock_ui_init(void)
{
    // Установка часового пояса (Москва, UTC+3)
    setenv("TZ", "MSK-3", 1);
    tzset();

    // Инициализация цветов для каждой пары разрядов (часы, минуты, секунды)
    colors[0] = 0;
    colors[1] = 120;
    colors[2] = 240;

    // Обнуляем буфер пикселей
    memset(led_strip_pixels, 0, sizeof(led_strip_pixels));
    anim_step = 0;

    ESP_LOGI(TAG, "Clock UI initialized, timezone set to MSK-3");
}

void clock_ui_fill_time(void)
{
    // Получаем текущее время
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    // Очищаем буфер
    memset(led_strip_pixels, 0, sizeof(led_strip_pixels));

    // Извлекаем цифры часов, минут, секунд
    digits[0] = timeinfo.tm_hour / 10;
    digits[1] = timeinfo.tm_hour % 10;
    digits[2] = timeinfo.tm_min / 10;
    digits[3] = timeinfo.tm_min % 10;
    digits[4] = timeinfo.tm_sec / 10;
    digits[5] = timeinfo.tm_sec % 10;

    // Для каждого разряда выбираем цвет из палитры
    for (int i = 0; i < CLOCK_DIGITS; i++) {
        uint32_t r, g, b;
        uint16_t hue = colors[i / 2];
        hsv2rgb(hue, 100, 100, &r, &g, &b);
        set_clock_digit(led_strip_pixels, i, digits[i], (uint8_t)r, (uint8_t)g, (uint8_t)b);

        // Изменяем оттенок для следующего обновления (плавное вращение)
        colors[i / 2] = (colors[i / 2] + 1) % 360;
    }

    // Можно также добавить эффект перехода между цветами, но пока оставляем
}

void clock_ui_fill_animation(void)
{
    // Каждый вызов меняет цифру и цвет
    uint8_t digit = anim_step % 10;
    uint16_t hue = (anim_step * 36) % 360;  // меняем цвет каждые 10 шагов

    uint32_t r, g, b;
    hsv2rgb(hue, 100, 100, &r, &g, &b);

    // Очищаем буфер и заполняем все разряды одной и той же цифрой
    memset(led_strip_pixels, 0, sizeof(led_strip_pixels));
    for (int pos = 0; pos < CLOCK_DIGITS; pos++) {
        set_clock_digit(led_strip_pixels, pos, digit, (uint8_t)r, (uint8_t)g, (uint8_t)b);
    }

    anim_step++;
}