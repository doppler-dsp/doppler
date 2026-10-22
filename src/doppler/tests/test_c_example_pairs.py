"""The two-process C examples, run as pairs — including Ctrl+C.

`native/examples/.examples-skip` excuses `receiver`/`transmitter` from the
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

The second kind of pair is a Python example and its C twin that run the
same captures with the same seeds: :data:`TWINS`. Each twin asserts its own
bounds, so the two could drift apart with every gate green; this runs both
from one build and requires the same printed lines. One build matters: a
different compiler or sanitizer build moves a floating-point result by an
error count, which is not a defect. The binaries come from the job's own
build tree, :func:`~doppler.tests._repo.build_dir` -- the coverage job builds
only ``build-cov`` and says so through ``DOPPLER_BUILD_DIR``. Without the
binary it skips, except under CI, where that tree is built and a missing
binary is a failure.
"""

from __future__ import annotations

import os
import re
import signal
import socket
import subprocess
import sys
import threading
import time

import pytest

from doppler.tests._repo import build_dir, repo_root

pytestmark = pytest.mark.examples

REPO = repo_root(__file__)
BIN = build_dir() / "native" / "examples"

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
    tx, tx_out = _start("transmitter")

    # Wait for the SENDER to say it is up before anything is interrupted.
    # Its banner is printed after `signal(SIGINT, ...)` in main(), so the
    # line is proof the handler is installed -- and that is the whole
    # point: a signal delivered before then is not ignored, it KILLS the
    # process (exit -2) while it is still in the dynamic linker, which no
    # amount of handler code can prevent. Waiting on the receiver alone
    # could not tell: `Packets:` matched the dashboard's own `Packets: 0`
    # and any traffic another test left on the subject, so the wait
    # returned instantly and this test interrupted a process that had not
    # started executing. It failed on 2 of 7 Python versions in CI and on
    # nothing locally, which is what a millisecond-wide race looks like.
    deadline = time.monotonic() + 20.0
    try:
        while time.monotonic() < deadline:
            if any("Waiting" in ln for ln in tx_out):
                break
            time.sleep(0.05)
        else:
            pytest.fail(
                f"transmitter never started\n{''.join(tx_out)[-1500:]}"
            )

        # THEN let the wire actually move, so a pair that exchanges nothing
        # cannot pass this by exiting promptly. A non-zero count is the
        # evidence -- `Packets:` alone matches the idle dashboard.
        while time.monotonic() < deadline:
            if any(re.search(r"Packets: +[1-9]", ln) for ln in rx_out):
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


#: Python examples whose C twin prints the same result lines. The Python
#: twin's "wrote <figure>" line and the C twin's closing "OK" are each one
#: face's own, and are not compared.
TWINS = ("async_dsss_receiver_lifecycle", "async_dsss_receiver_spec_demo")


def _result_lines(text: str) -> list[str]:
    return [
        ln
        for ln in text.splitlines()
        if ln.strip() and not ln.startswith("wrote ") and ln != "OK"
    ]


# `examples_wallclock` (pyproject.toml): each case runs a whole example under
# a 300 s wall-clock limit, and under xdist it competes with every other
# worker for the cores it needs to finish. Measured 2026-10-22:
# async_dsss_receiver_spec_demo.py takes 21 s on an optimized build and 102 s
# on the coverage build (llvm instrumentation, -O0) with a 20-core box to
# itself; on CI's 4-vCPU coverage leg, sharing the machine with `-n auto`,
# it passed on #2545 and then timed out twice on #2548, whose only change
# near it was more tests. Its own assertion is not a timing, so it is not an
# `.examples-serial` entry; the marker says why it runs serially, and both
# the example gate and the coverage leg give it their serial pass.
@pytest.mark.examples_wallclock
@pytest.mark.parametrize("name", TWINS)
def test_twins_print_the_same_lines(name: str, tmp_path) -> None:
    binary = BIN / name
    if not binary.exists():
        if os.environ.get("CI"):
            pytest.fail(f"{name} not built: no {binary} in the job's tree")
        pytest.skip(f"{name} not built (run `make build`)")
    script = REPO / "src" / "doppler" / "examples" / f"{name}.py"
    py = subprocess.run(
        [sys.executable, str(script)],
        cwd=tmp_path,
        env=dict(os.environ, MPLBACKEND="Agg"),
        stdin=subprocess.DEVNULL,
        capture_output=True,
        text=True,
        timeout=300,
    )
    c = subprocess.run(
        [str(binary)],
        cwd=tmp_path,
        stdin=subprocess.DEVNULL,
        capture_output=True,
        text=True,
        timeout=300,
    )
    assert py.returncode == 0, py.stderr[-2000:]
    assert c.returncode == 0, c.stderr[-2000:]
    py_lines, c_lines = _result_lines(py.stdout), _result_lines(c.stdout)
    assert py_lines, f"{name}.py printed no result lines"
    assert py_lines == c_lines, (
        f"{name}: the twins disagree\n--- Python ---\n"
        + "\n".join(py_lines)
        + "\n--- C ---\n"
        + "\n".join(c_lines)
    )
