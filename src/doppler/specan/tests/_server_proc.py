"""Helpers for driving a real ``doppler-specan --web`` child in tests.

They live apart from ``test_server_ws.py`` because that module opens with
``pytest.importorskip`` on the server packages, which skips everything in it
when they are absent. These helpers need none of them: ``recv`` takes any
object with an awaitable ``recv``, and ``wait_for_state`` needs only the
standard library. Keeping them here lets ``test_server_harness.py`` prove the
bounds hold on every leg, whether or not the server packages are installed.

Every wait is bounded (#2208). The suite has no per-test timeout, so an
unbounded wait stalls a CI leg until its job cap instead of failing in
seconds. The server child logs to a file, not a pipe: an unread pipe blocks
the child once its buffer fills, and a startup traceback sits in it unread.
The tests read the file when something goes wrong, so a bounded failure says
why.
"""

from __future__ import annotations

import asyncio
import json
import socket
import time
import urllib.request
from typing import TYPE_CHECKING, Any

if TYPE_CHECKING:
    import subprocess
    from pathlib import Path

# Deadlines, in seconds. Each is far longer than a healthy run needs and far
# shorter than the CI job cap a hang would otherwise run into. Readiness gets
# the longest: ``urlopen error timed out`` fits a slow Windows cold start
# (imports, then uvicorn) as well as a dead child, and a dead child is caught
# at once by the exit check in ``wait_for_state``, not by this deadline.
READY_TIMEOUT = 60.0
RECV_TIMEOUT = 10.0
DRIVE_TIMEOUT = 60.0


def free_port() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def log_tail(log: Path, limit: int = 4000) -> str:
    """Return the last ``limit`` characters the server wrote to ``log``.

    ``errors="replace"`` keeps one undecodable byte from hiding the rest of
    the output, which is the point of reading it at all.
    """
    text = log.read_text(encoding="utf-8", errors="replace")
    return text[-limit:] if text else "(the server wrote nothing)"


def wait_for_state(
    url: str,
    proc: subprocess.Popen[bytes],
    log: Path,
    timeout: float = READY_TIMEOUT,
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
                f"{log_tail(log)}"
            )
        try:
            with urllib.request.urlopen(url, timeout=0.5) as resp:
                if json.loads(resp.read()).get("demo"):
                    return
        except Exception as exc:
            last_exc = exc
        time.sleep(0.2)
    raise TimeoutError(
        f"server never became ready: {last_exc}\n{log_tail(log)}"
    )


async def recv(ws: Any, timeout: float = RECV_TIMEOUT) -> Any:
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
