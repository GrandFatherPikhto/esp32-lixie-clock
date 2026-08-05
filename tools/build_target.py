#!../.venv/bin/python
# -*- coding: utf-8 -*-
"""
build_target.py - build the clock firmware for a specific ESP32 variant.

The firmware is chip-agnostic: only the WS2812 data GPIO and the target chip
need to be chosen. This helper wraps `idf.py set-target` + `idf.py build`
(and optionally menuconfig / flash / monitor).

Usage (ESP-IDF environment must be active, e.g. `export.ps1` on Windows or
`$IDF_PATH/export.sh` on Linux/macOS):

    # Build for the default target
    python tools/build_target.py

    # Build for an ESP32-S3, open menuconfig, then flash and monitor
    python tools/build_target.py --target esp32s3 --menuconfig --flash --monitor

    # Build using the "build:" section of ./config.yaml (project root)
    python tools/build_target.py --config config.yaml

Supported targets: esp32, esp32s2, esp32s3, esp32c3, esp32c6, esp32h2

Command-line arguments always override the values from the YAML config.
"""

import argparse
import subprocess
import sys
from pathlib import Path

SUPPORTED_TARGETS = ["esp32", "esp32s2", "esp32s3", "esp32c3", "esp32c6", "esp32h2"]


# ---------------------------------------------------------------------------
# YAML configuration support
# ---------------------------------------------------------------------------

def default_config_path():
    """Project-root config.yaml (two levels up from this script)."""
    return Path(__file__).resolve().parent.parent / "config.yaml"


def load_build_config(explicit):
    """Return the `build:` section of the YAML config, or {} if not found."""
    path = Path(explicit) if explicit else default_config_path()
    if not path.is_file():
        if explicit:
            sys.exit("Configuration file not found: %s" % path)
        return {}
    try:
        import yaml
    except ImportError:  # pragma: no cover
        sys.exit("PyYAML is required to read YAML config. Install it with: pip install pyyaml")
    try:
        with open(path, "r", encoding="utf-8") as f:
            data = yaml.safe_load(f) or {}
    except yaml.YAMLError as exc:
        sys.exit("Failed to parse %s: %s" % (path, exc))
    build = data.get("build")
    return build if isinstance(build, dict) else {}


def main():
    parser = argparse.ArgumentParser(description="Build the clock firmware for an ESP32 variant.")
    parser.add_argument("--config", "-y", default=None,
                        help="Path to a YAML file with a `build:` section "
                             "(default: ./config.yaml in the project root)")
    parser.add_argument("--target", "-t", default=None, choices=SUPPORTED_TARGETS,
                        help="Target chip (default: from config.yaml, else esp32)")
    parser.add_argument("--menuconfig", "-m", action="store_true",
                        help="Open menuconfig before building")
    parser.add_argument("--flash", "-f", action="store_true",
                        help="Flash the firmware after building")
    parser.add_argument("--monitor", "-M", action="store_true",
                        help="Open the serial monitor after flashing")
    parser.add_argument("--clean", "-c", action="store_true",
                        help="Run a full clean build")

    args = parser.parse_args()
    cfg = load_build_config(args.config)

    target = args.target or cfg.get("target") or "esp32"
    if target not in SUPPORTED_TARGETS:
        sys.exit("Unsupported target from config: %s (choose from: %s)"
                 % (target, ", ".join(SUPPORTED_TARGETS)))

    clean = args.clean or bool(cfg.get("clean", False))
    menuconfig = args.menuconfig or bool(cfg.get("menuconfig", False))
    flash = args.flash or bool(cfg.get("flash", False))
    monitor = args.monitor or bool(cfg.get("monitor", False))

    commands = ["idf.py", "set-target", target]
    if clean:
        commands += ["fullclean"]
    if menuconfig:
        commands += ["menuconfig"]
    commands += ["build"]
    if flash:
        commands += ["flash"]
    if monitor:
        commands += ["monitor"]

    print("$ " + " ".join(commands))
    try:
        subprocess.run(commands, check=True)
    except FileNotFoundError:
        sys.exit(
            "idf.py was not found. Activate the ESP-IDF environment first "
            "(Windows: export.ps1; Linux/macOS: $IDF_PATH/export.sh)."
        )
    except subprocess.CalledProcessError as exc:
        sys.exit("Build failed with exit code %d" % exc.returncode)


if __name__ == "__main__":
    main()
