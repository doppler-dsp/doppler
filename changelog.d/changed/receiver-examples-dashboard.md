- **Both receiver examples now show the frame header, throughput and
    latency — and show the same ones.** `examples/c/receiver.c` and
    `src/doppler/examples/receiver.py` render a dashboard that is identical
    field for field: the wire header as it arrived (version, format with its
    BLUE code, kind, flags, `data_rep`, sequence, `num_samples`,
    `payload_bytes`), power in dBFS, sustained rate in MS/s and MB/s, and
    one-way latency with min/mean/max.

    The latency is what `timestamp_ns` is for and nothing was using: it is
    the sender's own stamp against arrival, so the display says plainly that
    it only means something when both ends share a clock. Throughput is
    measured from the first frame rather than from start-up, so waiting for
    a sender does not depress the number for a minute afterwards.

    Parity is not cosmetic here — the power comes from one shared C
    primitive on both sides, so the two dashboards cannot disagree about a
    frame, and the Python one no longer needs numpy to compute it.
