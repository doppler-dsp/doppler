"""The two-process C examples, run as pairs — including Ctrl+C.

`examples/c/.examples-skip` excuses `receiver`/`transmitter` from the
smoke gate with a reason that is true as far as it goes — "runs until
Ctrl+C, no exit condition, so no broker makes them terminate" — and that
reason quietly ASSUMES the thing this file tests. Ctrl+C was the stated
way to stop them, nothing ran them, and a receiver that ignores Ctrl+C
shipped. They stay excused from the smoke gate, which runs one binary to
completion; they are exercised here instead, as the pair they are.

This runs the pair the way a reader does, and asserts the property that
was broken: a receiver must stop when you interrupt it, *including* when
the sender has already stopped and no traffic is arriving. That case is
the whole bug. With traffic flowing every packet returns control to the
example's loop and the signal flag is seen immediately, so a test that
only interrupts a busy receiver passes against the defect.

Requires the binaries (`make build`) and a NATS broker; skips cleanly
without either, the same way the C round-trip tests do.
"""

from __future__ import annotations

import os
import signal
import socket
import subprocess
import threading
import time

import pytest

from doppler.tests._repo import repo_root

pytestmark = pytest.mark.examples

REPO = repo_root(__file__)
BIN = REPO / "build" / "examples" / "c"

# How long the interrupted process may take to exit. The example asks the
# broker for a 250 ms receive timeout, so one timeout plus teardown is the
# real budget; this is generous next to it and still far below the hang it
# guards against (an unbounded recv parks for an hour).
STOP_DEADLINE_S = 5.0


def _broker_reachable(host: str = "127.0.0.1", port: int = 4222) -> bool:
    try:
        with socket.create_connection((host, port), timeout=0.5):
            return True
    except OSError:
        return False


def _start(name: str) -> tuple[subprocess.Popen, list[str]]:
    """Run one example in its own process group, draining its output.

    Two things, and the second is not optional. The process group is so a
    signal reaches it exactly as a terminal's Ctrl+C would. The drain
    thread is because a dashboard prints a screen per frame: a pipe nobody
    reads fills in well under a second, and the example then blocks in
    write() rather than in recv() -- at which point this test proves
    nothing about the interrupt and hangs on a full buffer instead. CI
    found that, having passed locally, which is the usual way round for a
    race decided by how fast the reader is.
    """
    proc = subprocess.Popen(
        [str(BIN / name)],
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


def _interrupt(proc: subprocess.Popen) -> None:
    os.killpg(os.getpgid(proc.pid), signal.SIGINT)


def _require(name: str) -> None:
    if not (BIN / name).exists():
        pytest.skip(f"{name} not built (run `make build`)")


def test_receiver_stops_on_sigint_after_the_sender_has_stopped() -> None:
    """The reported defect: Ctrl+C is ignored once traffic stops.

    `dp_sub_recv` with the default timeout blocks inside the NATS client
    until a message arrives, so an example's `keep_running` flag is never
    re-read and the handler that sets it may as well not exist. The fix
    is a bounded `dp_sub_set_timeout`; this is the test that can tell.
    """
    _require("receiver")
    _require("transmitter")
    if not _broker_reachable():
        pytest.skip("two-process C example: no NATS broker on :4222")

    rcv, rx_out = _start("receiver")
    time.sleep(1.0)
    tx, _tx_out = _start("transmitter")

    # Let the wire actually move before anything is interrupted, so a pair
    # that exchanges nothing cannot pass this by exiting promptly.
    deadline = time.monotonic() + 20.0
    try:
        while time.monotonic() < deadline:
            if any("Packets:" in ln for ln in rx_out):
                break
            time.sleep(0.05)
        else:
            pytest.fail(f"receiver saw no packets\n{''.join(rx_out)[-1500:]}")

        # The sender stops FIRST. From here the receiver's recv has nothing
        # to return, which is where an unbounded wait becomes a hang.
        _interrupt(tx)
        tx.wait(timeout=10)
        time.sleep(1.0)

        started = time.monotonic()
        _interrupt(rcv)
        try:
            rcv.wait(timeout=STOP_DEADLINE_S)
        except subprocess.TimeoutExpired:
            pytest.fail(
                "receiver ignored SIGINT with no traffic arriving: it is "
                "parked in a blocking dp_sub_recv, so the handler's flag "
                "is never re-read. Give the example a bounded "
                "dp_sub_set_timeout and treat DP_ERR_TIMEOUT as a re-check."
            )
        elapsed = time.monotonic() - started
        assert rcv.returncode == 0, f"receiver exited {rcv.returncode}"
        assert elapsed < STOP_DEADLINE_S
        assert tx.returncode == 0, f"transmitter exited {tx.returncode}"
    finally:
        for p in (rcv, tx):
            if p.poll() is None:
                os.killpg(os.getpgid(p.pid), signal.SIGKILL)
                p.wait(timeout=5)
