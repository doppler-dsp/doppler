"""
doppler-source — streaming IQ source entry point.

Generates synthetic IQ samples and pushes them over a NATS PUSH endpoint
so they can be consumed by downstream blocks in a doppler pipeline.

Usage
-----
    doppler-source --type tone --bind nats://127.0.0.1:4222/iq [options]
"""

from __future__ import annotations

import argparse
import signal
from datetime import datetime, timezone
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    import types

BLOCK_SIZE = 4096  # samples per push frame


def _log(msg: str) -> None:
    ts = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    print(f"[{ts}] {msg}", flush=True)


def main() -> None:
    parser = argparse.ArgumentParser(
        prog="doppler-source",
        description="doppler streaming IQ source",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument(
        "--type",
        required=True,
        choices=["tone"],
        help="Source type",
    )
    parser.add_argument(
        "--bind",
        required=True,
        metavar="ADDR",
        help="NATS PUSH endpoint, e.g. nats://127.0.0.1:4222/iq",
    )
    parser.add_argument(
        "--fs",
        type=float,
        default=2.048e6,
        metavar="Hz",
        help="Sample rate",
    )
    parser.add_argument(
        "--center",
        type=float,
        default=0.0,
        metavar="Hz",
        help="Center frequency (metadata only)",
    )
    parser.add_argument(
        "--tone-freq",
        type=float,
        default=100e3,
        metavar="Hz",
        help="Tone offset from DC",
    )
    parser.add_argument(
        "--tone-power",
        type=float,
        default=-20.0,
        metavar="DBM",
        help="Tone power in dBm",
    )
    parser.add_argument(
        "--noise-floor",
        type=float,
        default=-90.0,
        metavar="DBM",
        help="AWGN noise floor in dBm",
    )

    args = parser.parse_args()

    from doppler.specan.source import DemoSource
    from doppler.stream import CF64, Push

    _log(
        f"doppler-source started — type=tone bind={args.bind}"
        f" fs={args.fs:.0f} tone_freq={args.tone_freq:.0f}Hz"
        f" tone_power={args.tone_power}dBm noise_floor={args.noise_floor}dBm"
    )

    source = DemoSource(
        sample_rate=args.fs,
        center_freq=args.center,
        tone_freq=args.tone_freq,
        tone_power=args.tone_power,
        noise_floor=args.noise_floor,
    )

    # Graceful shutdown on SIGTERM
    _running = True

    def _stop(signum: int, frame: types.FrameType | None) -> None:
        nonlocal _running
        _running = False

    signal.signal(signal.SIGTERM, _stop)

    try:
        with Push(args.bind, CF64) as push:
            while _running:
                iq, fs, cf = source.read(BLOCK_SIZE)
                push.send(
                    iq.astype("complex128"),
                    sample_rate=fs,
                    center_freq=cf,
                )
    except KeyboardInterrupt:
        pass
    finally:
        source.close()
        _log("doppler-source stopped")


if __name__ == "__main__":
    main()
