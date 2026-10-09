- **`FFT`, `FFT2D`, `Corr` and `Corr2D` `execute*` refuse any input that is
    not exactly the plan length.** Pass exactly `n` samples (`ny*nx` for the
    2-D objects). Before, a wrong length was accepted silently: a short
    array was a heap over-read, a long one was truncated. Now the C kernels
    return `SIZE_MAX` (nothing read, written or counted; a dwell does not
    advance) and Python raises `ValueError`. The 12 entry points are
    `FFT.execute_cf32 / cf64 / inplace_cf32 / inplace_cf64 / ci16 / ci8`,
    `FFT2D.execute_cf32 / cf64 / inplace_cf32 / inplace_cf64`,
    `Corr.execute` and `Corr2D.execute` (#1925). `execute_ci16` / `ci8` still
    accept an odd element count and ignore the stray last value (#1933).
