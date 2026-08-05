#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start the interactive console (ESP Console REPL on the default UART,
 * i.e. the board's USB-serial port, 115200 baud) and register the clock
 * configuration commands: get, set, save, reset, reboot, status.
 *
 * This creates its own FreeRTOS task; call it once from app_main.
 */
esp_err_t config_console_init(void);

#ifdef __cplusplus
}
#endif
