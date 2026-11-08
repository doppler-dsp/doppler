- **The acquisition engine is the correlator's two search flavors** (#2752
    step 3.4b, #2930). Saved blobs from `Acquisition`, `BurstAcquisition`
    and everything composing them are refused (`ACQR` retires for `CORL`;
    BCAP, DSRX, ADRX, the pool and DBRX bump). In C, `dp_acq_*` is
    `dp_correlator_*`, `acq_result_t` is `dp_correlator_hit_t` (`row`, `col`
    for `doppler_bin`, `code_phase`), and `doppler/acq/` is gone. Python's
    signatures and tuples are unchanged. libdoppler.so no longer exports
    `dp_async_dsss_receiver_set_pooled`, `_set_search_only` or
    `_take_picks`: never public API (a private header's), and the static
    library is unchanged.
