- **`dp_acq_create_burst`, `dp_burst_acq_create` and `dp_burst_capture_create` /
    `_backed` take a trailing `size_t rate_hypotheses`; `acq_result_t` and
    every Python hit tuple gain a last field, `doppler_rate_hz_s`; an acquisition
    state blob is version 6.** Pass `1` for today's behaviour. The Python
    constructors default `rate_hypotheses=1`, so no constructor call moves, but a
    caller unpacking a hit as 8 fields now gets 9, and a v5 blob is refused.
