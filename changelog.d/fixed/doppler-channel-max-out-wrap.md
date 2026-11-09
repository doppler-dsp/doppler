- **`DopplerChannel.execute_profile` wrote past its position scratch when
    `max_out` was huge.** The C call sized that scratch as
    `max_out * sizeof (double)`, so a `max_out` above `SIZE_MAX / 8`
    wrapped to a few bytes, and the call then wrote one position per
    output past them. `dp_doppler_channel_execute_profile` now refuses such
    a `max_out` with `DP_ERR_INVALID` before producing anything, like its
    other refusals: no output, elapsed time unchanged.
