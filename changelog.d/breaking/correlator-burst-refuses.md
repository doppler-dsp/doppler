- **A burst search refuses the continuous search's calls in C**, touching
    nothing: `dp_correlator_set_keep_surface()` (now `int`),
    `set_carrier_freq_hz()` and `set_telemetry()` return
    `DP_ERR_UNSUPPORTED`; `surface()`, `surface_doppler_hz()`,
    `surface_chip_phase()`, `block_prompt()` and `block_raw()` return
    `SIZE_MAX`. None had a burst caller or test
    ([design §18](docs/design/correlator-python.md)).
