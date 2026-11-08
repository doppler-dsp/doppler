- **Seven correlator classes are one: `doppler.correlation.Correlator`.**
    Its knobs pick the flavor, and `flavor` says which they built
    ([guide](docs/guide/correlator.md)). `Corr`, `Corr2D` and
    `DelayDoppler` (its `ny` is `doppler_rows`) are `Correlator(ref, ...)`;
    `CorrDetector`/`CorrDetector2D` add `detect=1`; `Acquisition` is
    `Correlator(nrz, framing="continuous")` with the chips as +1/-1
    samples; `BurstAcquisition` is `Correlator(p, framing="burst")`, its
    `fs` now `sample_rate`. A property the flavor does not serve raises
    `AttributeError`, and a method `ValueError`; a refused constructor
    argument raises `ValueError` naming the knob (#2752).
