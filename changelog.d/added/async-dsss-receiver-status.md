- **`AsyncDsssReceiver.status()` -- one record, by value.** State, where the
    emitter is now (live Doppler, chip phase, code rate, C/N0), both lock
    flags with the symbol-lock metric and threshold, both residual carrier
    errors, and the two clocks in input samples -- what a pool holder reads
    per data-free window to key the searcher's exclusion zones on the live
    estimate. The one-at-a-time properties are the same fields' other face;
    §11.3 of
    [the design page](https://doppler-dsp.github.io/doppler/design/async-dsss-receiver/).
