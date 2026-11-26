- **`PolynomialPhaseEstimator` and `PolynomialPhaseEstimate` moved from
    `doppler.dsss` to the new `doppler.estimation`.** The module holds the
    feedforward estimators (a block in, one estimate out, no loop):
    carrier phase, frequency and frequency rate, and symbol timing
    (`SymbolTimingEstimator`, `MatchedSymbolTimingEstimator`). Change the
    import; the classes and the C API (`dp_ppe_*`) are unchanged.
