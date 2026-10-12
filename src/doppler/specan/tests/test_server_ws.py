"""Regression test for gh-475: WebSocket receive desync.

Drives a real ``doppler-specan --web`` server (subprocess) with the
``websockets`` client library and asserts that a rapid burst of incoming
commands -- the shape one slider drag produces -- is fully applied, and
that unrelated commands sent afterward still land. Before the fix,
cancelling ``ws.receive_text()`` against a 1 ms poll timeout desynced
Starlette's WebSocket receive state: a burst applied partway through and
then silently dropped everything sent afterward, on any control.

Every wait on the server is bounded (#2208). The suite has no per-test
timeout, so an unbounded ``recv`` against a server that never sends would
stall a CI leg until its job cap instead of failing in seconds. The server
child logs to a file, not a pipe: an unread pipe blocks the child once its
buffer fills, and a startup traceback sits in it unread. The test reads the
file when something goes wrong, so a bounded failure says why.
"""

from __future__ import annotations

import asyncio
import json
import os
import socket
import subprocess
import sys
import time
import urllib.request
from typing import TYPE_CHECKING, Any

import pytest

if TYPE_CHECKING:
    from pathlib import Path

pytest.importorskip("fastapi")
pytest.importorskip("uvicorn")
websockets = pytest.importorskip("websockets")

# Deadlines, in seconds. Each is far longer than a healthy run needs and far
# shorter than the CI job cap a hang would otherwise run into. Readiness gets
# the longest: ``urlopen error timed out`` fits a slow Windows cold start
# (imports, then uvicorn) as well as a dead child, and a dead child is caught
# at once by the exit check in ``_wait_for_state``, not by this deadline.
_READY_TIMEOUT = 60.0
_RECV_TIMEOUT = 10.0
_DRIVE_TIMEOUT = 60.0


def _free_port() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def _log_tail(log: Path, limit: int = 4000) -> str:
    """Return the last ``limit`` characters the server wrote to ``log``.

    ``errors="replace"`` keeps one undecodable byte from hiding the rest of
    the output, which is the point of reading it at all.
    """
    text = log.read_text(encoding="utf-8", errors="replace")
    return text[-limit:] if text else "(the server wrote nothing)"


def _wait_for_state(
    url: str,
    proc: subprocess.Popen[bytes],
    log: Path,
    timeout: float = _READY_TIMEOUT,
) -> None:
    """Block until ``url`` reports the demo source, or fail with the log.

    Fails at once, rather than after ``timeout``, when ``proc`` has already
    exited: a server that died at startup will never answer, and what it
    printed on the way out is the diagnosis. Either failure carries the tail
    of ``log``.
    """
    deadline = time.monotonic() + timeout
    last_exc: Exception | None = None
    while time.monotonic() < deadline:
        code = proc.poll()
        if code is not None:
            raise RuntimeError(
                f"server exited with code {code} before it was ready:\n"
                f"{_log_tail(log)}"
            )
        try:
            with urllib.request.urlopen(url, timeout=0.5) as resp:
                if json.loads(resp.read()).get("demo"):
                    return
        except Exception as exc:
            last_exc = exc
        time.sleep(0.2)
    raise TimeoutError(
        f"server never became ready: {last_exc}\n{_log_tail(log)}"
    )


@pytest.fixture
def specan_server(tmp_path):
    """Run ``doppler.specan --web`` on a free port; yield ``(port, log)``."""
    port = _free_port()
    log = tmp_path / "server.log"
    # Unbuffered, so a child that hangs mid-startup still shows how far it got:
    # with stdout redirected to a file it is block-buffered, and the banner
    # would sit in the child's memory instead of in the log.
    env = {**os.environ, "PYTHONUNBUFFERED": "1"}
    with log.open("wb") as out:
        proc = subprocess.Popen(
            [
                sys.executable,
                "-m",
                "doppler.specan",
                "--source",
                "demo",
                "--web",
                "--no-browser",
                "--host",
                "127.0.0.1",
                "--port",
                str(port),
            ],
            stdout=out,
            stderr=subprocess.STDOUT,
            env=env,
        )
        try:
            _wait_for_state(f"http://127.0.0.1:{port}/state", proc, log)
            yield port, log
        finally:
            proc.terminate()
            try:
                proc.wait(timeout=10)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait(timeout=10)


def _get_state(port: int) -> dict:
    with urllib.request.urlopen(
        f"http://127.0.0.1:{port}/state", timeout=2.0
    ) as resp:
        return json.loads(resp.read())


async def _recv(ws: Any, timeout: float = _RECV_TIMEOUT) -> Any:
    """Receive one websocket message, or raise ``TimeoutError``.

    A bare ``await ws.recv()`` waits for as long as the server stays silent,
    which is forever if it hung. Raising the builtin ``TimeoutError`` on
    every supported Python (``asyncio.TimeoutError`` is a different class
    before 3.11) keeps one failure shape for the caller to report.
    """
    try:
        return await asyncio.wait_for(ws.recv(), timeout)
    except asyncio.TimeoutError as exc:
        raise TimeoutError(f"no frame within {timeout} s") from exc


async def _drive(port: int) -> dict:
    uri = f"ws://127.0.0.1:{port}/ws"
    async with websockets.connect(uri) as ws:
        await _recv(ws)  # drain one frame so the connection is live

        # A rapid burst mimicking one slider drag: many messages, no
        # pause -- this is exactly the pattern that used to desync the
        # receive state after the first command or two.
        n = 60
        for i in range(n):
            fn = -0.45 + i * (0.9 / (n - 1))
            await ws.send(json.dumps({"tone_freq": fn}))
            await asyncio.sleep(0.005)

        await asyncio.sleep(0.3)
        state_after_burst = _get_state(port)

        # Unrelated controls sent well after the burst must still land --
        # this is the part that stayed broken even after the burst's own
        # tail command got through, pre-fix.
        await ws.send(json.dumps({"tone_power": -7.0}))
        await asyncio.sleep(0.2)
        await ws.send(json.dumps({"noise_floor": -55.0}))
        await asyncio.sleep(0.3)

        return {
            "burst_final_fn": state_after_burst["demo"]["tone_freq"],
            "expected_fn": -0.45 + (n - 1) * (0.9 / (n - 1)),
            "final": _get_state(port),
        }


def test_rapid_burst_fully_applied(specan_server):
    port, log = specan_server
    try:
        result = asyncio.run(asyncio.wait_for(_drive(port), _DRIVE_TIMEOUT))
    except Exception as exc:
        # A bounded failure is only useful if it says why.
        raise AssertionError(
            f"driving the server failed: {exc!r}\n"
            f"server output:\n{_log_tail(log)}"
        ) from exc

    assert result["burst_final_fn"] == pytest.approx(
        result["expected_fn"], abs=1e-9
    )
    assert result["final"]["demo"]["tone_power"] == pytest.approx(-7.0)
    assert result["final"]["demo"]["noise_floor"] == pytest.approx(-55.0)


class _SilentSocket:
    """A websocket that is connected and never sends a frame."""

    async def recv(self) -> str:
        await asyncio.sleep(3600)
        return ""


def test_a_silent_server_fails_the_receive_instead_of_hanging():
    async def receive_from_a_silent_server() -> Any:
        # The outer bound is this test's own guard: were ``_recv`` to stop
        # bounding, this fails in seconds with a different error instead of
        # hanging at the CI job cap.
        return await asyncio.wait_for(
            _recv(_SilentSocket(), timeout=0.2), timeout=5.0
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
                _wait_for_state(
                    f"http://127.0.0.1:{_free_port()}/state",
                    proc,
                    log,
                    timeout=_READY_TIMEOUT,
                )
        finally:
            proc.wait(timeout=10)
