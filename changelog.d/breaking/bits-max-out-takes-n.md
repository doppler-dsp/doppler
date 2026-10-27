- **`MpskReceiver.bits_max_out` and `BpskReceiver.bits_max_out` take the
    block length.** Old: `bits_max_out()`, which returned 0 ("size `out` to
    the input") and let 8PSK at `sps` below 3 lose bits. New:
    `bits_max_out(len(x))`, which returns `len(x) * log2(m)`, the bound
    `bits()` can actually fill. Size an `out=` buffer from it (#2643).
