- **`dp_spectrogram_create` takes `hop = 0` to mean `nfft`, PSD's rule.**
    v0.66.0 refused 0; a hop above `nfft` is still refused (#1894).
