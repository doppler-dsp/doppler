- **`FFT.execute_ci16` / `execute_ci8` refuse an odd element count.** An
    input of `2n + 1` int values passed the exact-length check and its last
    value was silently ignored; it now raises `ValueError`. Every wrong length
    on these two methods now reads one message, counted in int values and
    naming the length passed, e.g.
    `execute_ci16 takes exactly one frame: 2 * FFT.n = 128 interleaved I/Q values, got 129`
    (#1933).
