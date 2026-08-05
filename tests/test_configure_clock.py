"""Unit tests for tools/configure_clock.py (USB console configuration tool).

No ESP32 hardware is required: the serial port is replaced by a FakeSerial
stub and the YAML config is supplied via temp files.
"""

import argparse
import json

import pytest

import configure_clock as cc
from conftest import FakeSerial


def _ns(**kwargs):
    """Build an argparse.Namespace with the attributes cmd_* functions use."""
    defaults = dict(config=None, json=False, save=False, reboot=False, kv=[])
    defaults.update(kwargs)
    return argparse.Namespace(**defaults)


def _get_response():
    return (
        b"get\r\n"
        b"ssid = MyWiFi\r\n"
        b"password = secret\r\n"
        b"ntp_server = pool.ntp.org\r\n"
        b"brightness = 100\r\n"
    )


# ---------------------------------------------------------------------------
# YAML configuration support
# ---------------------------------------------------------------------------

def test_default_config_path_points_at_project_root():
    path = cc.default_config_path()
    assert path.name == "config.yaml"
    assert path.parent.name == "clock"  # the project root directory
    # The real project config.yaml should exist and resolve.
    assert cc.resolve_config_path(None) == path


def test_resolve_config_path_explicit_exists(tmp_path):
    p = tmp_path / "settings.yaml"
    p.write_text("ssid: test\n", encoding="utf-8")
    assert cc.resolve_config_path(str(p)) == p


def test_resolve_config_path_missing_explicit_exits(tmp_path):
    with pytest.raises(SystemExit) as exc:
        cc.resolve_config_path(str(tmp_path / "nope.yaml"))
    assert "Configuration file not found" in str(exc.value)


def test_load_yaml(tmp_path):
    p = tmp_path / "cfg.yaml"
    p.write_text("ssid: test\nbrightness: 50\n", encoding="utf-8")
    assert cc.load_yaml(p) == {"ssid": "test", "brightness": 50}


def test_load_yaml_invalid_exits(tmp_path):
    p = tmp_path / "bad.yaml"
    p.write_text("ssid: [unclosed\n", encoding="utf-8")
    with pytest.raises(SystemExit) as exc:
        cc.load_yaml(p)
    assert "Failed to parse" in str(exc.value)


# ---------------------------------------------------------------------------
# Output parsing
# ---------------------------------------------------------------------------

def test_clean_output_drops_echo_and_log_lines():
    raw = (
        b"I (123) wifi_mgr: connecting\r\n"
        b"get\r\n"
        b"W (1) app: some log\r\n"
        b"ssid = MyWiFi\r\n"
        b"brightness = 100\r\n"
    )
    assert cc.clean_output(raw) == ["ssid = MyWiFi", "brightness = 100"]


def test_parse_get_ignores_non_kv_lines():
    lines = ["ssid = MyWiFi", "not a kv line", " brightness = 100 ", "hue = 200"]
    assert cc.parse_get(lines) == {"ssid": "MyWiFi", "brightness": "100", "hue": "200"}


# ---------------------------------------------------------------------------
# Serial protocol
# ---------------------------------------------------------------------------

def test_send_command_sends_command_and_stops_at_prompt():
    ser = FakeSerial({"get": b"get\r\nssid = MyWiFi\r\n"})
    lines = cc.send_command(ser, "get")
    assert ser.written[-1] == b"get\r\n"
    assert lines == ["ssid = MyWiFi"]


def test_send_command_times_out_when_no_response(monkeypatch):
    state = {"t": 0.0}

    def fake_time():
        state["t"] += 1.0
        return state["t"]

    monkeypatch.setattr(cc.time, "time", fake_time)
    monkeypatch.setattr(cc.time, "sleep", lambda _s: None)
    ser = FakeSerial({})
    with pytest.raises(TimeoutError):
        cc.send_command(ser, "get")


# ---------------------------------------------------------------------------
# Subcommands
# ---------------------------------------------------------------------------

def test_cmd_get_plain(capsys):
    ser = FakeSerial({"get": _get_response()})
    cc.cmd_get(ser, _ns())
    out = capsys.readouterr().out
    # cmd_get pads keys to a common width, so check the substrings separately.
    assert "ssid" in out and "MyWiFi" in out
    assert "brightness = 100" in out


def test_cmd_get_json(capsys):
    ser = FakeSerial({"get": _get_response()})
    cc.cmd_get(ser, _ns(json=True))
    data = json.loads(capsys.readouterr().out)
    assert data == {"ssid": "MyWiFi", "password": "secret",
                    "ntp_server": "pool.ntp.org", "brightness": "100"}


def test_cmd_set_sends_joined_pairs(capsys):
    command = "set ssid=MyWiFi password=secret"
    ser = FakeSerial({command: (command + "\r\nssid = MyWiFi\r\n").encode()})
    cc.cmd_set(ser, _ns(kv=["ssid=MyWiFi", "password=secret"]))
    assert ser.written[-1] == (command + "\r\n").encode()
    assert "ssid = MyWiFi" in capsys.readouterr().out


def test_cmd_set_invalid_pair_exits():
    ser = FakeSerial({})
    with pytest.raises(SystemExit) as exc:
        cc.cmd_set(ser, _ns(kv=["no_equals_sign"]))
    assert "Invalid key=value pair" in str(exc.value)


def test_cmd_save(capsys):
    ser = FakeSerial({"save": b"save\r\nconfig saved\r\n"})
    cc.cmd_save(ser, _ns())
    assert ser.written[-1] == b"save\r\n"
    assert "config saved" in capsys.readouterr().out


def test_cmd_reset(capsys):
    ser = FakeSerial({"reset": b"reset\r\ndefaults restored\r\n"})
    cc.cmd_reset(ser, _ns())
    assert ser.written[-1] == b"reset\r\n"
    assert "defaults restored" in capsys.readouterr().out


def test_cmd_status(capsys):
    ser = FakeSerial({"status": b"status\r\nwifi: connected\r\n"})
    cc.cmd_status(ser, _ns())
    assert ser.written[-1] == b"status\r\n"
    assert "wifi: connected" in capsys.readouterr().out


def test_cmd_reboot_handles_timeout_gracefully(monkeypatch, capsys):
    state = {"t": 0.0}

    def fake_time():
        state["t"] += 1.0
        return state["t"]

    monkeypatch.setattr(cc.time, "time", fake_time)
    monkeypatch.setattr(cc.time, "sleep", lambda _s: None)
    cc.cmd_reboot(FakeSerial({}), _ns())
    out = capsys.readouterr().out
    assert "Rebooting" in out
    assert "Done." in out


def test_cmd_apply_builds_set_command(tmp_path, capsys):
    cfg = tmp_path / "cfg.yaml"
    cfg.write_text(
        "ssid: MyWiFi\n"
        "password: secret\n"
        "brightness: 50\n"
        "color_mode: true\n"     # bool -> int
        "hue: 200\n"
        "unknown_key: x\n"       # skipped with a warning
        "build:\n  target: esp32\n"  # reserved for build_target.py
        "",
        encoding="utf-8",
    )
    expected = "set ssid=MyWiFi password=secret brightness=50 color_mode=1 hue=200"
    ser = FakeSerial({expected: (expected + "\r\nok\r\n").encode()})
    cc.cmd_apply(ser, _ns(config=str(cfg)))
    assert ser.written[-1] == (expected + "\r\n").encode()
    out = capsys.readouterr().out
    assert "Applying 5 setting(s)" in out
    assert "unknown_key" in out  # the skip warning


def test_cmd_apply_save_and_reboot(tmp_path, capsys):
    cfg = tmp_path / "cfg.yaml"
    cfg.write_text("ssid: MyWiFi\n", encoding="utf-8")
    command = "set ssid=MyWiFi"
    ser = FakeSerial({
        command: (command + "\r\nok\r\n").encode(),
        "save": b"save\r\nconfig saved\r\n",
        "reboot": b"reboot\r\n",
    })
    cc.cmd_apply(ser, _ns(config=str(cfg), save=True, reboot=True))
    commands = [w.decode().strip() for w in ser.written]
    assert commands == [command, "save", "reboot"]
    out = capsys.readouterr().out
    assert "config saved" in out
    assert "Rebooting" in out


def test_cmd_apply_no_config_file_exits(monkeypatch, tmp_path):
    monkeypatch.setattr(cc, "default_config_path", lambda: tmp_path / "missing.yaml")
    with pytest.raises(SystemExit) as exc:
        cc.cmd_apply(FakeSerial({}), _ns())
    assert "No config file found" in str(exc.value)


def test_cmd_apply_no_usable_keys_exits(tmp_path):
    cfg = tmp_path / "cfg.yaml"
    cfg.write_text("build:\n  target: esp32\n", encoding="utf-8")
    with pytest.raises(SystemExit) as exc:
        cc.cmd_apply(FakeSerial({}), _ns(config=str(cfg)))
    assert "No configurable keys found" in str(exc.value)


# ---------------------------------------------------------------------------
# Port auto-detection
# ---------------------------------------------------------------------------

def test_auto_detect_port_prefers_bridge(monkeypatch):
    class Port:
        def __init__(self, device, description):
            self.device = device
            self.description = description

    ports = [
        Port("COM1", "Standard Serial over Bluetooth link"),
        Port("COM5", "Silicon Labs CP210x USB to UART Bridge"),
    ]
    monkeypatch.setattr("serial.tools.list_ports.comports", lambda: ports)
    assert cc.auto_detect_port() == "COM5"


def test_auto_detect_port_falls_back_to_first(monkeypatch):
    class Port:
        def __init__(self, device, description):
            self.device = device
            self.description = description

    ports = [Port("COM3", "Some Random Device")]
    monkeypatch.setattr("serial.tools.list_ports.comports", lambda: ports)
    assert cc.auto_detect_port() == "COM3"


def test_auto_detect_port_no_ports_exits(monkeypatch):
    monkeypatch.setattr("serial.tools.list_ports.comports", lambda: [])
    with pytest.raises(SystemExit) as exc:
        cc.auto_detect_port()
    assert "No serial ports found" in str(exc.value)
