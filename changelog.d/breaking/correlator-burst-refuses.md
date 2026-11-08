- **A burst search refuses the continuous search's calls in C.** On an
    engine from `dp_correlator_create_burst()`, these used to run and now
    refuse, touching nothing: `dp_correlator_set_keep_surface()` (now
    returns `int`, `DP_ERR_INVALID`), `dp_correlator_set_carrier_freq_hz()`
    and `dp_correlator_set_telemetry()` (`DP_ERR_INVALID`), and
    `dp_correlator_surface()`, `dp_correlator_surface_doppler_hz()`,
    `dp_correlator_surface_chip_phase()`, `dp_correlator_block_prompt()`
    and `dp_correlator_block_raw()` (`SIZE_MAX`). None had a burst caller or
    test; `dp_correlator_surface_complex()`, `set_threads()` and the surface
    sink stay served ([design §18](docs/design/correlator-python.md)).
