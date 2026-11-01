- **`Acquisition` reads an awkward code period on a power-of-two lag grid.**
    When the period is not 7-smooth (2,046 samples for a 1,023-chip code at
    two a chip), each tile's cross-spectrum is zero-padded to the next power
    of two before its inverse transform: an exact interpolation of the same
    circular correlation, at a fifth of the inverse's cost. The surface is
    `surface_rows` x the new `lag_bins` columns, a column is `code_bins /   lag_bins` of a sample, and `surface_chip_phase()` maps each to chips. The
    padding is `dp_fft_pad_spectrum()`, now public, and corr2d's own.
- **A second emitter beside a stronger one is judged at its own cell.**
    `AsyncDsssReceiver`'s pick took a peak as a second emitter only if it
    cleared the stronger one's worst sidelobe anywhere (-17 dB on the
    1,023-chip Gold code) by the gate, so at the design floor a weaker
    emitter was never taken beside a stronger one. It now bounds each peak
    by what the stronger emitter's sidelobes can read at that cell
    (`dp_acq_sidelobe_ratio()`), from its statistic on its own lag (the new
    `test_stat_peak` on each hit). On a deep block the worst sidelobe stays
    a floor until a strong emitter's data splatter is modelled (#2730).
