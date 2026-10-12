"""Offline guards for the server tests' bounds (#2208) and the CLI's output.

``test_server_ws.py`` skips as a whole when the server packages are absent,
and the guards here need none of them: a fake socket, a ``python -c`` child,
and ``doppler.specan --help``. They live here, with no ``importorskip``, so
they run on every leg and a bound that stops bounding, or a CLI that crashes
on a stdout it cannot encode (#2225), turns CI red rather than silently
relying on a skipped file.
"""

from __future__ import annotations

import asyncio
import os
import subprocess
import sys
from typing import Any

import pytest

from doppler.specan.tests._server_proc import (
    READY_TIMEOUT,
    free_port,
    recv,
    wait_for_state,
)


class _SilentSocket:
    """A websocket that is connected and never sends a frame."""

    async def recv(self) -> str:
        await asyncio.sleep(3600)
        return ""


def test_a_silent_server_fails_the_receive_instead_of_hanging():
    async def receive_from_a_silent_server() -> Any:
        # The outer bound is this test's own guard: were ``recv`` to stop
        # bounding, this fails in seconds with a different error instead of
        # hanging at the CI job cap.
        return await asyncio.wait_for(
            recv(_SilentSocket(), timeout=0.2), timeout=5.0
        )

    with pytest.raises(TimeoutError, match=r"no frame within 0\.2 s"):
        asyncio.run(receive_from_a_silent_server())


def test_a_server_that_dies_at_startup_reports_what_it_printed(tmp_path):
    log = tmp_path / "server.log"
    with log.open("wb") as out:
        proc = subprocess.Popen(
            [
                sys.executable,
                "-c",
                "import sys; print('banner-marker'); sys.exit(3)",
            ],
            stdout=out,
            stderr=subprocess.STDOUT,
        )
        try:
            with pytest.raises(
                RuntimeError, match=r"(?s)code 3.*banner-marker"
            ):
                wait_for_state(
                    f"http://127.0.0.1:{free_port()}/state",
                    proc,
                    log,
                    timeout=READY_TIMEOUT,
                )
        finally:
            proc.wait(timeout=10)


def test_the_cli_starts_when_stdout_cannot_encode_its_help():
    # ``--help`` prints the parser's description, whose em dash an ASCII
    # stdout cannot encode. It needs none of the server packages, so this
    # guards the fix for #2225 on every leg: before it, the print raised
    # ``UnicodeEncodeError`` and the process exited with code 1.
    result = subprocess.run(
        [sys.executable, "-m", "doppler.specan", "--help"],
        env={**os.environ, "PYTHONIOENCODING": "ascii"},
        capture_output=True,
        timeout=60,
        check=False,
    )
    assert result.returncode == 0, result.stderr.decode("ascii", "replace")
    assert b"\\u2014" in result.stdout
