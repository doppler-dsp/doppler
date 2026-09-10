- **`AsyncDsssPool` is the pool on cell receivers** (part of #1283, closes
    it): the constructor drops the `refine_*` arguments and takes `gain`
    and `pullin_intervals`; `code_only_epochs` defaults to 813 and must
    give a searcher depth a cell receiver can take (`D ≥ 13` at 5 Mcps over
    Gold-1023 — a windowed waveform); `set_refine_min_blocks()` and
    `refine_min_blocks` are gone. `CellAsyncDsssPool` was the same object
    for one release and is folded in. Design §8.2, §12.27–12.28.
