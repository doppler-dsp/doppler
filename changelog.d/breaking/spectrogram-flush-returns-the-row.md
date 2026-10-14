- **`row = sp.flush()` replaces `sp.flush(buf)`.** `Spectrogram.flush()`
    returns the owed row as a `(1, nfft)` matrix, or `None`, as `push` returns
    rows; C gains `dp_spectrogram_flush_max_out` (#2181).
