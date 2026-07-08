"""pipeline_send.py — NATS PUSH sender example.

Generates a complex tone and distributes 8 192-sample packets to one or
more PULL workers via the NATS JetStream work-queue tier.  Frames are
load-balanced across all connected workers sharing the durable consumer.
Requires a running nats-server (e.g. `nats-server -js`).

Usage:
  python examples/python/pipeline_send.py [endpoint]
  python examples/python/pipeline_send.py                  # nats://127.0.0.1:4222/work
  python examples/python/pipeline_send.py nats://127.0.0.1:4222/work2

Run one or more pipeline_recv.py instances connecting to the same endpoint
before starting this sender.  Press Ctrl+C to stop.
"""

import argparse
import signal
import sys
import time

import numpy as np

from doppler.stream import CF64, Push, get_timestamp_ns

SAMPLE_RATE = 1_000_000  # 1 MHz
CENTER_FREQ = 2_400_000_000  # 2.4 GHz
BUFFER_SIZE = 8_192
SIGNAL_FREQ = 10_000  # 10 kHz tone

keep_running = True


def _sighandler(sig, frame) -> None:
    global keep_running
    keep_running = False


def _make_tone(
    n: int, freq: float, sample_rate: float, phase: float
) -> np.ndarray:
    t = np.arange(n) / sample_rate
    return np.exp(1j * (2 * np.pi * freq * t + phase)).astype(np.complex128)


def main() -> None:
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "endpoint", nargs="?", default="nats://127.0.0.1:4222/work"
    )
    args = parser.parse_args()

    signal.signal(signal.SIGINT, _sighandler)
    signal.signal(signal.SIGTERM, _sighandler)

    print("doppler Pipeline Sender (Python)")
    print(f"  Endpoint    : {args.endpoint}")
    print(f"  Sample rate : {SAMPLE_RATE / 1e6:.2f} MHz")
    print(f"  Centre freq : {CENTER_FREQ / 1e9:.4f} GHz")
    print(f"  Signal freq : {SIGNAL_FREQ / 1e3:.1f} kHz")
    print(f"  Packet size : {BUFFER_SIZE} samples")
    print("\nSending — connect workers with pipeline_recv.py")
    sys.stdout.flush()

    with Push(args.endpoint, CF64) as push:
        total_samples = 0
        packet_count = 0
        phase = 0.0

        while keep_running:
            samples = _make_tone(BUFFER_SIZE, SIGNAL_FREQ, SAMPLE_RATE, phase)
            push.send(
                samples, sample_rate=SAMPLE_RATE, center_freq=CENTER_FREQ
            )

            phase = (
                phase + 2 * np.pi * SIGNAL_FREQ * BUFFER_SIZE / SAMPLE_RATE
            ) % (2 * np.pi)
            total_samples += BUFFER_SIZE
            packet_count += 1

            if total_samples % SAMPLE_RATE == 0:
                ts = get_timestamp_ns()
                hh, rem = divmod(ts // 1_000_000_000, 3600)
                mm, ss = divmod(rem, 60)
                mb = total_samples * 16 / 1_048_576
                print(
                    f"\033[2J\033[H"
                    f"doppler Pipeline Sender (Python)\n"
                    f"=================================\n"
                    f"  Timestamp : {hh % 24:02d}:{mm:02d}:{ss:02d}\n"
                    f"  Packets   : {packet_count}\n"
                    f"  Total     : {total_samples} samples ({mb:.2f} MB)\n\n"
                    f"Press Ctrl+C to stop."
                )
                sys.stdout.flush()

            time.sleep(0.008)


if __name__ == "__main__":
    main()
