- **The `AsyncDsssReceiver`'s loops show their filter terms at their own
    cadence.**
    - `rx.code.steer.*` holds the code loop's error, proportional term,
        integrator, control and rate aid. It reports at each steer of the
        symbol aid, from `dp_dll_set_steer_telemetry()`.
    - `rx.demod.sync.prop` and `.integ` hold the timing loop's two filter
        terms, from `dp_ratesync_loop_set_filter_telemetry()`.
    - `rx.sym.num` and `.den` are the symbol-lock metric's two EMAs.
    - The pull-in validator's `rules.csv` adds each detector's look count,
        its EMA weight and its loop's bandwidth, read from the built receiver.
    - The probes observe and decide nothing. A receiver with them attached
        emits the same bytes and ends in the same state as one without. The
        despreader's and MpskReceiver's own probe sets are unchanged.
