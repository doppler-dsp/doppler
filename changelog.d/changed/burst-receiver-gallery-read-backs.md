- **The `DsssBurstReceiver` example now presents every read-back, not just
    the bursts.** All nine fields of `events()` are printed per burst and
    checked against the scene that produced them — the bin width against
    `fs/(sf*spc)`, the C/N0 estimate against the segment's own Es/N0, the
    coarse Doppler against its bin, the refined residual against a hundredth
    of it — plus two figure panels and the assertion that the scalar
    properties are exactly the last event row. Both faces do it: the C
    example gained the same section.
