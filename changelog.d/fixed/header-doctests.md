- **Every `>>>` example in a C header now runs, stub or no stub.** jm
    carries a header example into a `.pyi` only for some surfaces (not a
    property getter, a `destroy`, or a C function with no binding), so 94 of
    635 ran nowhere (#2595). Three of them were wrong: an under-powered
    `BurstAcquisition` in `dp_acq_psl_db`, an `I16Buffer.dropped` count its own
    header prose contradicted, and a `dp_wfm_synth_set_symbols` call one symbol
    long.
