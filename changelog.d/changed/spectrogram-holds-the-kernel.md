- **A Spectrogram allocates 28·nfft fewer bytes, virtual, not resident.** It
    holds PSD's kernel, not a PSD whose averager, dB buffer and ring it never
    touched. The 28 is derived (12 of heap plus the ring's 16 of memfd,
    counted once). Rows and state blobs are byte-identical on x86-64;
    off-x86 was not compared (#2187) (#1894, design §5.10).
