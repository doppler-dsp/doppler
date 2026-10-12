- **`dp_acq_create_burst`, `dp_burst_acq_create` and
    `dp_burst_capture_create` / `_backed` take a trailing `size_t   rate_hypotheses`; `acq_result_t` gains `doppler_rate_hz_s`; an acquisition
    state blob is version 6.** Pass `1` for today's behaviour. The Python
    constructors default `rate_hypotheses=1`, so no Python call moves; a v5 blob
    is refused.
