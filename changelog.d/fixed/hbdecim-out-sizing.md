- **`HalfbandDecimator.execute(x, out=...)` accepts an `out` that fits the
    result.** `execute_max_out` returned a fixed 32768, and the binding
    demanded that much of every `out=` and allocated it on every call. It
    now takes the input length and returns the real bound,
    `(len(x) + 1) // 2`, so `out=np.empty(len(x) // 2 + 1)` works (#2644).
