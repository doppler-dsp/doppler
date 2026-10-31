- **AccTrace's mean is a per-bin sum, and every fold vectorizes** (#2094).
    Mean readings are 10× more accurate over 10⁷ frames and may differ by
    one float32 ULP (147 of 4.1e7). Max/min-hold folds are about 10×
    faster on x86 (portable and native) and 4–5× on aarch64. The public `acc` field now holds the SUM in mean mode
    (divide by `count`), an Inf frame leaves +Inf where it read NaN, and a
    non-finite trace round-trips. The blob is version 3: a version-2 blob
    is refused, nested ones too, and so is a zero count with a nonzero
    trace. The record is `spectrogram-measurements.md` §5.16.
