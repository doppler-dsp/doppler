- **`dp_spectrogram_flush` takes the row's length, and returns a signed count.**
    The signature is now `int64_t dp_spectrogram_flush (s, float *row, size_t row_len)`.
    A row that is not exactly `nfft` floats is refused as `-1` before anything
    moves, and the owed frame stays owed. Before, a short row returned 0, which
    read as "nothing owed". Callers pass the row's length. The Python face
    raises `ValueError` for that refusal (#1894).
