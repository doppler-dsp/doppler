- **`Resampler` and `RateConverter` refuse a rate or phase count they cannot
    use.** Every rate entry point now needs a finite rate above 0 (NaN used to
    pass). A bank whose `num_phases` is not a power of two is refused; it used
    to read past the end of the bank. `Resampler()` now defaults to rate 1.0,
    not 0.0, and a bad `Resampler.rate` raises `ValueError`. Setting `rate`
    across 1 starts the new path fresh. A refused or failed
    `RateConverter.rate` change leaves the converter as it was (#2640).
