- **`PSD` takes a `hop`, and carries its partial segment.** `PSD(hop=...)` sets the
    spacing between segments, from 1 to `n`; `0` (the default) means `n`, which keeps
    the old non-overlapping behaviour. `accumulate` now holds a trailing partial
    segment for the next call instead of dropping it, so any chunking of a stream
    averages to the same result as one call. A single call of whole segments at
    `hop = n` is unchanged, bit for bit (#1894). `accumulate_real` is unchanged and
    still drops its tail.
- **The `PSD` state blob grew.** It now holds the carried segment, and its version is
    2, so an older blob is refused. Anything that embeds a `PSD` state (the specan,
    carrier-acquisition and measurement blobs) grows by the same amount.
