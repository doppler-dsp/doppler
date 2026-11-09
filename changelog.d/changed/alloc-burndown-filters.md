- **The filter and resampler cores size their allocations through the
    checked helpers**: RateConverter, fir, hbdecim, hbdecim_q15,
    hbdecim_r2c, resamp, ddc, ddcr, delay, doppler_channel, interp_table
    and fft2d (29 sites) call `dp_mallocn`/`dp_xmallocn`/`dp_xreallocn` or
    `dp_size_mul`, and leave `scripts/.alloc-product-ratchet`.
    `DopplerChannel`'s profile execute now refuses a `max_out` whose
    position scratch cannot be sized with `DP_ERR_INVALID`, where it used to
    wrap to a short buffer.
