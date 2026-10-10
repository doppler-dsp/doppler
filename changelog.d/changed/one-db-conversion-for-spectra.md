- **Every dB spectrum reads through PSD's one conversion** (#2108).
    `ToneMeasure`, `IMDMeasure` and `NPRMeasure`'s `spectrum_dbfs`, and the
    spectrum analyzer's display, each computed a private 10·log10 over the
    PSD they compose. They now return that PSD's own `psd_db` (the
    analyzer's cropped and offset). A reading moves by at most 3.25e-4 dB,
    and the measurement spectra's floor is PSD's −200 dB, where it was about
    −300. A `make lint` gate, `db-conversion`, keeps any new converter of
    an array to dB out of library C. Scalar measurements keep double.
