- **`BurstDespreader` keeps the carrier frequency it is given during a
    preamble.** Every write of a frequency into the carrier loop
    (`norm_freq`, the create/reset seed, `set_acq`'s period switch) now scales
    by the symbol period in force, so a value set during a `set_acq`
    preamble no longer comes back as `val * sf / acq_sf` one symbol later
    (#2648).
