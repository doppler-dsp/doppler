- **Acquisition's realized false-alarm rate is measured, decomposed and
    ratcheted — a caller gets ~1.65× the rate they configure.** The threshold
    sizes `pfa_cell` from the native cell count while the gate is decided by a
    maximum over the Doppler-*interpolated* surface, and a maximum over a
    finer sampling of the same band-limited process is stochastically larger.
    Measured at `pfa=1e-2` over 60,000 noise frames, it is two errors of
    opposite sign: `N_eff(1)/N` = **0.89 ± 0.04** (the native model is
    conservative — adjacent DFT bins are not perfectly independent) times
    `N_eff(2)/N_eff(1)` = **1.86 ± 0.09** (what interpolation adds). The
    *form* is right: the ratio holds across three decades of target, which is
    a cell-count error rather than a miscalibrated per-cell threshold. The
    certification records it as F7 and ratchets it, the characterization
    predicts its sweep from the delivered rate, and
    `docs/design/dsss-acquisition.md` §9.1 derives the model.
    [#1064](https://github.com/doppler-dsp/doppler/issues/1064)
