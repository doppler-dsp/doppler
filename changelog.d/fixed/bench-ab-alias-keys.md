- **`make bench-ab ALIAS=` names bench binaries, and a bad alias stops after
    one pass.** Rows are keyed by `fullname`, whose C family is the bench
    binary (`ALIAS=bench_corr2d_core=bench_correlator_core`). The examples
    named the published family instead, and an alias pairing nothing was
    found only after all K passes (#2752 step 3.2).
