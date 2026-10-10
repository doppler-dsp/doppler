- **PSD windows each frame with the periodic (DFT-even) form** (#2053), the
    spectral-estimation convention, so every PSD reading moves, and with it
    the Spectrogram, the measure objects, the analyzer and
    CarrierAcquisition. A bin-centred Hann tone reads nothing beyond its two
    neighbours (−82 dBc leaked into bin 20 before), Blackman-Harris's ENBW
    is Harris's 2.004 bins (2.036 at `n = 64` before), and Hann at `n = 2`
    is `[0, 1]`, now accepted. `dp_psd_window` is the one builder, and
    `measure_min_samples` now plans from it: its plans were 0.1% long. The
    window functions stay symmetric.
