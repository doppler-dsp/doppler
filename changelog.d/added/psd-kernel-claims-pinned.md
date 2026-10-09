- **PSD's per-frame kernel has its header claims pinned in C**, the base the
    Spectrogram's "a row is that frame's PSD" stands on: the average is left
    untouched, 0 dBFS under every window and both references, the -200 dB
    floor, negative and padded bins, ENBW against published coefficients, and
    the transform length. Test-only; part a of #1911.
