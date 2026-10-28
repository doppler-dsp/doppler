- **`Corr2D` and `CorrDetector2D` now honor `nthreads` on the single-row
    fast path.** It used to be ignored. Now 1 is serial, N runs N workers
    capped at `ny`, and `nthreads <= 0` means every online core. The output
    is bit-identical across counts. The general 2-D path and `col_out` stay
    serial. With `dwell >= 2`, the map now differs from v0.76.0 at float
    rounding, about 1e-7 relative. The accumulate loop compiles differently
    under `-ffast-math` (#2666).
