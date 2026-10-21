- **`AsyncDsssReceiver` takes a ranked, filtered peak, so searchers on one
    stream split the signals.** `set_rank(r)` and `set_exclusions(phases)`
    skip stronger or held signals; a peak a stronger signal's own
    Doppler-offset sidelobes could explain is not a signal, which sets the
    pool's near-far limit. While refining, `status().chip_phase` names the
    seed's signal instead of 0. Rank 0 with no exclusions picks as before;
    the state blob is v13 (`async-dsss-parallel.md` §3, #2525).
