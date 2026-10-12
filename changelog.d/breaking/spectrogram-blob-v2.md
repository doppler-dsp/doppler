- **A Spectrogram state blob is version 2 and carries its configuration, so
    `dp_spectrogram_set_state` refuses one from another mode, `nfft`, hop,
    window or Kaiser beta, and refuses a version-1 blob.** v0.66.0 wrote
    version 1, and its header said another window, beta or mode restores (#2102).
