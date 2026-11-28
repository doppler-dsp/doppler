- **`AsyncDsssPool` sizes each receiver's row from the receiver's own
    bound, and checks that every call took its period.** A row holds a
    push's calls times the most one call of a code period can produce (the
    receiver's `steps_max_out()` at a code period), in place of the pool's
    own rule, which was about a code period of samples in symbols for every
    call: measured, the most one call returns across the pool's tests is 2
    symbols against a bound of 61. A Debug build aborts on a refused period
    rather than leaving it unfed (#3398).
