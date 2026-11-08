- **A huge `doppler_uncertainty` no longer builds a one-tile search.** The
    tile count converted an out-of-range double to `size_t`, which is
    undefined: x86 read 1e300 Hz as 2^63 tiles, which wrapped to ONE, and
    the search quietly covered a single span. The count now saturates, and
    a search whose grid no `size_t` can hold is refused, naming
    `doppler_uncertainty` (or `reps`) (#2752).
