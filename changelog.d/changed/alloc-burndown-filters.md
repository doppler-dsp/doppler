- **The filter and resampler cores size their allocations through the
    checked helpers**: RateConverter, fir, hbdecim, hbdecim_q15,
    hbdecim_r2c, resamp, ddc, ddcr, delay, doppler_channel, interp_table
    and fft2d call `dp_mallocn`/`dp_reallocn`/`dp_callocn` and their
    aborting twins, and leave `scripts/.alloc-product-ratchet`. The sizes
    that used to wrap now refuse; see Fixed.
