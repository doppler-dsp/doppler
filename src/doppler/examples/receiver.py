"""receiver.py — NATS SUB receiver example.

Subscribes to a doppler transmitter and displays a live dashboard: the
frame header as it arrived on the wire, signal power, throughput, one-way
latency, packet statistics and the first few samples.  Field for field
the same dashboard as the C receiver example, computed the same way --
the power comes from `doppler.stream.mean_power()`, which is the same
`dp_mean_power()` the C example calls, not a second implementation in
numpy.  Requires a running nats-server (e.g. `nats-server -js`).

Usage:
  python receiver.py [endpoint]
  python receiver.py                          # nats://127.0.0.1:4222/iq
  python receiver.py nats://broker.example:4222/iq

Press Ctrl+C to stop.
"""

import argparse
import math
import signal
import sys
import time

import numpy as np

from doppler.interrupt import Interrupt
from doppler.stream import (
    CF64,
    Subscriber,
    format_name,
    get_timestamp_ns,
    mean_power,
)


def _fmt_chars(code: int) -> str:
    """The BLUE two-character format code the header carries, as text."""
    return bytes([code & 0xFF, (code >> 8) & 0xFF]).decode("ascii", "replace")


def _format_ts(ts_ns: int) -> str:
    """Local wall-clock time, the same rendering native/examples/receiver.c
    produces with localtime_r -- the two dashboards must agree on the
    number they print for one frame."""
    secs = ts_ns // 1_000_000_000
    ms = (ts_ns % 1_000_000_000) // 1_000_000
    tm = time.localtime(secs)
    return f"{tm.tm_hour:02d}:{tm.tm_min:02d}:{tm.tm_sec:02d}.{ms:03d}"


def frames_missing(last_seq: int, seq: int) -> int:
    """Frames skipped between two consecutive frames' ``sequence`` numbers.

    The header's ``sequence`` counts frames per publisher from 0, so a jump
    forward from ``last_seq`` to ``seq`` skipped ``seq - last_seq - 1``
    frames. A sequence that repeats or goes BACKWARDS skipped none. A SUB
    socket's delivery is at-most-once (docs/design/streaming.md, section 9),
    so no frame comes twice: a repeat or a backward step means the
    publisher restarted, counting from 0 again, or a second publisher
    shares the address. Subtracting regardless added a negative to the
    dropped count on every restart (#2017). The C twin is
    ``receiver_frames_missing`` in ``native/examples/receiver_seq.h``.

    Parameters
    ----------
    last_seq : int
        The previous frame's ``sequence``.
    seq : int
        This frame's ``sequence``.

    Returns
    -------
    int
        The frames skipped, never negative.

    Examples
    --------
    >>> frames_missing(4, 7)
    2
    >>> frames_missing(1000, 0)  # a restarted publisher, not a drop
    0
    """
    return seq - last_seq - 1 if seq > last_seq else 0


class SequenceCount:
    """The running dropped-frame count over a stream: what ``main()`` keeps
    between frames. The C twin is ``receiver_seq_t`` and
    ``receiver_seq_feed`` in ``native/examples/receiver_seq.h``.

    The first frame anchors the count and adds nothing, so a receiver that
    joins mid-stream (first frame 1,000,000) has dropped nothing yet. Each
    later frame adds :func:`frames_missing` from the previous one and
    becomes the anchor whichever way it moved: after a restart the count
    follows the new numbering, so a gap there counts too.

    ``dropped`` is a LOWER bound across a restart. The frames a restarting
    publisher sent before it went down but after the last one received,
    and any it sent after coming up but before the first one received,
    leave no gap in either numbering, so nothing counts them.

    Attributes
    ----------
    last : int or None
        The previous frame's ``sequence``; None before the first frame.
    dropped : int
        The frames skipped so far.

    Examples
    --------
    >>> count = SequenceCount()
    >>> for seq in [7, 8, 9, 12, 0, 1, 3]:  # a gap, a restart, a gap
    ...     count.feed(seq)
    >>> count.dropped
    3
    """

    def __init__(self) -> None:
        self.last: int | None = None
        self.dropped = 0

    def feed(self, seq: int) -> None:
        """Take one frame's ``sequence``.

        Parameters
        ----------
        seq : int
            The frame's header ``sequence``.
        """
        if self.last is not None:
            self.dropped += frames_missing(self.last, seq)
        self.last = seq


def main() -> None:
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "endpoint", nargs="?", default="nats://127.0.0.1:4222/iq"
    )
    args = parser.parse_args()

    print(
        f"doppler Receiver (Python)\n  Endpoint: {args.endpoint}\n"
        f"\nWaiting for packets..."
    )
    sys.stdout.flush()

    total_samples = 0
    total_bytes = 0
    packet_count = 0
    count = SequenceCount()

    # Throughput is measured from the FIRST frame, not from start-up: the
    # wait for a sender to appear is not part of the rate, and counting it
    # makes the number climb for a minute after the stream begins.
    t_first_ns = 0

    # End-to-end latency, from the sender's own timestamp_ns to the moment
    # this process holds the samples. Both stamps are CLOCK_REALTIME, so the
    # figure is only meaningful when the two ends share a clock -- the same
    # host, or hosts disciplined by PTP/NTP. Across an undisciplined pair it
    # measures clock offset, not transport.
    lat_sum_ms = 0.0
    lat_min_ms = float("inf")
    lat_max_ms = 0.0

    # No timeout on recv(): it BLOCKS, which is what a dashboard wants --
    # it has nothing to do between frames. That is only safe because the
    # Interrupt guard is holding a C-level handler that unblocks the wait;
    # a Python handler cannot, because it runs only when the interpreter
    # regains control and the blocking recv is what prevents that. SIGTERM
    # is included so a container stop behaves like a Ctrl+C -- and both are
    # named, where the retired interrupt_on_sigint() folded SIGINT in
    # silently.
    stop_on = np.array([signal.SIGINT, signal.SIGTERM], dtype=np.int32)
    with Subscriber(args.endpoint) as sub, Interrupt(stop_on):
        while True:
            try:
                samples, hdr = sub.recv()
            except KeyboardInterrupt:
                break

            now = get_timestamp_ns()
            n = int(hdr.get("num_samples", len(samples)))
            packet_count += 1
            total_samples += n
            total_bytes += int(hdr.get("payload_bytes", 0))
            if t_first_ns == 0:
                t_first_ns = now

            # Forward gaps only: a restarted publisher's sequence goes back
            # to 0, which is not a drop (SequenceCount).
            seq = hdr.get("sequence", 0)
            count.feed(seq)

            sent = int(hdr.get("timestamp_ns", 0))
            lat_ms = (now - sent) / 1e6 if now > sent else 0.0
            lat_sum_ms += lat_ms
            lat_min_ms = min(lat_min_ms, lat_ms)
            lat_max_ms = max(lat_max_ms, lat_ms)

            # One call, whatever the wire carried: mean_power normalises the
            # integer formats by full scale, so this is dBFS either way and
            # the example does not branch on the type to say so.
            pwr_db = 10.0 * math.log10(mean_power(samples) + 1e-12)

            ts = _format_ts(sent)
            rate = hdr.get("sample_rate", 0)
            freq = hdr.get("center_freq", 0)
            fmt = int(hdr.get("format", CF64))
            kind = int(hdr.get("kind", 0))
            flags = int(hdr.get("flags", 0))
            version = int(hdr.get("version", 0))
            payload = int(hdr.get("payload_bytes", 0))

            mb = total_bytes / 1_048_576
            secs = (now - t_first_ns) / 1e9 if now > t_first_ns else 0.0
            msps = total_samples / secs / 1e6 if secs > 0 else 0.0
            mbps = mb / secs if secs > 0 else 0.0
            kind_s = "TLM (records)" if kind else "IQ (samples)"
            chunked = "  CHUNKED" if flags & 1 else ""

            lat_mean_ms = lat_sum_ms / packet_count
            show = min(5, len(samples))
            sample_lines = "\n".join(
                f"    [{i}] I: {samples[i].real:+.6f}, "
                f"Q: {samples[i].imag:+.6f}"
                for i in range(show)
            )

            print(
                f"\033[2J\033[H"
                f"doppler Receiver (Python)\n"
                f"=========================\n"
                f"  Endpoint:     {args.endpoint}\n"
                f"  Sample Rate:  {rate / 1e6:.2f} MHz\n"
                f"  Center Freq:  {freq / 1e9:.2f} GHz\n\n"
                # The frame header as it is on the wire -- the fields a
                # receiver written against docs/design/streaming.md parses.
                f"  -- frame header (v{version}) --\n"
                f'  format:       {format_name(fmt)} ("{_fmt_chars(fmt)}"'
                f", {payload // n if n else 0} B/sample)\n"
                f"  kind:         {kind_s}\n"
                f"  flags:        0x{flags:04X}{chunked}\n"
                f"  data_rep:     {hdr.get('data_rep', '')}\n"
                f"  sequence:     {seq}\n"
                f"  timestamp:    {ts}\n"
                f"  num_samples:  {n}\n"
                f"  payload:      {payload} bytes\n\n"
                f"  Power:        {pwr_db:.2f} dBFS\n"
                f"  Rate:         {msps:.2f} MS/s  ({mbps:.1f} MB/s)\n"
                f"  Latency:      {lat_ms:.3f} ms   "
                f"(min {lat_min_ms:.3f}, mean {lat_mean_ms:.3f}, "
                f"max {lat_max_ms:.3f})\n"
                f"                one-way, sender clock -> here; only\n"
                f"                meaningful if both share a clock\n\n"
                f"  Packets:      {packet_count}\n"
                f"  Total:        {total_samples} samples "
                f"({mb:.2f} MB in {secs:.1f} s)\n"
                f"  Dropped:      {count.dropped}\n\n"
                f"  First {show} samples:\n{sample_lines}\n\n"
                f"Press Ctrl+C to stop."
            )
            sys.stdout.flush()


if __name__ == "__main__":
    main()
