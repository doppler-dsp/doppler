- **Coverage counted the validator harnesses as library code.** `COV_IGNORE`
    dropped `native/tests/` and `native/benchmarks/` but never named
    `native/validation/`, so 9310 lines of harness — a quarter of the
    denominator — were scored as first-party source. The reported total moves
    from 84.36% to 86.05%, the number for doppler's own code.
    [#1292](https://github.com/doppler-dsp/doppler/issues/1292).
