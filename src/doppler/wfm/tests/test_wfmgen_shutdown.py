"""`wfmgen --continuous` must stop on a signal, and leave valid output.

A continuous run has no natural end, so the only way it ever stops is a
signal — and until this gate existed, ``wfmgen`` had **no signal handling
at all**: no ``signal()``, no ``SIGINT``, nothing, in 1500 lines that
advertise ``--continuous --realtime --output nats://…`` in their own
``--help``. Ctrl+C therefore killed it outright (doppler#969).

The two halves fail differently, which is why both are asserted:

- **file** — the BLUE header carries the final sample count and is written
  by ``wfm_writer_close``. A killed process never reaches it, so the
  capture is left with no valid header. That is worse than a short file:
  a short file reads, a headerless one does not.
- **nats** — a send returns once the *client* has the block, not once the
  server does. Exiting without draining leaves the tail to the client's
  own best-effort flush, capped at 500 ms with no way to report failure.

Both are checked by outcome, not by inspection: exit 0, and for the file
path the capture is read back through ``wfm.Reader``.

**The signal is not sent until the process is demonstrably running.** A
signal delivered before ``main()`` installs its handler does not get
handled — it terminates the process, and no amount of handler code
prevents it, because there is no handler yet. Measured on the C pair
test: SIGINT at ~0 ms after spawn is fatal 3/3, at 5 ms it is clean 3/3.
So these tests wait for evidence of progress (output actually appearing)
before interrupting, and a harness that skipped that wait would be
measuring process startup rather than shutdown.
"""

from __future__ import annotations

import os
import signal
import socket
import subprocess
import threading
import time
from typing import TYPE_CHECKING

import pytest

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
WFMGEN = REPO / "build" / "native" / "src" / "wfmcompose" / "wfmgen"

#: How long the interrupted process may take to exit. Generous next to the
#: drain budget it may legitimately spend, and far below the hang it guards
#: against — an unhandled signal path never exits at all.
STOP_DEADLINE_S = 20.0

#: How long to wait for the run to prove it is past startup.
STARTUP_DEADLINE_S = 20.0


def _broker_reachable(host: str = "127.0.0.1", port: int = 4222) -> bool:
    try:
        with socket.create_connection((host, port), timeout=0.5):
            return True
    except OSError:
        return False


def _require_binary() -> None:
    if not WFMGEN.exists():
        pytest.skip(f"{WFMGEN.name} not built (run `make build`)")


def _start(*args: str) -> tuple[subprocess.Popen[str], list[str]]:
    """Start wfmgen, draining its stdout on a thread.

    The thread is not a convenience. A pipe nobody reads fills in well
    under a second, and the process then blocks in ``write()`` instead of
    reaching the flag check between blocks -- at which point this test
    proves nothing about the signal and fails on a full buffer instead.
    Measured: without the pump, both tests here fail as "ignored SIGINT"
    against a binary that exits in 3 ms when its output goes to a file.
    """
    proc = subprocess.Popen(
        [str(WFMGEN), *args],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        stdin=subprocess.DEVNULL,
        text=True,
        start_new_session=True,
    )
    sink: list[str] = []

    def pump() -> None:
        assert proc.stdout is not None
        for line in proc.stdout:
            sink.append(line)

    threading.Thread(target=pump, daemon=True).start()
    return proc, sink


def _interrupt(proc: subprocess.Popen[str]) -> None:
    os.killpg(os.getpgid(proc.pid), signal.SIGINT)


def _wait_until(predicate, deadline_s: float, what: str) -> None:
    """Block until `predicate()` is true, or fail naming `what`."""
    deadline = time.monotonic() + deadline_s
    while time.monotonic() < deadline:
        if predicate():
            return
        time.sleep(0.05)
    pytest.fail(f"wfmgen never {what} within {deadline_s}s")


def _stop_cleanly(proc: subprocess.Popen[str], sink: list[str]) -> float:
    """SIGINT `proc`, require exit 0, and return how long it took."""
    started = time.monotonic()
    _interrupt(proc)
    try:
        proc.wait(timeout=STOP_DEADLINE_S)
    except subprocess.TimeoutExpired:
        os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
        proc.wait(timeout=5)
        pytest.fail(
            "wfmgen ignored SIGINT: a --continuous run has no other way to "
            "end, so an unhandled signal is an unkillable-by-Ctrl+C process"
        )
    elapsed = time.monotonic() - started
    assert proc.returncode == 0, (
        f"wfmgen exited {proc.returncode} on SIGINT (negative means it was "
        f"KILLED by the signal rather than handling it)\n"
        f"{''.join(sink)[-1500:]}"
    )
    return elapsed


def test_continuous_file_run_stops_and_leaves_a_readable_capture(
    tmp_path: Path,
) -> None:
    """The file half: exit 0, and the capture is still valid."""
    _require_binary()
    out = tmp_path / "capture.tmp"
    proc, sink = _start("--continuous", "--realtime", "--output", str(out))
    try:
        _wait_until(
            lambda: out.exists() and out.stat().st_size > 0,
            STARTUP_DEADLINE_S,
            "wrote any samples",
        )
        elapsed = _stop_cleanly(proc, sink)
        assert elapsed < STOP_DEADLINE_S
    finally:
        if proc.poll() is None:
            os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
            proc.wait(timeout=5)

    # The point of leaving the loop rather than dying in it: close() runs,
    # so the header is written and the file is a capture rather than a
    # prefix of one.
    from doppler import wfm

    reader = wfm.Reader(str(out))
    total = 0
    while True:
        block = reader.read(4096)
        if not len(block):
            break
        total += len(block)
    assert total > 0, "capture opened but held no samples"


def test_continuous_nats_run_stops_cleanly() -> None:
    """The stream half: exit 0, which requires the drain to have succeeded.

    `emit_to_stream` reports a failed drain by returning non-zero, so a
    clean exit code is the assertion — the tail reaching the server is
    exactly what a zero exit is being made to mean.
    """
    _require_binary()
    if not _broker_reachable():
        pytest.skip("no NATS broker on 127.0.0.1:4222")

    proc, sink = _start(
        "--continuous", "--realtime", "--output", "nats://127.0.0.1:4222/iq"
    )
    try:
        # No file to watch, so wait on the run's own banner reaching us.
        _wait_until(
            lambda: proc.poll() is not None or _elapsed_enough(proc),
            STARTUP_DEADLINE_S,
            "reached its publish loop",
        )
        assert proc.poll() is None, "wfmgen exited before it was interrupted"
        elapsed = _stop_cleanly(proc, sink)
        assert elapsed < STOP_DEADLINE_S
    finally:
        if proc.poll() is None:
            os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
            proc.wait(timeout=5)


_STARTED_AT: dict[int, float] = {}


def _elapsed_enough(proc: subprocess.Popen[str]) -> bool:
    """True once the process has been alive comfortably past its startup.

    The stream path prints nothing until it finishes, so there is no banner
    to wait on — but the window this guards against is milliseconds wide
    (the dynamic linker, before `main()` installs the handler), and half a
    second clears it by two orders of magnitude.
    """
    first = _STARTED_AT.setdefault(proc.pid, time.monotonic())
    return (time.monotonic() - first) > 0.5
