- **Acquisition's realized false-alarm rate is measured, documented and
    ratcheted — it is ~1.8× the configured target.** The threshold ladder
    sizes `pfa_cell` from the native cell count while the peak search runs on
    the Doppler-interpolated surface, and a maximum over a finer sampling of
    the same band-limited process is stochastically larger. Measured with only
    `ACQ_DOPPLER_INTERP` changed: **1.85× target at +5.4σ with interpolation
    on, 0.90× at −0.6σ with it off.** The certification records it as F7 and
    ratchets it, the characterization now predicts its sweep from the
    delivered rate rather than the configured one (which is what made it fail
    on a detector obeying its own threshold), and `docs/design/dsss-acquisition.md`
    §9 carries the tradeoff.
    [#1064](https://github.com/doppler-dsp/doppler/issues/1064)
