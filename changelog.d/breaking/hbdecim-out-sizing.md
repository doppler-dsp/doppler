- **`HalfbandDecimator.execute_max_out` takes the block length, and
    `HBDECIM_MAX_OUT` is gone.** Old: `execute_max_out()` (C:
    `dp_HalfbandDecimator_execute_max_out(s)`) returned a fixed 32768, and
    the binding demanded that much of every `out=`. New:
    `execute_max_out(len(x))` (C: `(s, x_len)`) returns the real bound,
    `(len(x) + 1) // 2`, so an `out=` that fits the result works. The public
    `HBDECIM_MAX_OUT` macro is removed; size from the call instead (#2644).
