"""wfm_realtime_stream.py — paced NATS publish of a wfmgen scene.

A self-contained real-time streaming demo over ``nats://`` (requires a
``nats-server`` running locally): a :class:`~doppler.wfm.Composer` is
streamed block-by-block, each block is **paced to wall-clock** with a
:class:`~doppler.wfm.SampleClock` (so the feed runs at ``fs``
samples/second, like a live SDR), published through a
:class:`~doppler.wfm.StreamSink`, and received back by a
:class:`doppler.stream.Subscriber` that prints live power/throughput. At the
end it reports the clock's pacing accuracy (samples emitted, underruns, worst
lateness).

The sink publishes **cf64** on purpose: ``doppler.stream``'s receiver currently
decodes only ``CI32``/``CF64``/``CF128``, so the ``StreamSink`` default ``cf32``
(and ``ci16``/``ci8``) are not yet decodable on the Python side — tracked in
``docs/dev/wfm-validation-findings.md#streamsink-stream-dtype-gap`` and filed
as a ``stream`` bug. Use cf64/ci32 for a Python subscriber until that lands.

Unlike the smoke-tested compute demos, this one binds a socket and paces in
real time, so it is a *manual* demo (run it directly); it still exits 0 cleanly
after a bounded run.

Run:
    nats-server -js &
    python examples/python/wfm_realtime_stream.py
"""

from __future__ import annotations

import random

import numpy as np

from doppler.stream import Subscriber
from doppler.wfm import Composer, SampleClock, Segment, StreamSink, qpsk, tone

FS = 500_000.0  # 500 kHz feed
FC = 2.4e9
BLOCK = 4096
TOTAL = 80_000  # ~0.16 s of signal at FS -> a short, bounded demo


def main() -> None:
    endpoint = f"nats://127.0.0.1:4222/wfm-feed-{random.randint(1, 10**9)}"
    # A two-source scene: a QPSK signal of interest plus a CW interferer.
    scene = Segment.sum(
        qpsk(fs=FS, sps=8, snr=20.0, snr_mode="esno"),
        tone(fs=FS, freq=1.2e5, level=-12.0),
        num_samples=TOTAL,
    )
    composer = Composer([scene])

    sub = Subscriber(endpoint)

    import time

    time.sleep(0.3)  # core NATS: sub must exist before the first publish

    # cf64 so the Python Subscriber can decode the frames (see docstring).
    sink = StreamSink(endpoint, sample_type="cf64")
    clock = SampleClock(fs=FS)

    time.sleep(0.1)

    sent_blocks = recv_blocks = recv_samples = 0
    print(f"streaming {TOTAL} samples @ {FS / 1e3:.0f} kHz over {endpoint}")
    print(f"{'block':>5} {'tx_n':>7} {'rx_pwr_dB':>10} {'elapsed_s':>10}")

    t0 = time.perf_counter()
    for i, block in enumerate(composer.stream(block=BLOCK)):
        sink.send(block, FS, FC)
        sent_blocks += 1
        clock.pace(len(block))  # block until wall-clock catches up to fs

        try:
            samples, _hdr = sub.recv(timeout_ms=50)
        except Exception:
            samples = None
        if samples is not None and len(samples):
            recv_blocks += 1
            recv_samples += len(samples)
            if i % 4 == 0:
                pwr = 10.0 * np.log10(
                    np.mean(np.abs(np.asarray(samples)) ** 2) + 1e-30
                )
                print(
                    f"{i:5d} {len(block):7d} {pwr:10.2f} "
                    f"{time.perf_counter() - t0:10.3f}"
                )

    sink.close()
    sub.close()

    print(
        f"\nsent {sent_blocks} blocks, received {recv_blocks} "
        f"({recv_samples} samples)"
    )
    print(
        f"SampleClock: paced {clock.samples} samples, "
        f"{clock.underruns} underruns, "
        f"max lateness {clock.max_lateness * 1e3:.2f} ms"
    )
    clock.close()


if __name__ == "__main__":
    main()
