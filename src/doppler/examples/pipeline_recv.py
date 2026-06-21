"""pipeline_recv.py — ZMQ PULL worker example.

Connects to a PUSH sender and processes packets as they arrive.  Multiple
instances can run simultaneously; each receives a share of the frames
round-robin from the sender.

Usage:
  python examples/python/pipeline_recv.py [endpoint] [worker-id]
  python examples/python/pipeline_recv.py                        # worker 0
  python examples/python/pipeline_recv.py tcp://localhost:5560 1

Press Ctrl+C to stop.
"""

import argparse
import signal
import sys

import numpy as np

from doppler.stream import CF64, CI32, Pull

keep_running = True


def _sighandler(sig, frame) -> None:
    global keep_running
    keep_running = False


def _power_db(samples: np.ndarray) -> float:
    pwr = float(np.mean(np.abs(samples) ** 2))
    return 10.0 * np.log10(pwr + 1e-12)


def _format_ts(ts_ns: int) -> str:
    secs = ts_ns // 1_000_000_000
    ms = (ts_ns % 1_000_000_000) // 1_000_000
    hh, rem = divmod(secs, 3600)
    mm, ss = divmod(rem, 60)
    return f"{hh % 24:02d}:{mm:02d}:{ss:02d}.{ms:03d}"


def main() -> None:
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("endpoint", nargs="?", default="tcp://localhost:5560")
    parser.add_argument("worker_id", nargs="?", type=int, default=0)
    args = parser.parse_args()

    signal.signal(signal.SIGINT, _sighandler)
    signal.signal(signal.SIGTERM, _sighandler)

    print(
        f"doppler Pipeline Worker {args.worker_id} (Python)\n"
        f"  Endpoint: {args.endpoint}\n\nWaiting for packets..."
    )
    sys.stdout.flush()

    total_samples = 0
    packet_count = 0

    with Pull(args.endpoint) as pull:
        while keep_running:
            try:
                samples, hdr = pull.recv(timeout_ms=500)
            except TimeoutError:
                continue

            n = len(samples)
            packet_count += 1
            total_samples += n

            stype = hdr.get("sample_type", CF64)
            bytes_per = 8 if stype == CI32 else 16
            mb = total_samples * bytes_per / 1_048_576
            pwr_db = _power_db(samples)
            ts = _format_ts(hdr.get("timestamp_ns", 0))
            rate = hdr.get("sample_rate", 0)
            freq = hdr.get("center_freq", 0)

            print(
                f"\033[2J\033[H"
                f"doppler Pipeline Worker {args.worker_id} (Python)\n"
                f"{'=' * 40}\n"
                f"  Endpoint:    {args.endpoint}\n"
                f"  Sample Rate: {rate / 1e6:.2f} MHz\n"
                f"  Center Freq: {freq / 1e9:.2f} GHz\n\n"
                f"  Timestamp:   {ts}\n"
                f"  Num Samples: {n}\n"
                f"  Power:       {pwr_db:.2f} dB\n\n"
                f"  Packets:     {packet_count}\n"
                f"  Total:       {total_samples} samples ({mb:.2f} MB)\n\n"
                f"Press Ctrl+C to stop."
            )
            sys.stdout.flush()


if __name__ == "__main__":
    main()
