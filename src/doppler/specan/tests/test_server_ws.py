"""Regression test for gh-475: WebSocket receive desync.

Drives a real ``doppler-specan --web`` server (subprocess) with the
``websockets`` client library and asserts that a rapid burst of incoming
commands -- the shape one slider drag produces -- is fully applied, and
that unrelated commands sent afterward still land. Before the fix,
cancelling ``ws.receive_text()`` against a 1 ms poll timeout desynced
Starlette's WebSocket receive state: a burst applied partway through and
then silently dropped everything sent afterward, on any control.

Every wait on the server is bounded, and the server child logs to a file
the test reads when something goes wrong (#2208). The helpers that do it
live in ``_server_proc``, and their guards in ``test_server_harness``, so
they run even where this module is skipped for want of the server packages.
"""

from __future__ import annotations

import asyncio
import json
import os
import subprocess
import sys
import urllib.request

import pytest

from doppler.specan.tests._server_proc import (
    DRIVE_TIMEOUT,
    free_port,
    log_tail,
    recv,
    wait_for_state,
)

pytest.importorskip("fastapi")
pytest.importorskip("uvicorn")
websockets = pytest.importorskip("websockets")


@pytest.fixture
def specan_server(tmp_path):
    """Run ``doppler.specan --web`` on a free port; yield ``(port, log)``."""
    port = free_port()
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
            wait_for_state(f"http://127.0.0.1:{port}/state", proc, log)
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


async def _drive(port: int) -> dict:
    uri = f"ws://127.0.0.1:{port}/ws"
    async with websockets.connect(uri) as ws:
        await recv(ws)  # drain one frame so the connection is live

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
        result = asyncio.run(asyncio.wait_for(_drive(port), DRIVE_TIMEOUT))
    except Exception as exc:
        # A bounded failure is only useful if it says why.
        raise AssertionError(
            f"driving the server failed: {exc!r}\n"
            f"server output:\n{log_tail(log)}"
        ) from exc

    assert result["burst_final_fn"] == pytest.approx(
        result["expected_fn"], abs=1e-9
    )
    assert result["final"]["demo"]["tone_power"] == pytest.approx(-7.0)
    assert result["final"]["demo"]["noise_floor"] == pytest.approx(-55.0)
