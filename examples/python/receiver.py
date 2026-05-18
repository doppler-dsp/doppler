"""receiver.py — ZMQ SUB receiver example.

Subscribes to a doppler transmitter and displays a live dashboard showing
signal power, packet statistics, and first few samples.  Mirrors the C
receiver example using the Python stream API.

Usage:
  python examples/python/receiver.py [endpoint]
  python examples/python/receiver.py                          # localhost:5555
  python examples/python/receiver.py tcp://192.168.1.10:5555

Press Ctrl+C to stop.
"""

import argparse
import signal
import sys

import numpy as np

from doppler.stream import CF64, CI32, Subscriber

keep_running = True


def _sighandler(sig, frame):
    global keep_running
    keep_running = False


def _power_db(samples: np.ndarray) -> float:
    pwr = float(np.mean(np.abs(samples) ** 2))
    return 10.0 * np.log10(pwr + 1e-12)


def _format_ts(ts_ns: int) -> str:
    secs = ts_ns // 1_000_000_000
    ms   = (ts_ns % 1_000_000_000) // 1_000_000
    hh, rem = divmod(secs, 3600)
    mm, ss  = divmod(rem, 60)
    return f"{hh % 24:02d}:{mm:02d}:{ss:02d}.{ms:03d}"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("endpoint", nargs="?", default="tcp://localhost:5555")
    args = parser.parse_args()

    signal.signal(signal.SIGINT,  _sighandler)
    signal.signal(signal.SIGTERM, _sighandler)

    print(f"doppler Receiver (Python)\n  Endpoint: {args.endpoint}\n"
          f"\nWaiting for packets...")
    sys.stdout.flush()

    total_samples  = 0
    packet_count   = 0
    last_seq       = None
    dropped        = 0

    with Subscriber(args.endpoint) as sub:
        while keep_running:
            try:
                samples, hdr = sub.recv(timeout_ms=500)
            except TimeoutError:
                continue

            n = len(samples)
            packet_count  += 1
            total_samples += n

            seq = hdr.get("sequence", 0)
            if last_seq is not None and seq != last_seq + 1:
                dropped += seq - last_seq - 1
            last_seq = seq

            pwr_db = _power_db(samples)
            ts     = _format_ts(hdr.get("timestamp_ns", 0))
            rate   = hdr.get("sample_rate", 0)
            freq   = hdr.get("center_freq",  0)
            stype  = hdr.get("sample_type",  CF64)
            mb     = total_samples * (8 if stype == CI32 else 16) / 1_048_576
            type_s = "CI32" if stype == CI32 else "CF64"

            show = min(5, n)
            sample_lines = "\n".join(
                f"    [{i}] {samples[i]:.6f}" for i in range(show)
            )

            print(f"\033[2J\033[H"
                  f"doppler Receiver (Python)\n"
                  f"=========================\n"
                  f"  Endpoint:     {args.endpoint}\n"
                  f"  Sample Type:  {type_s}\n"
                  f"  Sample Rate:  {rate / 1e6:.2f} MHz\n"
                  f"  Center Freq:  {freq / 1e9:.2f} GHz\n\n"
                  f"  Sequence:     {seq}\n"
                  f"  Timestamp:    {ts}\n"
                  f"  Num Samples:  {n}\n"
                  f"  Power:        {pwr_db:.2f} dB\n\n"
                  f"  Packets:      {packet_count}\n"
                  f"  Total:        {total_samples} samples ({mb:.2f} MB)\n"
                  f"  Dropped:      {dropped}\n\n"
                  f"  First {show} samples:\n{sample_lines}\n\n"
                  f"Press Ctrl+C to stop.")
            sys.stdout.flush()


if __name__ == "__main__":
    main()
