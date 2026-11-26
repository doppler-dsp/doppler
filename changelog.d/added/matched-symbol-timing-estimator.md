- **`MatchedSymbolTimingEstimator`: the feedforward timing estimate with
    its own matched filter.** Give it the pulse and the input's samples
    per symbol (any real number from 4 up) and hand it unfiltered samples:
    it decimates to 4 samples a symbol through a matched converter, skips
    that converter's start, and estimates. Every `TimingEstimate` now also
    carries `eye`, the eye center as a sample index into the samples
    handed over. See
    [the guide](docs/guide/tracking-loops.md#symboltimingestimator-timing-from-one-block-with-no-loop).
