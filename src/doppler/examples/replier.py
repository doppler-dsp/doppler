"""replier.py — NATS REP server example.

Connects a NATS REP endpoint, receives signal frames from a Requester,
applies a simple DSP operation (gain), and sends the result back.  Models
a remote DSP service that a client queries for processed signal blocks.
Requires a running nats-server (e.g. `nats-server -js`).

The REQ/REP pattern is strictly alternating: recv → send → recv → send.
The Replier must always reply before accepting the next request.

Usage:
  python examples/python/replier.py [endpoint] [--gain G]
  python examples/python/replier.py                    # ctrl subject, gain=1
  python examples/python/replier.py nats://127.0.0.1:4222/ctrl2 --gain 0.5

Run this before starting requester.py.  Press Ctrl+C to stop.
"""

import argparse
import signal
import sys

import numpy as np

from doppler.stream import CF64, Replier

keep_running = True


def _sighandler(sig, frame) -> None:
    global keep_running
    keep_running = False


def _power_db(samples: np.ndarray) -> float:
    pwr = float(np.mean(np.abs(samples) ** 2))
    return 10.0 * np.log10(pwr + 1e-12)


def main() -> None:
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "endpoint", nargs="?", default="nats://127.0.0.1:4222/ctrl"
    )
    parser.add_argument(
        "--gain",
        type=float,
        default=1.0,
        help="Linear gain applied to each received frame (default 1.0).",
    )
    args = parser.parse_args()

    signal.signal(signal.SIGINT, _sighandler)
    signal.signal(signal.SIGTERM, _sighandler)

    print("doppler Replier (Python)")
    print(f"  Endpoint : {args.endpoint}")
    print(
        f"  Gain     : {args.gain:.3f}"
        f" ({20 * np.log10(abs(args.gain)):.1f} dB)"
    )
    print("\nListening — start requester.py to connect")
    sys.stdout.flush()

    request_count = 0

    with Replier(args.endpoint, CF64) as rep:
        while keep_running:
            try:
                samples, hdr = rep.recv(timeout_ms=500)
            except TimeoutError:
                continue

            # Apply gain and reply.
            result = samples * args.gain
            rep.send(
                result,
                sample_rate=hdr.get("sample_rate", 0),
                center_freq=hdr.get("center_freq", 0),
            )

            request_count += 1
            n = len(samples)
            in_pwr = _power_db(samples)
            out_pwr = _power_db(result)
            rate = hdr.get("sample_rate", 0)
            print(
                f"  [{request_count:4d}]  n={n}  "
                f"fs={rate / 1e6:.2f} MHz  "
                f"in {in_pwr:+.1f} dB  out {out_pwr:+.1f} dB"
            )
            sys.stdout.flush()

    print("\nDone.")


if __name__ == "__main__":
    main()
