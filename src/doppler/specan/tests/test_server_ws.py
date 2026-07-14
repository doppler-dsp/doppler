"""Regression test for gh-475: WebSocket receive desync.

Drives a real ``doppler-specan --web`` server (subprocess) with the
``websockets`` client library and asserts that a rapid burst of incoming
commands -- the shape one slider drag produces -- is fully applied, and
that unrelated commands sent afterward still land. Before the fix,
cancelling ``ws.receive_text()`` against a 1 ms poll timeout desynced
Starlette's WebSocket receive state: a burst applied partway through and
then silently dropped everything sent afterward, on any control.
"""

from __future__ import annotations

import asyncio
import json
import socket
import subprocess
import sys
import time
import urllib.request

import pytest

pytest.importorskip("fastapi")
pytest.importorskip("uvicorn")
websockets = pytest.importorskip("websockets")


def _free_port() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def _wait_for_state(url: str, timeout: float = 15.0) -> None:
    deadline = time.monotonic() + timeout
    last_exc: Exception | None = None
    while time.monotonic() < deadline:
        try:
            with urllib.request.urlopen(url, timeout=0.5) as resp:
                if json.loads(resp.read()).get("demo"):
                    return
        except Exception as exc:
            last_exc = exc
        time.sleep(0.2)
    raise TimeoutError(f"server never became ready: {last_exc}")


@pytest.fixture
def specan_server():
    port = _free_port()
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
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    try:
        _wait_for_state(f"http://127.0.0.1:{port}/state")
        yield port
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


async def _drive(port: int) -> dict:
    uri = f"ws://127.0.0.1:{port}/ws"
    async with websockets.connect(uri) as ws:
        await ws.recv()  # drain one frame so the connection is live

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
    port = specan_server
    result = asyncio.run(_drive(port))

    assert result["burst_final_fn"] == pytest.approx(
        result["expected_fn"], abs=1e-9
    )
    assert result["final"]["demo"]["tone_power"] == pytest.approx(-7.0)
    assert result["final"]["demo"]["noise_floor"] == pytest.approx(-55.0)
