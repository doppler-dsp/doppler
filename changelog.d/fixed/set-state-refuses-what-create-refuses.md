- **`set_state` refuses what `create()` refuses, for `Corr`, `Pn`, `Gold`
    and `LoopFilter`.** A forged blob kept its size and passed the envelope:
    a `Corr` count at or past the dwell never dumped again, a zero or
    over-wide `Pn`/`Gold` register was stuck or leaked into the sequence, and
    a NaN loop-filter gain reached `LoopFilter`, the ratesync loop and the
    MPSK carrier filter. Each restore now checks the payload before it
    commits (the loop filter through the new `DP_DEFINE_POD_STATE_CHECKED`),
    and `Corr(dwell=0)` is refused with `ValueError` (#2641).
