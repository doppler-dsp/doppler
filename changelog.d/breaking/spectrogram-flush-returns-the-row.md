- **`Spectrogram.flush()` returns the owed row, as `push` returns its rows.**
    It is a `(1, nfft)` float32 matrix, or `None` when no row is owed, and it
    takes an optional `out=` of that shape. It used to fill a caller's nfft
    buffer and return a count, so every caller wrote the same three lines and
    had to remember nfft. The C kernel is unchanged, and
    `dp_spectrogram_flush_max_out` is new (#2181, jm gh-2233).
