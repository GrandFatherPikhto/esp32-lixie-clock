"""Unit tests for tools/build_target.py (multi-target build helper).

The build itself is never executed: subprocess.run is replaced with a stub so
we can assert on the exact idf.py command line produced by the YAML config and
CLI flags.
"""

import subprocess
import sys

import pytest

import build_target as bt


def _write_cfg(tmp_path, body):
    p = tmp_path / "cfg.yaml"
    p.write_text(body, encoding="utf-8")
    return str(p)


def _run_main(monkeypatch, argv, run_result=None):
    """Run build_target.main() with sys.argv stubbed and subprocess.run mocked."""
    recorded = {}

    def fake_run(cmd, check=True):
        recorded["cmd"] = list(cmd)
        if run_result is not None:
            raise run_result
        return None

    monkeypatch.setattr(bt.subprocess, "run", fake_run)
    monkeypatch.setattr(sys, "argv", ["build_target.py"] + argv)
    bt.main()
    return recorded


# ---------------------------------------------------------------------------
# YAML build configuration
# ---------------------------------------------------------------------------

def test_load_build_config_returns_section(tmp_path):
    path = _write_cfg(tmp_path, "ssid: x\nbuild:\n  target: esp32s3\n  clean: true\n")
    assert bt.load_build_config(path) == {"target": "esp32s3", "clean": True}


def test_load_build_config_missing_default_returns_empty(tmp_path, monkeypatch):
    monkeypatch.setattr(bt, "default_config_path", lambda: tmp_path / "none.yaml")
    assert bt.load_build_config(None) == {}


def test_load_build_config_missing_explicit_exits(tmp_path):
    with pytest.raises(SystemExit) as exc:
        bt.load_build_config(str(tmp_path / "nope.yaml"))
    assert "Configuration file not found" in str(exc.value)


def test_load_build_config_build_not_dict(tmp_path):
    path = _write_cfg(tmp_path, "build: just-a-string\n")
    assert bt.load_build_config(path) == {}


# ---------------------------------------------------------------------------
# main() command construction
# ---------------------------------------------------------------------------

def test_main_builds_default_target(monkeypatch, tmp_path):
    cfg = _write_cfg(tmp_path, "build: {}\n")
    recorded = _run_main(monkeypatch, ["--config", cfg])
    assert recorded["cmd"] == ["idf.py", "set-target", "esp32", "build"]


def test_main_target_from_config(monkeypatch, tmp_path):
    cfg = _write_cfg(tmp_path, "build:\n  target: esp32s3\n")
    recorded = _run_main(monkeypatch, ["--config", cfg])
    assert recorded["cmd"] == ["idf.py", "set-target", "esp32s3", "build"]


def test_main_cli_target_overrides_config(monkeypatch, tmp_path):
    cfg = _write_cfg(tmp_path, "build:\n  target: esp32s3\n")
    recorded = _run_main(monkeypatch, ["--config", cfg, "--target", "esp32c3"])
    assert recorded["cmd"] == ["idf.py", "set-target", "esp32c3", "build"]


def test_main_flags_appended_in_order(monkeypatch, tmp_path):
    cfg = _write_cfg(
        tmp_path,
        "build:\n"
        "  target: esp32\n"
        "  clean: true\n"
        "  menuconfig: true\n"
        "  flash: true\n"
        "  monitor: true\n",
    )
    recorded = _run_main(monkeypatch, ["--config", cfg])
    assert recorded["cmd"] == [
        "idf.py", "set-target", "esp32",
        "fullclean", "menuconfig", "build", "flash", "monitor",
    ]


def test_main_cli_flags_override_config(monkeypatch, tmp_path):
    cfg = _write_cfg(tmp_path, "build:\n  menuconfig: false\n")
    recorded = _run_main(monkeypatch, ["--config", cfg, "--menuconfig", "--clean"])
    assert recorded["cmd"][:4] == ["idf.py", "set-target", "esp32", "fullclean"]
    assert "menuconfig" in recorded["cmd"]


def test_main_unsupported_target_from_config_exits(monkeypatch, tmp_path):
    cfg = _write_cfg(tmp_path, "build:\n  target: esp99\n")
    with pytest.raises(SystemExit) as exc:
        _run_main(monkeypatch, ["--config", cfg])
    assert "Unsupported target" in str(exc.value)


def test_main_idf_not_found_exits(monkeypatch, tmp_path):
    cfg = _write_cfg(tmp_path, "build: {}\n")
    with pytest.raises(SystemExit) as exc:
        _run_main(monkeypatch, ["--config", cfg], run_result=FileNotFoundError())
    assert "idf.py was not found" in str(exc.value)


def test_main_build_failed_exits(monkeypatch, tmp_path):
    cfg = _write_cfg(tmp_path, "build: {}\n")
    with pytest.raises(SystemExit) as exc:
        _run_main(monkeypatch, ["--config", cfg],
                  run_result=subprocess.CalledProcessError(2, "idf.py"))
    assert "Build failed" in str(exc.value)
