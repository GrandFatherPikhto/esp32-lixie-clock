# ESP32 Lixie Clock — Architecture

This document describes the structure, modules and data flow of the Lixie-style edge-lit clock firmware, for readers who want to understand or extend the codebase. For how the project is verified, see [tests.md](tests.md).

## 1. Overview

The clock is an **ESP32** running **ESP-IDF** (recommended **v6.0.2**, also builds on v5.3). It drives a WS2812 addressable LED strip (10 LEDs per digit), keeps time via **NTP over Wi-Fi**, and is configured **at runtime over the board's USB port** (UART0 console) — no re-flashing needed for settings.

Core properties:

- **Pure logic separated from the hardware.** Everything that can be unit-tested on a host lives in `core/` with **no ESP-IDF dependencies** (config parsing/validation, time math, digit rendering). This is the single most important design decision: the same validation code runs in the firmware and in the host unit tests.
- **Chip-agnostic firmware.** One codebase builds for `esp32`, `esp32s2`, `esp32s3`, `esp32c3`, `esp32c6`, `esp32h2`. Only the WS2812 GPIO and the target chip need to be chosen.
- **Runtime configuration.** All operating parameters live in NVS and are changed via the serial console or the `configure_clock.py` helper.

## 2. Directory layout

| Path | Purpose |
|------|---------|
| `main/` | The ESP-IDF firmware component |
| `core/` | Pure, host-testable logic (no ESP-IDF): `core_config`, `core_time`, `core_display` |
| `tools/` | Python helpers: `configure_clock.py` (provision over USB), `build_target.py` (multi-target build) |
| `tests/` | pytest suite for the Python tools + `tests/c/` host C unit tests (Unity) |
| `.github/workflows/ci.yml` | CI: python tools, host C tests, firmware matrix build |
| `.devcontainer/` | Optional ESP-IDF Docker devcontainer |
| `config.yaml` | Single source of truth for provisioning the device (also drives the `build:` section) |

## 3. Module map

| Module | Files | Responsibility |
|--------|-------|----------------|
| `core_config` | `core/include/core_config.h`, `core/src/core_config.c` | `clock_config_t` struct + `key=value` parsing and range validation (pure, host-testable) |
| `core_time` | `core/include/core_time.h`, `core/src/core_time.c` | UTC→local h/m/s math, timezone format string, night-window check `core_time_is_night_hour()` (pure) |
| `core_display` | `core/include/core_display.h`, `core/src/core_display.c` | Digit→LED placement and HSV→RGB conversion (pure) |
| `app_config` | `main/app_config.c`, `main/include/app_config.h` | NVS-backed runtime config; `get`/`set`/`save`/`reset`/`dump`; getters; timezone apply |
| `config_console` | `main/config_console.c`, `main/include/config_console.h` | ESP Console REPL on UART0 (USB): `get`, `set`, `save`, `reset`, `reboot`, `status` |
| `wifi_manager` | `main/wifi_manager.c`, `main/include/wifi_manager.h` | Wi-Fi STA connect/reconnect, IP wait, and `wifi_manager_disconnect()` (radio off) |
| `sntp_sync` | `main/sntp_sync.c`, `main/include/sync_time.h` | One-shot / background SNTP sync; `time_is_synced()` flag |
| `clock_ui` | `main/clock_ui.c`, `main/include/clock_ui.h` | Stateful per-frame renderer: boot animation, time, cross-fade, slot machine, palettes, night mode, breathing |
| `led_display` | `main/led_display.c`, `main/include/led_display.h` | RMT channel + encoder init; `led_display_send()` to push pixel buffers |
| `led_strip_encoder` | `main/led_strip_encoder.c`, `main/include/led_strip_encoder.h` | WS2812-specific RMT encoder |
| `main` | `main/main.c` | Startup orchestration, FreeRTOS tasks, display hardware timer |

## 4. Runtime configuration chain

A setting such as `brightness` travels a single well-defined path:

1. **Input** — `configure_clock.py set brightness=50 save` (over USB) sends `set brightness=50` to the console; `configure_clock.py apply` reads `config.yaml`.
2. **Parse & validate** — the console `set` command calls `app_config_set()` → `core_config_set_value()`, which converts the string and enforces ranges (e.g. `night_low_brightness` 0..100, `cross_fade` 0..2000 ms).
3. **Store** — `app_config_save()` writes the whole `clock_config_t` struct as a single NVS blob.
4. **Read** — modules use `app_config_get_*()` getters each frame or cycle.

Because parsing/validation lives in `core/`, it is unit-tested on the host with no device attached.

## 5. Display renderer (`clock_ui`)

[`clock_ui_frame(pixels, time_synced)`](../main/clock_ui.c) is the stateful per-frame renderer, driven by the display task at ~33 fps (30 ms hardware timer):

- **Before sync** — boot animation: digits 0–9 chase with rotating colour.
- **After sync** — current time (local h/m/s from `core_time_utc_to_hms()`), coloured by the active palette.
- **Cross-fade** — digit changes fade over `cross_fade` milliseconds (0 = instant); the outgoing and incoming digits share the same 10 LEDs at complementary brightness.
- **Slot machine** — a periodic 0–9 roll (duration ~2.5 s) every `slot_machine_interval` minutes (0 = off), routed through the same cross-fade mechanism.
- **Night mode** — dims to `night_low_brightness` during `night_start`..`night_end` (wrap-aware) when `night_mode` is on.
- **Breathing** — optional brightness pulsing that combines with any palette.

All animation is **time-based** (500 ms accumulators for the classic palette/boot speeds), so the frame rate does not affect animation speed. Colour comes from `palette_hue()`, which takes the palette mode plus a global `hue_shift` offset and the secondary `hue_2` (Mono two-tone).

## 6. Tasks and timing

- **`sync_task`** (priority 5) — connects Wi-Fi and calls `init_sntp()`. On success it either exits (default) or, when `wifi_power_save` is on, switches the radio off and sleeps for `sync_interval` seconds before re-syncing. On failure it retries every 10 s.
- **`display_task`** (priority 4) — owns the pixel buffer. Instead of `vTaskDelay`, a periodic `esp_timer` (30 ms) notifies the task from an ISR; each wake renders one frame via `clock_ui_frame()` and sends it with `led_display_send()` (RMT stays in task context).

## 7. Wi-Fi / NTP / power save

- `wifi_manager` connects with the runtime SSID/password, auto-reconnects on disconnect, and waits for an IP (20 s timeout).
- `init_sntp()` starts an SNTP client with the runtime server, sync interval and sync method (immediate or smooth `adjtime`). With **power save off** (default) the client is left running and background SNTP keeps the time updated while Wi-Fi stays connected. With `wifi_power_save = 1`, the sync is one-shot, the SNTP client is torn down, and `sync_task` calls `wifi_manager_disconnect()` (radio fully off) until the next `sync_interval`; the display keeps running from the RTC-based libc time.
- The USB console and `configure_clock.py` run over UART and are unaffected by Wi-Fi state.

## 8. Tools (`tools/`)

- **`configure_clock.py`** — drives the serial console over USB: `get`, `set`, `save`, `reset`, `reboot`, `status`, `apply` (from YAML). Auto-detects the board's port by numeric VID/PID (CP210x/CH340/FTDI) then by description. Masks the Wi-Fi password in output.
- **`build_target.py`** — wraps `idf.py set-target` + `build` (+ optional `menuconfig`/`flash`/`monitor`/`clean`) for any supported chip; reads the `build:` section of `config.yaml`.

## 9. Testing and CI

- **Tier 1 — Python tools:** pytest suite in `tests/` (no board, serial port and YAML mocked).
- **Tier 2 — Host C unit tests:** Unity tests for `core/` run natively on Linux/WSL via `tests/c/run_tests.sh`.
- **Tier 3 — Firmware builds:** `idf.py` builds for all six targets, run in CI on Ubuntu for every push/PR.

See [tests.md](tests.md) for setup and how to run each tier locally (Windows and Linux/WSL).

## 10. Key functions

| Function | Description |
|----------|-------------|
| `app_config_init()` | Load config from NVS (or defaults) and apply the timezone |
| `app_config_set()/save()/reset()` | Change, persist, restore the runtime configuration |
| `core_config_set_value()` | Parse/validate one `key=value` against `clock_config_t` (pure) |
| `core_time_is_night_hour()` | Wrap-aware night-window check (pure) |
| `config_console_init()` | Start the serial console REPL |
| `wifi_manager_connect()` | Connect to the configured Wi-Fi and wait for an IP |
| `wifi_manager_disconnect()` | Stop the Wi-Fi radio (`esp_wifi_stop()`) |
| `init_sntp()` | Connect, start SNTP, wait for the first sync; returns success |
| `time_is_synced()` | `true` only after a successful time sync |
| `clock_ui_frame()` | Render one stateful frame (boot/time/cross-fade/slot/night) |
| `led_display_send(pixels, size)` | Send a pixel array to the strip (bounded wait) |
