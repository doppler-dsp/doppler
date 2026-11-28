- **`AsyncDsssReceiver` and `DsssReceiver` size `out` from the call's
    length.** `dp_async_dsss_receiver_steps_max_out(state, x_len)` and
    `dp_dsss_receiver_steps_max_out(state, x_len)` (Python:
    `steps_max_out(n)`) return the symbols held plus the most a call of
    `x_len` samples can produce, a true bound in place of the 0 that meant
    "use `x_len`". Their blobs gain the held symbols' store: async v24,
    DSSS v6 (#2469).
