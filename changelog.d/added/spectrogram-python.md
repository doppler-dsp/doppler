- **`Spectrogram`, the streaming short-time power spectrum, in Python.** A
    stream arrives in chunks of any size; each whole frame is one row of
    `nfft` bins, the PSD of that frame, with no averaging. `push` returns the
    rows it completes, `flush` ends the stream with the zero-padded partial
    frame, and the state round-trips bit for bit into a fresh object. Power
    rows by default; `mode="db"` asks for dBFS rows by name (#1894).
