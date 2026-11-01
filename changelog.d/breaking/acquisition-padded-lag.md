- **`Acquisition` reads an awkward code period on a power-of-two lag grid.**
    A period that is not 7-smooth (2,046 samples: 1,023 chips at two a chip)
    is read on `lag_bins` columns, the next power of two, by zero-padding
    each tile's cross-spectrum (`dp_fft_pad_spectrum()`, now public). A
    continuous hit's `code_phase` is a lag column, not a sample, `surface()`
    is `surface_rows` x `lag_bins`, `acq_result_t` gains `test_stat_peak`
    (rebuild), and the state blob is version 7 (`acq-surface.md`).
