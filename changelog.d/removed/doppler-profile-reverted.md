- **The `DopplerChannel` per-sample Doppler profile is withdrawn**
    (`execute_profile`, and the `DOPPLER_CHANNEL_STATE_VERSION` 2 bump that
    carried its accumulator). It shipped with a defect found on review: the
    carrier it applied depended on how the caller CHUNKED the stream.

    The resampled samples were fine — `resamp` carries its own state — but
    the carrier mapped output sample `j` back to a profile index with
    `j * m / got`, a chord across each block. `m / got` changes with block
    size, so the profile-to-output pairing moved: the same 200000-sample
    stream and the same profile, fed in blocks of 10000 versus 50000,
    differed by 2.75e-02 in amplitude. That contradicts the object's own
    documented contract, that feeding a stream in blocks gives the same
    samples as one large call.

    The exact mapping is not a ratio; it is the inverse of the cumulative
    rate — the time warp the dilation performs — and the object's own
    `excess(t)` is where that lives. The split-resume test did not catch it
    because both halves happened to fit in a single internal block, so the
    chord was never re-cut.

    Withdrawn rather than patched: the scalar `(doppler_ppm,   doppler_rate_ppm_s)` form is what the current work needs, and a broken
    array form on `main` is worse than no array form. See doppler#940 for
    the design, the reproducer and the two open questions.
