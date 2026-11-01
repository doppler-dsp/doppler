- **`AsyncDsssReceiver` and `AsyncDsssPool` take `sample_rate`; `spc` is
    gone.** Any rate of at least one sample a chip: a whole code period at
    that rate is searched as is, and a fractional one (11 MSa/s over 5 Mcps)
    is resampled once, to a power-of-two period, for the search alone --
    the refine and the tracking loops run on the caller's own samples, the
    hand-off mapped across the two timebases. Pass `sample_rate=spc *   chip_rate` for the old behaviour. While refining, `status().chip_phase`
    now includes the code's nominal advance, so it matches a tracking
    receiver's at any feed size.
