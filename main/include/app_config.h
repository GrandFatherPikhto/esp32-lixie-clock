#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#define RMT_LED_STRIP_RESOLUTION_HZ  10000000   // 10 MHz
#define RMT_LED_STRIP_GPIO_NUM       CONFIG_LED_GPIO

#define CLOCK_DIGITS                 CONFIG_NUM_DIGITS
#define CLOCK_LED_NUMBERS            (CLOCK_DIGITS * 10)

// Интервал синхронизации с NTP (в секундах) – теперь можно менять в одном месте
#define SNTP_SYNC_INTERVAL_SECONDS CONFIG_SNTP_SYNC_INTERVAL_PERIOD * 1000

#endif