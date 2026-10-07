- **A refused `execute_ctrl` / `execute_profile` returns a negative count, not
    0** (doppler#1869). `dp_Resampler_execute_ctrl` and
    `dp_doppler_channel_execute_profile` now return `int64_t`: the samples
    written, or `DP_ERR_INVALID` for a `ctrl`/`ppm` shorter than `x`, a NULL
    pointer, or a bad profile sample. 0 is still a valid, empty result, so the
    sign alone tells a C caller which it got. Python raises `ValueError` where
    `DopplerChannel.execute_profile` used to return an empty array.
