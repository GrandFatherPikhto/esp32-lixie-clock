# ⏰ Lixie-style Edge-Lit Clock on ESP32

A digital clock with "pseudo-vacuum-tube" display based on WS2812 LED strips and edge-lit acrylic segments (Lixie-style). Powered by ESP32, it synchronizes time via NTP over Wi-Fi, and is written in C for ESP-IDF v5.x. All settings can be changed **at runtime over the board's USB port** — no re-flashing required.

---

## ✨ Features

- Displays time (hours:minutes:seconds) on **6 digits** (runtime-configurable).
- Each digit uses **10 LEDs** (0–9) – only the required digit lights up per position.
- Smooth colour cycling for each pair of digits (hours, minutes, seconds) using HSV rotation (or a fixed colour).
- **Boot animation** (digits 0–9 chase) shown until the first successful time sync.
- Automatic time synchronisation via **NTP** with a runtime-configurable interval.
- **Runtime configuration over USB**: NTP server, sync interval, timezone, Wi-Fi SSID/password, brightness, digit count, LED GPIO, colour mode — all changeable from a PC via the `configure_clock.py` tool, persisted to NVS.
- Builds for all major ESP32 variants (`esp32`, `esp32-s2`, `esp32-s3`, `esp32-c3`, `esp32-c6`, `esp32-h2`).

---

## 🧩 Hardware

### Components
- Any ESP32 board (e.g., ESP32-DevKitC).
- Addressable LED strip **WS2812** (or compatible, e.g., SK6812) – number of LEDs = `digits × 10`.
- 3D-printed or laser-cut edge-lit segments – 10 LEDs per digit.

### Wiring

| ESP32 GPIO | Connection           |
|------------|----------------------|
| **GPIO14** (default) | WS2812 DIN          |
| 3.3V / 5V  | LED strip power (mind the current!) |
| GND        | Common ground        |

> ⚠️ Ensure your power supply can deliver enough current (approx. 60 mA per LED at full brightness). For 60 LEDs, this is ~3.6 A at white. A 5 V / 2 A external supply is recommended.

---

## ⚙️ Configuration

There are two levels of configuration:

1. **Runtime** (recommended) – all operating parameters are stored in NVS and can be changed from a PC over the board's USB port, without re-flashing.
2. **Build-time defaults** – the initial values (via `menuconfig`) that apply on first boot.

### 1️⃣ Runtime configuration (serial console / Python tool)

The firmware runs an interactive console (ESP Console) on **UART0** (the board's USB-serial bridge, 115200 baud) with the prompt `clock> `. You can talk to it manually with any serial terminal, or use the bundled tool:

```bash
# Read the current configuration
python tools/configure_clock.py get

# Set several parameters (applies immediately; SSID/password needed for Wi-Fi)
python tools/configure_clock.py set ssid=MyWiFi password=secret ntp_server=pool.ntp.org

# Persist to flash (NVS) and reboot (reboot applies gpio/digits)
python tools/configure_clock.py save
python tools/configure_clock.py reboot

# Show Wi-Fi / time-sync / display status
python tools/configure_clock.py status
```

Requires `pyserial` and `pyyaml` (`pip install -r tools/requirements.txt`; or use the project `.venv`, see below). The tool auto-detects the serial port; use `--port COM5` if needed.

**Available parameters** (`set key=value`):

| Key | Description | Default |
|-----|-------------|---------|
| `ntp_server` | NTP hostname | `pool.ntp.org` |
| `sync_interval` | NTP sync period, seconds (30..604800) | `3600` |
| `tz_offset` | UTC offset in minutes, e.g. `180` = UTC+3 (Moscow) | `180` |
| `ssid` | Wi-Fi network name | *(empty — must be set)* |
| `password` | Wi-Fi password | *(empty)* |
| `brightness` | LED brightness, percent (0..100) | `100` |
| `digits` | Number of digits to display (1..max) | `6` |
| `gpio` | LED strip GPIO *(applies after reboot)* | `14` |
| `color_mode` | `0` = rotate colours, `1` = fixed colour | `0` |
| `hue` | Fixed hue 0..359 (used when `color_mode=1`) | `200` |
| `sync_method` | `0` = immediate, `1` = smooth (adjtime) | `0` |

Console commands: `get`, `set`, `save`, `reset`, `reboot`, `status`, `help`.

#### YAML configuration (`config.yaml`)

The project ships with a [`config.yaml`](config.yaml) file in the root. It is the single source of truth for provisioning the device and is also read by [`build_target.py`](tools/build_target.py) (the `build:` section). The top-level keys map 1:1 to the `set` keys above.

```bash
# Apply everything from config.yaml, persist to NVS and reboot
python tools/configure_clock.py apply --save --reboot

# Use a different file
python tools/configure_clock.py --config my_settings.yaml apply
```

Edit `config.yaml` before flashing/running. Because it can contain real Wi-Fi credentials, keep it out of version control if needed (or store the credentials in a separate, ignored file).

> ⚠️ **Wi-Fi SSID and password are now configured here, not in `menuconfig`.** Until `ssid` is set, the clock keeps showing the boot animation and retries the Wi-Fi connection automatically.

### 2️⃣ Build-time defaults (`menuconfig`)

If you only use the runtime tool, no `menuconfig` is needed. The following **defaults** live under **`Clock Configuration`**:

| Parameter | Description |
|-----------|-------------|
| `GPIO number for LED strip (default)` | Initial LED pin (default 14). |
| `Number of digit displays (default)` | Initial active digits (default 6). |
| `Maximum number of digit displays` | Compile-time pixel buffer size; runtime `digits` is clamped to it. |
| `SNTP server name (default)` | Initial NTP server. |
| `Time synchronization method (default)` | `immediate` or `smooth`. |
| `Time synchronization period (default)` | Initial interval in seconds. |
| `Timezone offset (minutes from UTC, default)` | Initial timezone, e.g. 180 = UTC+3. |
| `Default LED brightness (percent)` | Initial brightness. |
| `Default color mode` / `Default fixed hue` | Initial colour settings. |

```bash
idf.py menuconfig
```

---

## 🌐 How Time Synchronisation Works

1. **At startup** the firmware loads the runtime config, connects to Wi-Fi (using the configured SSID/password), and queries the NTP server.
2. **After the first successful response**, the system time is set and `time_is_synced()` becomes `true`.
3. **The display** switches from the boot animation to the current time.
4. **Every configured interval** (default 1 hour) the ESP32 re-queries the NTP server and adjusts the time — Wi-Fi stays connected, no re-connect needed.
5. **If Wi-Fi/NTP is unavailable**, the clock keeps showing the animation and retries every 10 s until it succeeds.

> If the Internet is temporarily unavailable *after* a successful sync, the clock continues showing time using its internal RTC (ESP32 has no battery-backed RTC, so it will drift until the next successful sync).

---

## ✅ Verifying Settings After Flashing

After flashing, monitor the logs (`idf.py monitor`) and you should see:

```
I (xxxx) wifi_mgr: Got IP: 192.168.1.xx
I (xxxx) sntp_sync: Time synchronized successfully
I (xxxx) main: Time sync completed, sync task will exit
```

You can also connect to the serial console (115200 baud) and run `status` or `get`.

If synchronisation fails, check:
- `ssid` / `password` are set and saved (`configure_clock.py set ssid=... password=... save`);
- NTP server reachability (ping from your PC);
- Router not blocking NTP traffic (UDP port 123).

---

## 🛠️ Build and Flash

1. Install ESP-IDF (recommended v5.3 or v5.5.3).
2. Clone or copy the project files into a folder.
3. Open a terminal and activate the IDF environment:
   ```bash
   . $HOME/esp/esp-idf/export.sh   # Linux/macOS
   .\export.ps1                    # Windows (PowerShell)
   ```
4. Configure defaults via `idf.py menuconfig` (optional).
5. Build (optionally for a specific chip) and flash:

   ```bash
   # Single command for any supported target:
   python tools/build_target.py --target esp32s3 --flash
   # or manually:
   idf.py set-target esp32s3
   idf.py build
   idf.py flash monitor
   ```

   `build_target.py` also reads the `build:` section of [`config.yaml`](config.yaml) when no flags are given:
   ```bash
   python tools/build_target.py --config config.yaml
   ```

6. Configure Wi-Fi and preferences over USB:
   ```bash
   python tools/configure_clock.py set ssid=MyWiFi password=secret save reboot
   # or from the YAML file:
   python tools/configure_clock.py apply --save --reboot
   ```

### 🐍 Python virtual environment (optional but recommended)

Create a `.venv` in the project root (it is already ignored in `.gitignore`):

```bash
python -m venv .venv
.venv\Scripts\activate            # Windows
source .venv/bin/activate         # Linux/macOS
pip install -r tools/requirements.txt
```

Then run the tools as `python tools/configure_clock.py ...` and `python tools/build_target.py ...` with the venv active.

### 🧪 Tests (Python tooling — no hardware required)

[`configure_clock.py`](tools/configure_clock.py) and [`build_target.py`](tools/build_target.py) are covered by a pytest suite in [`tests/`](tests/) that mocks the serial port and YAML files — **no ESP32 board is needed**. Run it from the project root:

```bash
.venv\Scripts\python -m pytest tests -v   # Windows
.venv/bin/python -m pytest tests -v       # Linux/macOS
```

`pytest` is declared in [`tools/requirements.txt`](tools/requirements.txt) — install it with `pip install -r tools/requirements.txt` if it isn't present.

### 🛠️ Host C unit tests (Tier 2 — core logic, no board, needs Linux/WSL)

The pure firmware logic lives in [`core/`](core/) (config parsing/validation, time-zone math, digit rendering) with **no ESP-IDF dependencies**, so it can be unit-tested natively on a Linux host with Unity. Run from WSL2:

```bash
. /home/grand/esp/esp-idf-v6.0/export.sh     # or your IDF
tests/c/run_tests.sh
```

The script compiles `core/src/*.c` + Unity (from `$IDF_PATH`) + [`tests/c/test_core.c`](tests/c/test_core.c) into `build_host/` and runs it — 14 tests, 0 failures. These exercise the exact functions the firmware uses (`core_config_set_value`, `core_time_format_tz`, `core_display_set_digit`, ...).

The firmware is chip-agnostic — only the LED GPIO needs to match your board's wiring.

---

## 🧱 Code Architecture

The project is modular for easier maintenance and extension:

| Module | Purpose |
|--------|---------|
| **`app_config`** | NVS-backed runtime configuration (Wi-Fi, NTP, timezone, brightness, digits, GPIO, colours). Provides `get`/`set`/`save`/`reset` and getters used by the other modules. |
| **`config_console`** | ESP Console REPL on UART0 (USB). Registers `get`, `set`, `save`, `reset`, `reboot`, `status` commands. |
| **`wifi_manager`** | Wi-Fi STA connection using the runtime SSID/password, with automatic reconnect and IP-wait. |
| **`led_display`** | RMT channel + encoder init and `led_display_send()` to push pixel buffers. |
| **`clock_ui`** | Frame generation: HSV→RGB, digit placement, brightness/colour handling. `clock_ui_fill_time()` and `clock_ui_fill_animation()` render into a caller-provided buffer. |
| **`sntp_sync`** | SNTP init with runtime settings, first-sync wait, sync-status flag. |
| **`led_strip_encoder`** | WS2812-specific RMT encoder. |
| **`main`** | Startup orchestration and FreeRTOS tasks: `sync_task` and `display_task`. |

### FreeRTOS Tasks
- **`sync_task`** (priority 5) – connects Wi-Fi and calls `init_sntp()`; on success sets the sync flag and exits; on failure retries every 10 s.
- **`display_task`** (priority 4) – loops every 500 ms. Before sync: boot animation; after sync: current time. Owns the pixel buffer.

---

## 📦 Key Functions

| Function | Description |
|----------|-------------|
| `app_config_init()` | Loads config from NVS (or defaults) and applies the timezone. |
| `app_config_set/save/reset()` | Change, persist or restore the runtime configuration. |
| `config_console_init()` | Starts the serial console REPL with the configuration commands. |
| `wifi_manager_connect()` | Connects to the configured Wi-Fi and waits for an IP (retries in background). |
| `led_display_init()` | Initialises RMT + encoder using the runtime LED GPIO. |
| `led_display_send(pixels, size)` | Sends a pixel array to the strip (blocking). |
| `clock_ui_fill_time(pixels)` | Renders the current time into the buffer. |
| `clock_ui_fill_animation(pixels)` | Renders the boot animation into the buffer. |
| `init_sntp()` | Connects, starts SNTP and waits for the first sync; returns success. |
| `time_is_synced()` | `true` only after a successful time sync. |

---

## 🔧 Dependencies

- **`console`** – ESP-IDF component providing the interactive REPL (`esp_console`).
- All other components (RMT driver, SNTP, NVS, Wi-Fi, FreeRTOS) are part of ESP-IDF.

The previous dependency on `protocol_examples_common` was removed — Wi-Fi is now handled by the project's own `wifi_manager` so that the SSID/password can be configured at runtime.

---

## 📄 License

The code is distributed under **Apache-2.0** (original Espressif files) and **Unlicense / CC0-1.0** for custom additions (see file headers).

---

## 🤝 Acknowledgements

Inspired by the *Lixie* and *Edge-Lit* clock concepts. Uses examples from Espressif (SNTP, RMT encoder for WS2812).

---

## ✏️ Future Ideas

- Add date or temperature display (with a sensor).
- Switch colour schemes via a button (in addition to the console).
- Build a web interface for configuration.
- Optimise power consumption (disconnect Wi-Fi between syncs).
