- **A Spectrogram allocates 28·nfft fewer bytes.** It holds PSD's kernel, not
    a PSD whose averager, dB buffer and ring it never used; rows and state
    blobs are byte-identical (#1894, design §5.10).
