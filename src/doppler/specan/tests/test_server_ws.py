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
import contextlib
import json
import os
import re
import subprocess
import sys
import urllib.request
from typing import Any

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


@contextlib.contextmanager
def _run_server(tmp_path, extra_env):
    """Run ``doppler.specan --web`` on a free port; yield ``(port, log)``.

    ``extra_env`` is added to the child's environment. The child's output
    goes to a file the test reads when something goes wrong.
    """
    port = free_port()
    log = tmp_path / "server.log"
    # Unbuffered, so a child that hangs mid-startup still shows how far it got:
    # with stdout redirected to a file it is block-buffered, and the banner
    # would sit in the child's memory instead of in the log.
    env = {**os.environ, "PYTHONUNBUFFERED": "1", **extra_env}
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


@pytest.fixture
def specan_server(tmp_path):
    """The server on a free port; yields ``(port, log)``."""
    with _run_server(tmp_path, {}) as server:
        yield server


@pytest.fixture(params=["cp1252", "cp1252:surrogateescape"])
def specan_server_cp1252(tmp_path, request):
    """The server with a stdout that can only encode cp1252 (#2225).

    A redirected stdout takes the legacy code page on Windows, so the child
    there already runs this way. ``PYTHONIOENCODING`` gives every other
    platform the same stdout, which is what makes the regression reproducible
    on Linux. Two handlers are covered: ``cp1252`` alone gives ``strict``, and
    ``cp1252:surrogateescape`` is what the Windows runner measured for a
    stdout redirected to a file (Python 3.12.10, UTF-8 mode off). The second
    is the one a fix keyed on ``strict`` alone missed.
    """
    env = {"PYTHONIOENCODING": request.param}
    with _run_server(tmp_path, env) as server:
        yield server


def _get_state(port: int) -> dict:
    with urllib.request.urlopen(
        f"http://127.0.0.1:{port}/state", timeout=2.0
    ) as resp:
        return json.loads(resp.read())


async def _drain(ws: Any) -> None:
    """Read and discard frames until cancelled, as a browser does.

    The server streams about 30 frames a second whether or not anyone reads.
    A client that stops reading lets its receive queue fill, which pauses its
    reading, and then it cannot see the server's close frame: ``close()``
    waits out the whole ``close_timeout`` (10 s) and ends with code 1006.
    Against this server (websockets 16.0) ``close()`` took 10.01 s that way
    and 0.00 s with a reader running, so the server answers a close promptly
    and the wait was this client's. ``_drive`` cancels the reader just before
    it leaves the ``async with`` that closes the connection, too short a gap
    for the queue to fill again, and then checks the close code.
    """
    async for _ in ws:
        pass


async def _drive(port: int) -> dict:
    uri = f"ws://127.0.0.1:{port}/ws"
    async with websockets.connect(uri) as ws:
        await recv(ws)  # one frame, bounded: the connection is live

        # Read frames in the background for the rest of the drive. An abnormal
        # drop mid-drive raises when the reader is awaited in the ``finally``;
        # a clean close only ends its loop, so it is the close code, checked
        # after the block, that catches a close which stalls.
        drainer = asyncio.create_task(_drain(ws))
        try:
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

            # Unrelated controls sent well after the burst must still land
            # -- this is the part that stayed broken even after the burst's
            # own tail command got through, pre-fix.
            await ws.send(json.dumps({"tone_power": -7.0}))
            await asyncio.sleep(0.2)
            await ws.send(json.dumps({"noise_floor": -55.0}))
            await asyncio.sleep(0.3)

            result = {
                "burst_final_fn": state_after_burst["demo"]["tone_freq"],
                "expected_fn": -0.45 + (n - 1) * (0.9 / (n - 1)),
                "final": _get_state(port),
            }
        finally:
            drainer.cancel()
            with contextlib.suppress(asyncio.CancelledError):
                await drainer

    # Leaving the block closed the connection. websockets 16.0 swallows the
    # ConnectionClosed it raises after ``close_timeout`` aborts the transport,
    # so a close that stalls would only make this test 10 s slower and end
    # with code 1006: the code is what shows the handshake completed.
    assert ws.close_code == 1000, (
        f"the close handshake did not complete cleanly "
        f"(close code {ws.close_code})"
    )
    return result


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


def test_the_server_starts_when_stdout_cannot_encode_its_banner(
    specan_server_cp1252,
):
    # The fixture already failed with the child's own traceback if the server
    # did not come up. Serving is not enough, though: something else could
    # answer ``/state`` on a port this child never bound (a ``free_port``
    # race under xdist). The banner in the log shows that this child ran
    # under cp1252 and that the fix substituted the arrow it cannot encode.
    port, log = specan_server_cp1252
    assert _get_state(port)["demo"]
    text = log.read_text(encoding="utf-8", errors="replace")
    assert re.search(r"doppler specan\s+\\u2192\s+http", text), text
