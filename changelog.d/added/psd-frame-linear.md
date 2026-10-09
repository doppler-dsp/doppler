- **`dp_psd_frame_linear`: one frame in full-scale² units, against PSD's
    own reference.** The linear twin of `dp_psd_frame_db`, read from the same
    quotient, so a consumer that averages frames itself (the Spectrogram's
    power mode, #1894) gets a full-scale tone at 1.0 under every window
    instead of the raw power, which sits 20·log10(Σw) above it (54.18 dB for
    Hann at 1024 points). `frame_db` is byte-identical to before.
