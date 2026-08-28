- **`BurstDemod` is certified** — 15 limits on every push. The gaps clustered
    on the read-backs, which are what a caller consumes: `frame_valid`'s only
    negative case was an 8-sample input that returns before a CRC is ever
    computed, so a `frame_valid` ignoring the trailer entirely would have
    passed; `frame_offset` had only ever been observed as 0, its degenerate
    value; `n_symbols` and `reset()` were mentioned in neither language.
    Evidence: `src/doppler/dsss/tests/validation/burst_demod/results.md`.
