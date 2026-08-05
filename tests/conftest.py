"""Shared test configuration for the clock tooling tests.

The helper scripts in tools/ are plain scripts (not a package), so we add
tools/ to sys.path and expose a minimal pyserial stand-in. Running from the
project root:  .venv\\Scripts\\python -m pytest tests -v
"""

import sys
from pathlib import Path

import pytest

TOOLS_DIR = Path(__file__).resolve().parent.parent / "tools"
if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLS_DIR))

import configure_clock  # noqa: E402  (needs pyserial from the project .venv)


class FakeSerial:
    """Minimal pyserial.Serial stand-in.

    ``responses`` maps a command line (without the trailing EOL) to the bytes
    the device prints back. The response should include the echoed command
    line as its first line (mirroring the real firmware); the trailing prompt
    is appended automatically because send_command() looks for it.
    """

    PROMPT = configure_clock.PROMPT

    def __init__(self, responses=None, auto_ack=False):
        self.responses = responses or {}
        self.auto_ack = auto_ack
        self.written = []
        self.reset_count = 0
        self._buffer = b""

    def reset_input_buffer(self):
        self.reset_count += 1
        self._buffer = b""

    def write(self, data):
        self.written.append(data)
        command = data.decode("utf-8").strip()
        response = self.responses.get(command)
        if response is None and self.auto_ack:
            # Acknowledge any (e.g. chunked) command without registering it.
            response = (command + "\r\nok\r\n").encode()
        # Only append the prompt when a response exists; otherwise leave the
        # buffer empty so send_command() can time out (used by the tests).
        self._buffer = (response + self.PROMPT) if response is not None else b""

    def read(self, size=256):
        chunk = self._buffer[:size]
        self._buffer = self._buffer[size:]
        return chunk

    def close(self):
        pass


@pytest.fixture
def make_serial():
    """Fixture returning a factory that builds FakeSerial instances."""

    def _make(responses=None):
        return FakeSerial(responses)

    return _make
