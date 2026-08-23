"""Stopping a stream cleanly: interrupt, end-of-stream, drain.

Three questions no producer/consumer pair could answer before, shown on
the real tool rather than a toy:

1. **Can a continuous run be stopped at all?** ``wfmgen --continuous`` has
   no natural end. It now installs a signal handler before it opens
   anything, so SIGINT leaves the generate loop instead of killing the
   process mid-write.
2. **Does the consumer learn the stream ended?** A timeout means only
   "nothing yet", which is exactly what a consumer cannot act on. wfmgen
   publishes an explicit end-of-stream frame, and ``recv()`` raises
   ``EOFError``.
3. **Did the tail arrive?** A send returns once the *client* has the
   block, not the server. wfmgen drains with a budget and reports the
   result, so exit code 0 means the samples actually landed.

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

#: How long to let wfmgen produce before interrupting it, AT FULL RATE --
#: no --realtime, so the generator runs as fast as it can and the receive
#: side spends most of its time inside the transport's wait. That is the
#: condition the interrupt bound has never been measured on: idle, a wait
#: checks the flag ten times a second; saturated, it is inside the client
#: nearly always. Interrupting a flat-out producer is the interesting case.
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

    # Consume CONCURRENTLY, from the moment the producer starts.
    #
    # Not an aesthetic choice. Reading only after the interrupt makes the
    # client buffer the whole run in memory: measured, a 5 s unpaced run
    # queued ~16 GB, and the drain then "received" it at 18 GB/s because
    # doppler's recv is zero-copy and was handing out pointers to RAM, not
    # reading a socket. That is a rate for a transfer that had already
    # happened, and an example that would OOM a CI runner.
    #
    # Reading as it arrives bounds memory AND makes the numbers mean
    # something: producer and consumer measured over the same interval.
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

        # 1. Interrupt it while it is SATURATED, not idle. This is the
        #    condition the interrupt bound had never been measured on.
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
            "the consumer never saw an end-of-stream frame — it would have "
            "had only silence to interpret, which is the whole defect"
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
            "drops\nframes rather than slowing the sender, so this is what "
            "ARRIVED, not\nwhat was sent. A number for what was SENT needs "
            "the producer\ninstrumented too -- which is what the "
            "characterization is for."
        )
    finally:
        if proc.poll() is None:
            os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
            proc.wait(timeout=5)
        sub.close()

    print("\nall three questions answered: stopped, ended, delivered")
    return 0


if __name__ == "__main__":
    sys.exit(main())
