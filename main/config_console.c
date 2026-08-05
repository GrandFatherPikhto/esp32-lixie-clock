#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_system.h"
#include "esp_check.h"
#include "esp_console.h"
#include "app_config.h"
#include "core_config.h"
#include "wifi_manager.h"
#include "sync_time.h"
#include "config_console.h"

static const char *TAG = "console";

/* ---------------------------------------------------------------------------
 * Command implementations
 * ------------------------------------------------------------------------- */

static int cmd_get(int argc, char **argv)
{
    app_config_dump();
    return 0;
}

static int cmd_set(int argc, char **argv)
{
    if (argc < 2) {
        printf("usage: set <key=value> [<key=value> ...]\n");
        return 1;
    }
    int failed = 0;
    for (int i = 1; i < argc; i++) {
        char key[64];
        char val[64];
        if (core_config_split_kv(argv[i], key, sizeof(key), val, sizeof(val)) != 0) {
            printf("bad argument: %s (expected key=value)\n", argv[i]);
            failed = 1;
            continue;
        }
        esp_err_t err = app_config_set(key, val);
        if (err == ESP_OK) {
            printf("set %s = %s\n", key, val);
        } else if (err == ESP_ERR_NOT_FOUND) {
            printf("unknown key: %s (try: get)\n", key);
            failed = 1;
        } else {
            printf("invalid value for %s: %s\n", key, val);
            failed = 1;
        }
    }
    printf(failed ? "some values rejected; run 'save' to persist the valid ones\n"
                  : "use 'save' to persist\n");
    return failed;
}

static int cmd_save(int argc, char **argv)
{
    esp_err_t err = app_config_save();
    printf(err == ESP_OK ? "config saved\n" : "save failed\n");
    return err == ESP_OK ? 0 : 1;
}

static int cmd_reset(int argc, char **argv)
{
    app_config_reset();
    printf("config reset to defaults and saved\n");
    return 0;
}

static int cmd_reboot(int argc, char **argv)
{
    printf("rebooting...\n");
    fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_restart();
    return 0;
}

static int cmd_status(int argc, char **argv)
{
    printf("wifi connected : %s\n", wifi_manager_is_connected() ? "yes" : "no");
    printf("time synced    : %s\n", time_is_synced() ? "yes" : "no");
    printf("ntp server     : %s\n", app_config_get_ntp_server());
    printf("sync interval  : %lu s\n", (unsigned long)app_config_get_sync_interval());
    printf("tz offset      : %ld min\n", (long)app_config_get_tz_offset());
    printf("brightness     : %u%%\n", app_config_get_brightness());
    printf("digits         : %u\n", app_config_get_digits());
    printf("led gpio       : %d\n", app_config_get_gpio());
    return 0;
}

/* ---------------------------------------------------------------------------
 * Registration
 * ------------------------------------------------------------------------- */

static void register_clock_commands(void)
{
    const esp_console_cmd_t cmds[] = {
        { .command = "get",    .help = "Print the current runtime configuration",
          .func = cmd_get },
        { .command = "set",    .help = "Set runtime parameters: set key=value [key=value ...]",
          .func = cmd_set },
        { .command = "save",   .help = "Persist the current configuration to NVS",
          .func = cmd_save },
        { .command = "reset",  .help = "Restore default configuration and save it",
          .func = cmd_reset },
        { .command = "reboot", .help = "Restart the device (applies gpio/digits changes)",
          .func = cmd_reboot },
        { .command = "status", .help = "Show Wi-Fi / time sync / display status",
          .func = cmd_status },
    };
    for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
        esp_console_cmd_register(&cmds[i]);
    }
}

esp_err_t config_console_init(void)
{
    esp_console_repl_config_t repl_cfg = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_cfg.prompt = "clock> ";

    esp_console_dev_uart_config_t hw_cfg = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();

    esp_console_repl_t *repl = NULL;
    ESP_RETURN_ON_ERROR(esp_console_new_repl_uart(&hw_cfg, &repl_cfg, &repl),
                        TAG, "new repl uart");
    register_clock_commands();
    ESP_RETURN_ON_ERROR(esp_console_start_repl(repl), TAG, "start repl");

    ESP_LOGI(TAG, "Console started. Type 'help' for available commands.");
    return ESP_OK;
}
