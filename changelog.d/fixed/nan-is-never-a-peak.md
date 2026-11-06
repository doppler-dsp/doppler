- **A NaN is never a peak, in `CorrDetector`, `CorrDetector2D` and
    `Acquisition`.** The one peak picker seeded its pick at the first cell,
    and nothing compares larger than a NaN, so a NaN there hid a finite peak
    elsewhere. It now skips NaN cells on both its paths. Finite data picks
    exactly as before. An all-NaN map now gives `CorrDetector` a peak of 0
    at lag 0, not NaN, as `CorrDetector2D` already did; at `threshold = 0`
    that is the hit it reports (#2752 step 3.3, design U2).
