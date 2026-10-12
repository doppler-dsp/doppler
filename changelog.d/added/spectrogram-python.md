- **`Spectrogram`, the streaming short-time power spectrum, in Python.**
    Chunks of any size in, one PSD row per `hop` samples out (`hop` defaults
    to `nfft`), `flush` to end the stream; power rows by default, `mode="db"`
    by name. The configuration reads back; the state round-trips bit for bit
    and refuses a blob from another configuration (#1894, #2102).
