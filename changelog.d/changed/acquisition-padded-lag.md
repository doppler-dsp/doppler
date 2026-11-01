- **A second emitter beside a stronger one is judged at its own cell.**
    `AsyncDsssReceiver`'s pick took a peak as a second emitter only if it
    cleared the stronger one's worst sidelobe anywhere (-17 dB on the
    1,023-chip Gold code) by the gate, so at the design floor a weaker
    emitter was never taken beside a stronger one. It now bounds each peak
    by what the stronger emitter's sidelobes can read at that cell
    (`dp_acq_sidelobe_ratio()`), from its statistic on its own lag (the new
    `test_stat_peak` on each hit). On a deep block the worst sidelobe stays
    a floor until a strong emitter's data splatter is modelled (#2730).
