# ⏰ Lixie‑style Edge‑Lit Clock on ESP32

A digital clock with "pseudo‑vacuum‑tube" display based on WS2812 LED strips and edge‑lit acrylic segments (Lixie‑style). Powered by ESP32, it synchronizes time via NTP over Wi‑Fi, and is written in C for ESP‑IDF v5.x.

---

## ✨ Features

- Displays time (hours:minutes:seconds) on **6 digits** (configurable).
- Each digit uses **10 LEDs** (0–9) – only the required digit lights up per position.
- Smooth colour cycling for each pair of digits (hours, minutes, seconds) using HSV rotation.
- **Boot animation** (before time sync): digits 0–9 chase across all positions with changing colours.
- Automatic time synchronisation via **NTP** (interval configurable).
- Wi‑Fi stays connected for periodic time updates (no re‑connection needed).
- All settings adjustable through **menuconfig** (GPIO, digit count, NTP server, sync interval).

---

## 🧩 Hardware

### Components
- Any ESP32 board (e.g., ESP32‑DevKitC).
- Addressable LED strip **WS2812** (or compatible, e.g., SK6812) – number of LEDs = `NUM_DIGITS × 10`.
- 3D‑printed or laser‑cut edge‑lit segments – 10 LEDs per digit.

### Wiring

| ESP32 GPIO | Connection           |
|------------|----------------------|
| **GPIO14** (default) | WS2812 DIN          |
| 3.3V / 5V  | LED strip power (mind the current!) |
| GND        | Common ground        |

> ⚠️ Ensure your power supply can deliver enough current (approx. 60 mA per LED at full brightness). For 60 LEDs, this is ~3.6 A at white. A 5 V / 2 A external supply is recommended.

---

## ⚙️ Configuration via `menuconfig`

Before building, run:

```bash
idf.py menuconfig
```

All settings are split into two main sections:

---

### 1️⃣ Wi‑Fi Connection

The clock **gets time via NTP over the Internet**, so Wi‑Fi is required.

Wi‑Fi settings are **not** under `Clock Configuration` but in the standard **`Example Connection Configuration`** section.

- Navigate to **`Example Connection Configuration`** → **`Wi-Fi SSID`** and **`Wi-Fi Password`**.
- Enter your network name (SSID) and password.
- Ensure the connection method is set to **`Wi-Fi`** (default).

For hidden networks, adjust **`Wi-Fi Scan Method`** → choose `Fast scan` or `All channel scan`.

> 💡 **Note:** The ESP32 connects to Wi‑Fi once at startup (`init_sntp()`) and remains connected for periodic updates. If you want to save power, you can modify the code to disconnect between synchronisations.

---

### 2️⃣ NTP Time Synchronisation

NTP settings are under **`Clock Configuration`** (your custom menu).

| Parameter | Description |
|-----------|-------------|
| **`SNTP server name`** | NTP server address (default `pool.ntp.org`). You can change to `time.google.com`, `ru.pool.ntp.org`, etc. |
| **`Time synchronization method`** | • `immediate` – sets time instantly on response.<br> • `smooth` – gradual adjustment (adjtime) to avoid jumps.<br> • `custom` – for your own implementation (not used here). |
| **`Time synchronization period`** | Interval in **seconds** (default 3600 = 1 hour). Set lower (e.g., 600) for more frequent updates. |

---

### 3️⃣ Hardware Settings (also in `Clock Configuration`)

| Parameter | Description |
|-----------|-------------|
| **`GPIO number for LED strip`** | Pin connected to WS2812 DIN (default 14). |
| **`Number of digit displays`** | How many digits: 4 for HH:MM, 6 for HH:MM:SS (default 6). |

---

## 🌐 How Time Synchronisation Works

1. **At startup**, ESP32 enables Wi‑Fi, connects to your network, and queries the NTP server.
2. **After receiving the response**, the system time is set and the flag `time_is_synced()` becomes `true`.
3. **The display** switches from boot animation to showing the current time.
4. **Every hour** (or your configured interval), ESP32 automatically polls the NTP server and adjusts the time – without re‑connecting to Wi‑Fi (the connection stays active).

> If the Internet is temporarily unavailable, the clock continues showing time using its internal RTC (ESP32 has no battery‑backed RTC, so it will drift until the next successful sync).

---

## ✅ Verifying Settings After Flashing

After flashing, monitor the logs (`idf.py monitor`) and you should see:

```
I (xxxx) wifi: station: <your_SSID> connected
I (xxxx) sntp_sync: Time synchronized successfully
I (xxxx) main: Time sync completed, sync task will exit
```

If synchronisation fails, check:
- SSID and password correctness;
- NTP server reachability (ping from your PC);
- Router not blocking NTP traffic (UDP port 123).

---

## 🛠️ Build and Flash

1. Install ESP‑IDF (recommended v5.3 or v5.5.3).
2. Clone or copy the project files into a folder.
3. Open a terminal and activate the IDF environment:
   ```bash
   . $HOME/esp/esp-idf/export.sh   # Linux/macOS
   # or
   .\export.ps1                    # Windows (PowerShell)
   ```
4. Navigate to the project directory:
   ```bash
   cd /path/to/clock
   ```
5. Configure via `idf.py menuconfig`.
6. Build and flash:
   ```bash
   idf.py build flash monitor
   ```

After boot, ESP32 connects to Wi‑Fi, syncs time, and starts displaying it. Until then, you’ll see the boot animation.

---

## 🧱 Code Architecture

The project is modular for easier maintenance and extension:

| Module | Purpose |
|--------|---------|
| **`led_display`** | Initialises RMT channel and encoder for WS2812. Provides `led_display_send()` to push pixel buffers. |
| **`clock_ui`** | Frame generation: HSV→RGB conversion, digit placement. Functions `clock_ui_fill_time()` (current time) and `clock_ui_fill_animation()` (boot animation). |
| **`sntp_sync`** | Time sync: Wi‑Fi connection, SNTP initialisation, automatic updates at configured interval. Exports `init_sntp()` and `time_is_synced()`. |
| **`main`** | FreeRTOS tasks: `sync_task` (one‑off sync) and `display_task` (continuous display refresh, switching between animation and clock). |
| **`led_strip_encoder`** | WS2812‑specific RMT encoder that converts RGB bytes into pulse sequences. |
| **`app_config.h`** | Global macros (pin, frequency, digit count, sync interval) tied to menuconfig parameters. |

### FreeRTOS Tasks
- **`sync_task`** – priority 5, runs once, calls `init_sntp()`, sets `sntp_synced` flag on success, then deletes itself.
- **`display_task`** – priority 4, loops checking the flag. If time not yet synced → shows animation, else → shows current time. Updates every 500 ms.

---

## 📦 Key Functions

| Function | Description |
|----------|-------------|
| `led_display_init()` | Initialises RMT and encoder. Called once at start. |
| `led_display_send(pixels, size)` | Sends pixel array to the strip (blocking). |
| `clock_ui_init()` | Sets timezone (MSK‑3) and resets state. |
| `clock_ui_fill_time()` | Fills global `led_strip_pixels` with current time digits (hours, minutes, seconds) with cycling colours. |
| `clock_ui_fill_animation()` | Fills buffer with animation – all digits show one digit (0→9) with rotating colour. |
| `init_sntp()` | Connects to Wi‑Fi, initialises SNTP with given interval, waits for first sync. |
| `time_is_synced()` | Returns `true` if time has been successfully obtained. |

---

## 🔧 Dependencies

This project uses the `protocol_examples_common` component from ESP‑IDF for Wi‑Fi connection (`example_connect()`). The dependency is declared in `idf_component.yml`:

```yaml
dependencies:
  protocol_examples_common:
    path: ${IDF_PATH}/examples/common_components/protocol_examples_common
```

All other components (RMT driver, SNTP, NVS, FreeRTOS) are part of ESP‑IDF.

---

## 📄 License

The code is distributed under **Apache‑2.0** (original Espressif files) and **Unlicense / CC0‑1.0** for custom additions (see file headers).

---

## 🤝 Acknowledgements

Inspired by the *Lixie* and *Edge‑Lit* clock concepts. Uses examples from Espressif (SNTP, RMT encoder for WS2812).

---

## ✏️ Future Ideas

- Add date or temperature display (with a sensor).
- Switch colour schemes via a button.
- Build a web interface for configuration without re‑flashing.
- Optimise power consumption (disconnect Wi‑Fi between syncs).

If you have questions or suggestions, please open an Issue in the repository.