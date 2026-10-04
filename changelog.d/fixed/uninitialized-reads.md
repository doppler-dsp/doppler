- **Every `-Wmaybe-uninitialized` / `-Wuninitialized` in doppler's own C is
    gone** (22 under gcc 16, `-Wall -Wextra`; #1658). Three were real:
    `detector2d`'s argmax left `pk` unset if the peak list was empty; the
    `RateConverter` and `ratesync` tests sized a `calloc` from an `n` their
    helper had not set when it returned NULL; and `async_dsss_pool` and
    `wfm_synth` read a `set_state` header that `dp_r_bytes` leaves untouched
    on an errored reader. The rest were false positives of a documented
    contract (the output is written exactly when the step returns 1) and now
    initialise the local.
