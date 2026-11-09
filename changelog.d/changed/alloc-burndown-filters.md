- **The filter and resampler cores size their allocations through the
    checked helpers**: RateConverter, fir, hbdecim, hbdecim_q15,
    hbdecim_r2c, resamp, ddc, ddcr, delay, doppler_channel, interp_table
    and fft2d call `dp_mallocn`/`dp_reallocn`/`dp_callocn` and their
    aborting twins, and leave `scripts/.alloc-product-ratchet`. A `FIR`
    block too long to size is refused (0 outputs, state untouched), and
    `DopplerChannel`'s profile execute refuses with `DP_ERR_INVALID` a
    `max_out` whose position scratch cannot be sized, where both used to
    wrap to a short buffer.
