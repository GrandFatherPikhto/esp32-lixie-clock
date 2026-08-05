# Tests

How to set up the environment and run the project's tests on **Windows** and **Linux / WSL2**, plus what each tier covers. Architecture context is in [architect.md](architect.md).

## Overview — three test tiers

| Tier | What is tested | Tooling | Board needed? |
|------|---------------|---------|---------------|
| 1 | Python tools: `configure_clock.py`, `build_target.py` | pytest (mock serial / YAML) | No |
| 2 | Pure `core/` logic (config, time, display) | Unity (native C) | No, but needs Linux/WSL |
| 3 | Firmware builds for all supported chips | `idf.py` / `build_target.py` | No for build; yes to flash |

The CI workflow (`.github/workflows/ci.yml`) runs all three tiers on Ubuntu for every push / pull request, so a green check means everything passes without any local setup.

## Environment setup

### Windows

1. **Install ESP-IDF v6.0.2** (Espressif installer). Open an ESP-IDF PowerShell and run `export.ps1` (or use the "ESP-IDF" terminal shortcut) so `idf.py` is on `PATH`.
2. **Python virtual environment** for the tools:
   ```powershell
   python -m venv .venv
   .venv\Scripts\activate
   pip install -r tools/requirements.txt   # pyserial, pyyaml, pytest
   ```

### Linux / WSL2

1. **Install ESP-IDF v6.0.2** and install the toolchain (`$IDF_PATH/install.sh` then `. $IDF_PATH/export.sh` in bash). The vendored Unity test framework ships inside ESP-IDF.
2. **C compiler** for the host tests: `sudo apt-get install -y gcc` (Unity tests need a Linux host, so run them in WSL2 if you are on Windows).
3. **Python virtual environment** for the tools:
   ```bash
   python3 -m venv .venv
   source .venv/bin/activate
   pip install -r tools/requirements.txt
   ```

> **This workspace:** the ESP-IDF at `~/esp/esp-idf/v6.0.2` is a v6.1-dev snapshot whose generated picolibc specs conflict with the installed toolchain. The local (gitignored) `sdkconfig` is therefore set to `CONFIG_LIBC_NEWLIB=y` so a plain `idf.py build` works. CI uses a clean v6.0.2 + toolchain.

## Running the tests

### Tier 1 — Python tooling (pytest)

```bash
.venv\Scripts\python -m pytest tests -v   # Windows
.venv/bin/python -m pytest tests -v       # Linux / WSL
```

- **Coverage:** `tests/test_configure_clock.py` (serial protocol via a `FakeSerial` stub, YAML `apply`, password masking, port auto-detection incl. numeric VID/PID), `tests/test_build_target.py` (exact `idf.py` command construction from YAML/CLI; `subprocess.run` is stubbed — the build is never executed).
- **Expected:** `48 passed`.

### Tier 2 — Host C unit tests (Unity, Linux/WSL only)

```bash
. $IDF_PATH/export.sh          # sets IDF_PATH for the vendored Unity
bash tests/c/run_tests.sh
```

- The script compiles `core/src/*.c` + Unity (from `$IDF_PATH/components/unity/unity`) + `tests/c/test_core.c` into `build_host/` and runs the binary.
- **Coverage:** `core_config` parsing/validation (all keys and ranges, incl. `night_*`, `cross_fade`, `slot_machine_interval`, `wifi_power_save`, `hue_shift`, `hue_2`), `core_time` (timezone format, UTC→h/m/s, `core_time_is_night_hour`), `core_display` (digit value, GRB placement, brightness scaling, bounds, HSV→RGB).
- **Expected:** `15 Tests 0 Failures 0 Ignored OK`.

### Tier 3 — Firmware build (any OS with ESP-IDF active)

```bash
. $IDF_PATH/export.sh                       # Linux/macOS   (Windows: export.ps1)
python tools/build_target.py --target esp32
# or manually:
idf.py set-target esp32
idf.py build
```

- Supported targets: `esp32`, `esp32s2`, `esp32s3`, `esp32c3`, `esp32c6`, `esp32h2`.
- To flash and monitor a connected board: `idf.py -p /dev/ttyUSB0 flash monitor` (Linux) or `idf.py -p COM5 flash monitor` (Windows).

## Test types summary

| Type | Tool | Where | Host |
|------|------|-------|------|
| Python unit tests (mocked serial/YAML) | pytest | `tests/test_*.py` | Windows + Linux |
| Native C unit tests (pure logic) | Unity | `tests/c/test_core.c` | Linux/WSL only |
| Firmware compile + link | `idf.py` | whole project | Any (ESP-IDF active) |
| Full regression gate | GitHub Actions | `.github/workflows/ci.yml` | Ubuntu (CI) |

## Troubleshooting

- **`IDF_PATH is not set` / Unity not found** (Tier 2): run `export.sh` first so `$IDF_PATH/components/unity/unity` exists.
- **`pytest` not found:** `pip install -r tools/requirements.txt` into the project `.venv`.
- **`idf.py: Permission denied` on Linux:** run the export under **bash**, not `/bin/sh` — `bash -c '. $IDF_PATH/export.sh && idf.py build'`.
- **Picolibc specs error at build:** the installed IDF/toolchain pair is mismatched; switching the local `sdkconfig` to `CONFIG_LIBC_NEWLIB=y` resolves it (see the workspace note above).
