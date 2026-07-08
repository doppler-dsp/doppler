"""
doppler-fir — streaming FIR filter chain block.

Pulls IQ frames from an upstream doppler pipeline stage, applies a FIR
filter, and pushes the filtered frames to the next stage.

Usage
-----
    doppler-fir --connect nats://127.0.0.1:4222/dp-chain-5600 \\
                --bind nats://127.0.0.1:4222/dp-chain-5601 \\
                [--taps T0 T1 T2 ...]
"""

from __future__ import annotations

import argparse
import signal
from datetime import datetime, timezone
from typing import TYPE_CHECKING

import numpy as np

if TYPE_CHECKING:
    import types

RECV_TIMEOUT_MS = 500  # poll interval so SIGTERM is noticed promptly


def _log(msg: str) -> None:
    ts = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    print(f"[{ts}] {msg}", flush=True)


def main() -> None:
    parser = argparse.ArgumentParser(
        prog="doppler-fir",
        description="doppler streaming FIR filter chain block",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument(
        "--connect",
        required=True,
        metavar="ADDR",
        help="NATS PULL endpoint to read from, e.g. "
        "nats://127.0.0.1:4222/dp-chain-5600",
    )
    parser.add_argument(
        "--bind",
        required=True,
        metavar="ADDR",
        help="NATS PUSH endpoint to write to, e.g. "
        "nats://127.0.0.1:4222/dp-chain-5601",
    )
    parser.add_argument(
        "--taps",
        type=float,
        nargs="+",
        default=[1.0],
        metavar="T",
        help="FIR tap coefficients (real-valued). Defaults to a single "
        "unity tap (pass-through) when omitted.",
    )

    args = parser.parse_args()

    from doppler.filter import FIR
    from doppler.stream import CF64, Pull, Push

    _log(
        f"doppler-fir started — connect={args.connect} bind={args.bind}"
        f" ntaps={len(args.taps)}"
    )

    fir = FIR(np.array(args.taps, dtype=np.complex64))

    _running = True

    def _stop(signum: int, frame: types.FrameType | None) -> None:
        nonlocal _running
        _running = False

    signal.signal(signal.SIGTERM, _stop)

    try:
        with Pull(args.connect) as pull, Push(args.bind, CF64) as push:
            while _running:
                try:
                    samples, hdr = pull.recv(timeout_ms=RECV_TIMEOUT_MS)
                except TimeoutError:
                    continue
                y = fir.execute(samples.astype(np.complex64))
                push.send(
                    y.astype("complex128"),
                    sample_rate=hdr["sample_rate"],
                    center_freq=hdr["center_freq"],
                )
                pull.ack(samples)
    except KeyboardInterrupt:
        pass
    finally:
        _log("doppler-fir stopped")


if __name__ == "__main__":
    main()
