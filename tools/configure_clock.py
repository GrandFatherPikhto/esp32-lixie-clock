#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
configure_clock.py - configure the ESP32 Lixie clock over the board's USB port.

The firmware exposes an interactive console (ESP Console) on UART0
(the board's USB-serial bridge, 115200 baud) with the prompt "clock> ".
This tool drives that console to read / change / persist settings.

Usage examples:
    # Read the current configuration
    python configure_clock.py get
    python configure_clock.py --port COM5 get --json

    # Set several parameters at once (applies to RAM immediately)
    python configure_clock.py set ssid=MyWiFi password=secret ntp_server=pool.ntp.org

    # Persist to flash (NVS) and reboot to apply gpio/digits
    python configure_clock.py save
    python configure_clock.py reboot

    # Restore factory defaults
    python configure_clock.py reset

    # Show Wi-Fi / time-sync / display status
    python configure_clock.py status

    # Apply all settings from config.yaml (project root), persist + reboot
    python configure_clock.py apply --save --reboot
    python configure_clock.py --config my_settings.yaml apply

    # Build + flash first (requires the ESP-IDF environment to be active)
    python build_target.py --target esp32s3 --flash
    python configure_clock.py set ssid=MyWiFi password=secret save reboot

Available keys (run `get` on the device to see current values):
    ntp_server     NTP hostname, e.g. pool.ntp.org
    sync_interval  NTP sync period in seconds (30 .. 604800)
    tz_offset      UTC offset in minutes, e.g. 180 = UTC+3 (Moscow)
    ssid           Wi-Fi network name
    password       Wi-Fi password
    brightness     LED brightness in percent (0 .. 100)
    digits         number of digits to display (1 .. max digits)
    gpio           LED strip GPIO (applies after reboot)
    color_mode     0 = rotate colors, 1 = fixed color
    hue            fixed hue (0 .. 359), used when color_mode = 1
    sync_method    0 = immediate, 1 = smooth (adjtime)

`apply` reads a YAML file (default: ./config.yaml) whose top-level keys map
1:1 to the keys above and issues a single `set` with all of them. Requires
PyYAML: pip install pyyaml
"""

import argparse
import json
import re
import sys
import time
from pathlib import Path

try:
    import serial
except ImportError:  # pragma: no cover
    sys.exit("pyserial is required. Install it with: pip install pyserial")

PROMPT = b"clock> "
EOL = "\r\n"
DEFAULT_BAUD = 115200

# Device-side keys accepted by the firmware `set` command.
KNOWN_KEYS = {
    "ntp_server", "sync_interval", "tz_offset",
    "ssid", "password",
    "brightness", "digits", "gpio", "color_mode", "hue", "sync_method",
}

# Match the device-side "key = value" lines printed by the `get` command.
KEYVAL_RE = re.compile(r"^\s*(\w+)\s*=\s*(.*?)\s*$")

# Lines emitted by the ESP-IDF logging subsystem (interleaved with console
# output). They are stripped so the parsed output is deterministic.
LOG_LINE_RE = re.compile(r"^[IWEVDv]\s*\([\d:,.]+\)\s+\S+:\s")


# ---------------------------------------------------------------------------
# Serial helpers
# ---------------------------------------------------------------------------

def auto_detect_port():
    import serial.tools.list_ports as ports
    all_ports = list(ports.comports())
    if not all_ports:
        sys.exit("No serial ports found. Connect the board and specify --port.")
    # Prefer common ESP32 USB-to-UART bridge descriptions.
    keywords = ("cp210", "ch340", "ch341", "ftdi", "silicon labs", "uart")
    for p in all_ports:
        desc = (p.description or "").lower()
        if any(k in desc for k in keywords):
            return p.device
    return all_ports[0].device


def send_command(ser, command, timeout=8.0):
    """Send one command line and return everything printed before the prompt."""
    ser.reset_input_buffer()
    ser.write((command + EOL).encode())

    buf = b""
    deadline = time.time() + timeout
    while time.time() < deadline:
        chunk = ser.read(256)
        if chunk:
            buf += chunk
            if PROMPT in buf:
                idx = buf.rfind(PROMPT)
                return clean_output(buf[:idx])
        else:
            time.sleep(0.05)
    raise TimeoutError("Timed out waiting for a response to: %s" % command)


def clean_output(raw):
    """Strip the echoed command and interleaved log lines."""
    text = raw.decode("utf-8", "replace")
    lines = []
    first = True
    for line in text.splitlines():
        stripped = line.strip()
        if not stripped:
            continue
        if first and not KEYVAL_RE.match(stripped) and LOG_LINE_RE.match(stripped):
            continue  # drop log lines before the first real output
        if LOG_LINE_RE.match(stripped):
            continue  # drop log lines in general
        if first:
            first = False
            # The very first line is the echo of our own command; skip it.
            continue
        lines.append(stripped)
    return lines


def parse_get(lines):
    result = {}
    for line in lines:
        m = KEYVAL_RE.match(line)
        if m:
            result[m.group(1)] = m.group(2)
    return result


# ---------------------------------------------------------------------------
# YAML configuration support
# ---------------------------------------------------------------------------

def default_config_path():
    """Project-root config.yaml (two levels up from this script)."""
    return Path(__file__).resolve().parent.parent / "config.yaml"


def resolve_config_path(explicit):
    """Use an explicit path, or the project config.yaml if it exists."""
    path = Path(explicit) if explicit else default_config_path()
    if not path.is_file():
        if explicit:
            sys.exit("Configuration file not found: %s" % path)
        return None
    return path


def load_yaml(path):
    try:
        import yaml
    except ImportError:  # pragma: no cover
        sys.exit("PyYAML is required to read YAML config. Install it with: pip install pyyaml")
    try:
        with open(path, "r", encoding="utf-8") as f:
            return yaml.safe_load(f) or {}
    except yaml.YAMLError as exc:
        sys.exit("Failed to parse %s: %s" % (path, exc))


# ---------------------------------------------------------------------------
# Subcommands
# ---------------------------------------------------------------------------

def cmd_get(ser, args):
    lines = send_command(ser, "get")
    data = parse_get(lines)
    if args.json:
        print(json.dumps(data, indent=2, ensure_ascii=False))
    else:
        if not data:
            print("No configuration read from the device (output was):")
            for line in lines:
                print("  " + line)
        width = max(len(k) for k in data) if data else 0
        for key in sorted(data):
            print("%-*s = %s" % (width, key, data[key]))


def cmd_set(ser, args):
    pairs = []
    for item in args.kv:
        if "=" not in item:
            sys.exit("Invalid key=value pair: %s" % item)
        pairs.append(item)
    command = "set " + " ".join(pairs)
    lines = send_command(ser, command)
    for line in lines:
        print(line)


def cmd_save(ser, args):
    for line in send_command(ser, "save"):
        print(line)


def cmd_reset(ser, args):
    for line in send_command(ser, "reset"):
        print(line)


def cmd_reboot(ser, args):
    print("Rebooting the device...")
    try:
        for line in send_command(ser, "reboot", timeout=3.0):
            print(line)
    except TimeoutError:
        pass  # expected: the device restarts and drops the connection
    print("Done. Re-open the port to continue configuring.")


def cmd_status(ser, args):
    for line in send_command(ser, "status"):
        print(line)


def cmd_apply(ser, args):
    path = resolve_config_path(args.config)
    if path is None:
        sys.exit("No config file found. Create config.yaml in the project root "
                 "or pass --config <file>.")
    data = load_yaml(path)

    pairs = []
    for key, value in data.items():
        if key == "build":
            continue  # reserved for build_target.py
        if key not in KNOWN_KEYS:
            print("Warning: skipping unknown key %r" % key)
            continue
        if value is None:
            continue
        if isinstance(value, bool):
            value = int(value)
        pairs.append("%s=%s" % (key, value))

    if not pairs:
        sys.exit("No configurable keys found in %s" % path)
    print("Applying %d setting(s) from %s:" % (len(pairs), path))
    command = "set " + " ".join(pairs)
    for line in send_command(ser, command):
        print(line)

    if args.save:
        for line in send_command(ser, "save"):
            print(line)
    if args.reboot:
        cmd_reboot(ser, args)


# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="Configure the ESP32 Lixie clock over its USB serial port.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="Example: configure_clock.py --port COM5 set ssid=MyWiFi password=secret save")
    parser.add_argument("--port", "-p", default=None,
                        help="Serial port (e.g. COM5 on Windows, /dev/ttyUSB0 on Linux). "
                             "Auto-detected if omitted.")
    parser.add_argument("--baud", "-b", type=int, default=DEFAULT_BAUD,
                        help="Serial baud rate (default: %d)" % DEFAULT_BAUD)
    parser.add_argument("--config", "-c", default=None,
                        help="Path to a YAML file for the `apply` command "
                             "(default: ./config.yaml in the project root)")

    sub = parser.add_subparsers(dest="command", required=True)

    p_get = sub.add_parser("get", help="Read the current configuration")
    p_get.add_argument("--json", action="store_true", help="Output as JSON")

    p_set = sub.add_parser("set", help="Set key=value parameters")
    p_set.add_argument("kv", nargs="+", metavar="key=value",
                       help="One or more key=value pairs, e.g. ssid=MyWiFi")

    p_apply = sub.add_parser(
        "apply", help="Apply all settings from a YAML file (default: config.yaml)")
    p_apply.add_argument("--save", action="store_true",
                         help="Persist the applied settings to NVS")
    p_apply.add_argument("--reboot", action="store_true",
                         help="Reboot the device after applying")

    sub.add_parser("save", help="Persist the current configuration to NVS")
    sub.add_parser("reset", help="Restore default configuration and save it")
    sub.add_parser("reboot", help="Restart the device (applies gpio/digits)")
    sub.add_parser("status", help="Show Wi-Fi / time-sync / display status")

    args = parser.parse_args()

    port = args.port or auto_detect_port()
    ser = serial.Serial(port, args.baud, timeout=1.0)
    try:
        time.sleep(0.2)           # let the device settle
        handlers = {
            "get": cmd_get,
            "set": cmd_set,
            "apply": cmd_apply,
            "save": cmd_save,
            "reset": cmd_reset,
            "reboot": cmd_reboot,
            "status": cmd_status,
        }
        handlers[args.command](ser, args)
    finally:
        ser.close()


if __name__ == "__main__":
    main()
