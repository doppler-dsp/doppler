"""Stopping a stream cleanly: interrupt, end-of-stream, drain.

A streaming producer has to answer three questions when it stops, and
this shows how:

1. **How is a continuous run stopped?** ``wfmgen --continuous`` has no
   natural end. It installs a signal handler before opening anything, so
   SIGINT leaves the generate loop rather than killing the process
   mid-write.
2. **How does the consumer know the stream ended?** A receive timeout
   means only "nothing has arrived yet" — it cannot distinguish an idle
   sender from a finished one. The sender publishes an explicit
   end-of-stream frame, and ``recv()`` raises ``EOFError``.
3. **Did everything arrive?** A send returns once the client holds the
   block, not once the server does. Draining waits for that, with a
   budget, and reports the result — so exit code 0 means the samples
   landed.

Shown on **all three faces of wfmgen**, because the contract is the
library's and not any one binding's:

- **CLI** — the ``wfmgen`` binary as a subprocess, stopped with a real
  SIGINT. This is what a person types.
- **Python API** — ``Composer`` into a ``StreamSink``, with the loop
  checking the ``Interrupt`` guard and ending with
  ``send_eos()`` then ``drain()``. This is what a script does.
- **C API** — ``doppler_wfmgen(argc, argv)``, the same CLI as a callable,
  demonstrated by ``native/examples/graceful_shutdown_demo.c``.

Run it::

    python graceful_shutdown_demo.py

Needs a NATS broker on 127.0.0.1:4222 (``make nats-up``). See
``docs/design/io-termination.md`` for why all three questions are the same
question wearing different clothes.
"""

from __future__ import annotations

import os
import signal
import socket
import subprocess
import sys
import threading
import time
from pathlib import Path

from doppler.stream import Subscriber

ENDPOINT = "nats://127.0.0.1:4222/graceful-demo"

#: How long to produce before interrupting, at full rate — no ``--realtime``,
#: so the generator runs as fast as it can. Interrupting a saturated
#: producer is the demanding case: the receive side is inside the
#: transport's wait almost continuously, rather than checking the interrupt
#: flag between idle polls.
RUN_S = 5.0

#: Cap on the post-interrupt drain, so a large backlog cannot turn the
#: demo into an unbounded read. Reaching it is itself informative.
DRAIN_DEADLINE_S = 15.0

#: The interrupted process must exit well inside this. It is generous
#: against the drain budget it may legitimately spend, and far below the
#: hang it guards against -- an unhandled signal never exits at all.
STOP_DEADLINE_S = 20.0


def _broker_up(host: str = "127.0.0.1", port: int = 4222) -> bool:
    try:
        with socket.create_connection((host, port), timeout=0.5):
            return True
    except OSError:
        return False


def _find_wfmgen() -> Path | None:
    """Locate the built binary, without hard-coding a build layout."""
    here = Path(__file__).resolve()
    for parent in here.parents:
        cand = parent / "build" / "native" / "src" / "wfmcompose" / "wfmgen"
        if cand.exists():
            return cand
    return None


def main() -> int:
    if not _broker_up():
        print(
            "no NATS broker on 127.0.0.1:4222 — start one with `make nats-up`"
        )
        return 0

    wfmgen = _find_wfmgen()
    if wfmgen is None:
        print("wfmgen not built — run `make build`")
        return 0

    print("--- face 1: the CLI, as a subprocess ---")
    print(f"subscribing to {ENDPOINT}")
    sub = Subscriber(ENDPOINT)

    print(f"starting: {wfmgen.name} --continuous (full rate, unpaced)")
    proc = subprocess.Popen(
        [str(wfmgen), "--continuous", "--output", ENDPOINT],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.STDOUT,
        stdin=subprocess.DEVNULL,
        start_new_session=True,
    )

    # Consume while the producer runs, not after it stops.
    #
    # A subscriber that does not read lets the client queue the whole run
    # in memory — an unpaced producer can bank many gigabytes in seconds.
    # Reading concurrently bounds that, and it is the only way to get a
    # throughput number that means anything, since producer and consumer
    # are then measured over the same interval.
    frames = 0
    samples = 0
    ended = False
    stop_reading = threading.Event()

    def consume() -> None:
        nonlocal frames, samples, ended
        while not stop_reading.is_set():
            try:
                block, _hdr = sub.recv(timeout_ms=1000)
            except EOFError:
                ended = True
                return
            except Exception:
                continue  # a timeout means "nothing yet", so keep reading
            frames += 1
            samples += len(block)

    reader = threading.Thread(target=consume, daemon=True)

    try:
        print(
            f"producing flat out for {RUN_S:.0f} s, consuming as it "
            f"arrives ..."
        )
        t0 = time.monotonic()
        reader.start()
        time.sleep(RUN_S)

        # 1. Interrupt it mid-stream, while it is saturated.
        print("interrupting the producer (SIGINT) mid-stream")
        started = time.monotonic()
        os.killpg(os.getpgid(proc.pid), signal.SIGINT)
        proc.wait(timeout=STOP_DEADLINE_S)
        stop_ms = (time.monotonic() - started) * 1e3

        # 3. Exit 0 is the drain's verdict: the tail reached the server.
        assert proc.returncode == 0, (
            f"producer exited {proc.returncode}; a negative code means the "
            f"signal KILLED it rather than being handled"
        )
        print(f"producer stopped cleanly in {stop_ms:.0f} ms, exit 0")

        # 2. The reader is still going; it stops when it hears the ending.
        reader.join(timeout=DRAIN_DEADLINE_S)
        elapsed = time.monotonic() - t0
        stop_reading.set()

        assert ended, (
            "the consumer never saw an end-of-stream frame, so it had only "
            "silence to interpret"
        )
        assert frames > 0, "no frames crossed the wire before the interrupt"

        msa = samples / elapsed / 1e6
        gbs = samples * 8 / elapsed / 1e9
        print(
            f"consumer received {frames} frames / {samples / 1e6:.1f} Msa "
            f"over {elapsed:.2f} s"
        )
        print(f"  arriving at {msa:.1f} MSa/s ({gbs:.2f} GB/s of cf32)")
        print(
            "\nPUB/SUB is at-most-once: a subscriber that cannot keep up "
            "drops\nframes rather than slowing the sender, so this counts "
            "what ARRIVED,\nnot what was sent."
        )
    finally:
        if proc.poll() is None:
            os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
            proc.wait(timeout=5)
        sub.close()

    print("\n--- face 2: the Python API, in-process ---")
    _python_api_face()

    print("\nall three questions answered, on every face that asks them")
    return 0


def _python_api_face() -> None:
    """The same shutdown, composed and sent from Python.

    No subprocess and no signal. A producer loop asks the guard between
    blocks, and that is the same flag a signal handler sets — so one loop
    serves both Ctrl+C and a programmatic stop without needing to know
    which it got. This stop is programmatic, which keeps the example
    deterministic.

    The guard arms nothing here (an empty signal list): it is a handle to
    the process-wide flag, which is all a programmatic stop needs.
    """
    import numpy as np

    from doppler.interrupt import Interrupt
    from doppler.wfm import Composer, Segment, StreamSink

    endpoint = "nats://127.0.0.1:4222/graceful-demo-py"
    sub = Subscriber(endpoint)
    sink = StreamSink(endpoint, "cf32")

    frames = 0
    try:
        seg = Segment("tone", num_samples=4096)
        block = Composer([seg]).compose().astype(np.complex64)

        # Constructing the guard clears the flag, so the loop starts from
        # a known state without a separate call.
        it = Interrupt(np.array([], dtype=np.int32))
        t0 = time.monotonic()
        while not it.interrupted():
            sink.send(block, 1e6, 1e9)
            frames += 1
            if time.monotonic() - t0 > 0.5:
                # What a signal handler would do. The loop cannot tell the
                # difference between this and a Ctrl+C.
                it.interrupt()

        # The ordered shutdown: stop producing, say so, then let it land.
        # send_eos() must precede drain() — a drain cannot be reversed and
        # refuses sends once it starts flushing.
        sink.send_eos()
        sink.drain(5000)
        it.resume()
        print(f"sent {frames} frames, then send_eos() + drain()")

        got = 0
        ended = False
        while True:
            try:
                _blk, _hdr = sub.recv(timeout_ms=2000)
            except EOFError:
                ended = True
                break
            except Exception:
                break
            got += 1

        assert ended, "the Python face must announce its ending too"
        assert got > 0, "no frames arrived on the Python face"
        print(f"consumer received {got} frames, then EOFError")
    finally:
        sink.close()
        sub.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
