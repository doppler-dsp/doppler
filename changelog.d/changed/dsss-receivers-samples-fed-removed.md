- **`samples_fed` is removed from the `DsssReceiver` and
    `AsyncDsssReceiver` C state structs.** It was a count kept in parallel
    with the embedded engine's, so it went wrong after
    `configure_search_raw()` or `set_state()`. A C caller that read it reads
    `dp_acq_position (state->acq)` instead: the engine's own position, in
    the timebase its hits are counted in (#2042).
