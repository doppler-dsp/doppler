- **The published benchmark page moves to v0.43.0**, five releases after it
    last did. `benchmarks/published` had stalled at v0.37.3 because `make bench`
    was the one unpinned jm call site and had been silently resolving to a
    version too old to understand the manifest — fixed earlier in this cycle,
    and this is the first publish since.

    Measured with `make bench-interleaved` (five alternating passes per build,
    per-benchmark best kept) on `030b7679`, so the two columns are free of the
    cross-run drift the old two-pass `bench-publish` picked up.

    The new `syncword` rows are the most native-sensitive thing on the page:
    the marker search is an XOR-and-sum over every offset, which vectorises,
    so `-march=native` buys **+45 % at a 32-bit marker and +97 % at 256** over
    the portable wheel build. `max_errors_for` is flat at +2 %, as a scalar
    log-space sum should be.
