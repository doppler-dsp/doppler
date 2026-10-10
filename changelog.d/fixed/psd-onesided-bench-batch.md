- **`psd::power_onesided` is timed over a batch of calls, not one**
    (#2143). One call per round put each pass on the clock's ~10 ns step,
    so at 1,024 bins the cell moved in 7% steps: #2131's +7.1% was one.
    A round now reads 16,384 bins' worth (16 calls at 1,024, at least 4),
    and the row still reports per call. Its first run after this is a new
    baseline: the clock read's own cost is now shared by the batch.
