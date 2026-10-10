- **AccTrace's mean is a per-bin sum, and every fold vectorizes** (#2094).
    The mean mode sums each bin and divides once when the trace is read, so
    there is no per-frame divide. Over 10⁷ frames its error is 2.4e-14, ten
    times smaller than the Welford update it replaced. Max/min-hold are now
    a select, which the portable build vectorizes, about 7–10× faster there.
    The state blob is version 3; a version-2 blob is refused, including one
    nested in `PSD`, `Specan` or `CarrierAcquisition`.
