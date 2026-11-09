- **Filter and resampler sizes that overflowed a `size_t` wrote past short
    buffers.** The byte count wrapped to a few bytes and the call then wrote
    every element (64-bit counts): `dp_ddc_execute`/`_ctrl` and
    `dp_RateConverter_execute`/`_ctrl` at `n_in >= 2^61`, `dp_fir_execute`
    near `SIZE_MAX`, `dp_hbdecim_create`/`dp_hbdecim_r2c_create` at
    `num_taps >= 2^62`, `dp_interp_table_create` at `table_len >= 2^60`,
    `dp_delay_create` above `2^62` taps, and `dp_resamp_create_custom` when
    `num_phases * num_taps` wraps. Each now refuses: an execute returns 0
    with its state untouched, a create returns NULL.
