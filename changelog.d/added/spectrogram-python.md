- **`Spectrogram`, the streaming short-time power spectrum, in Python.**
    Chunks of any size in, one PSD row of `nfft` bins per `hop` samples out
    (`hop` defaults to `nfft`); power rows by default, `mode="db"` by name.
    Its configuration reads back, and its state round-trips bit for bit (#1894).
