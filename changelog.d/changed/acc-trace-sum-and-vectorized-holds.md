- **AccTrace's mean is a per-bin sum, and every fold vectorizes** (#2094).
    The mean sums each bin and divides once when it is read: over 10⁷
    frames the error is 2.4e-14, ten times smaller than before. Max/min-hold
    are now a select, about 7–10× faster in the portable build. **What
    callers see:** a mean reading can differ from the previous release by
    one float32 ULP (147 of 4.1e7 readings did). The public
    `dp_acc_trace_state_t::acc` field now holds the SUM in mean mode, so an
    external C reader of it must divide by `count`. An Inf frame now
    leaves a mean at +Inf, where it read NaN before. A non-finite frame makes
    a non-finite trace, and a blob carries and restores it. The state blob
    is version 3, and a version-2 blob is refused, including one nested in
    `PSD`, `Specan`, `CarrierAcquisition`, `AsyncDsssReceiver` or
    `AsyncDsssPool`. So is a forged blob with a zero count and a nonzero
    trace.
