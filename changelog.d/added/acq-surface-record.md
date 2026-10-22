- **The acquisition surface's §8 numbers come from a validator, not a
    scratch script.** `validate_acq_surface` computes the code's ambiguity at
    a tile offset (the band's worst is −16.95 dB, at −4500 Hz), what a window
    on either transform costs, and what the engine's peak lists hold. Its
    `--check` is a CTest, and `make docs-check` fails when a table on
    [the page](../../docs/design/acq-surface.md) is not what its run renders.
    `make acq-surface-record` re-runs it.
