"""requester.py — ZMQ REQ client example.

Sends a block of samples to a Replier and receives the processed reply.
Models a remote DSP service: the client uploads a signal, the server
processes it and returns the result.

The REQ/REP pattern is strictly alternating: send → recv → send → recv.
Both sides must follow this order or ZMQ raises an FSM error.

Usage:
  python examples/python/requester.py [endpoint]
  python examples/python/requester.py                       # localhost:5562
  python examples/python/requester.py tcp://192.168.1.5:5562

Run replier.py first.  Press Ctrl+C to stop.
"""

import argparse
import signal
import sys
import time

import numpy as np

from doppler.stream import CF64, Requester

SAMPLE_RATE = 1_000_000  # 1 MHz
CENTER_FREQ = 2_400_000_000  # 2.4 GHz
BUFFER_SIZE = 1_024
SIGNAL_FREQ = 10_000  # 10 kHz tone
N_REQUESTS = 10  # 0 → run forever

keep_running = True


def _sighandler(sig, frame):
    global keep_running
    keep_running = False


def _make_tone(
    n: int, freq: float, sample_rate: float, phase: float
) -> np.ndarray:
    t = np.arange(n) / sample_rate
    return np.exp(1j * (2 * np.pi * freq * t + phase)).astype(np.complex128)


def _power_db(samples: np.ndarray) -> float:
    pwr = float(np.mean(np.abs(samples) ** 2))
    return 10.0 * np.log10(pwr + 1e-12)


def main() -> None:
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("endpoint", nargs="?", default="tcp://localhost:5562")
    parser.add_argument(
        "--count",
        type=int,
        default=N_REQUESTS,
        help="Number of request/reply cycles (0 = unlimited).",
    )
    args = parser.parse_args()

    signal.signal(signal.SIGINT, _sighandler)
    signal.signal(signal.SIGTERM, _sighandler)

    print("doppler Requester (Python)")
    print(f"  Endpoint    : {args.endpoint}")
    print(f"  Packet size : {BUFFER_SIZE} samples")
    count = "unlimited" if args.count == 0 else str(args.count)
    print(f"  Requests    : {count}")
    sys.stdout.flush()

    with Requester(args.endpoint, CF64) as req:
        phase = 0.0
        i = 0
        while keep_running and (args.count == 0 or i < args.count):
            x = _make_tone(BUFFER_SIZE, SIGNAL_FREQ, SAMPLE_RATE, phase)
            phase = (
                phase + 2 * np.pi * SIGNAL_FREQ * BUFFER_SIZE / SAMPLE_RATE
            ) % (2 * np.pi)

            t0 = time.monotonic_ns()
            req.send(x, sample_rate=SAMPLE_RATE, center_freq=CENTER_FREQ)
            reply, hdr = req.recv(timeout_ms=5000)
            rtt_us = (time.monotonic_ns() - t0) / 1_000

            in_pwr = _power_db(x)
            out_pwr = _power_db(reply)
            print(
                f"  [{i:4d}]  RTT {rtt_us:7.1f} µs  "
                f"in {in_pwr:+.1f} dB  out {out_pwr:+.1f} dB  "
                f"seq {hdr.get('sequence', '?')}"
            )
            sys.stdout.flush()
            i += 1

    print("\nDone.")


if __name__ == "__main__":
    main()
