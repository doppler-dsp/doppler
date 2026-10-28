- **`set_state` refuses what `create()` refuses, for `Corr`, `Pn`, `Gold`
    and `LoopFilter`.** A forged blob kept its size and passed the envelope:
    a `Corr` count at or past the dwell never dumped again, a zero or
    over-wide `Pn`/`Gold` register was stuck or leaked into the sequence
    (zero stays legal where the feedback lacks its top term and can reach
    it), and a NaN loop-filter gain reached `LoopFilter`, the ratesync loop
    and the MPSK carrier filter. Each restore now checks the payload before
    it commits (the loop filter through `DP_DEFINE_POD_STATE_CHECKED`), and
    `Corr`, `Corr2D`, `CorrDetector` and `CorrDetector2D` refuse `dwell=0`
    with `ValueError`, where it raised `MemoryError` (#2641).
