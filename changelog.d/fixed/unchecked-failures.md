- **Failures that used to pass silently are now reported or cannot happen.**
    `BurstDespreader.set_acq` returns a status and raises instead of quietly
    disabling the preamble; `Acquisition.set_telemetry` registers all ten
    probes or none; `Ddcr.execute` allocates before it consumes input;
    `design_lowpass` needs no scratch; `dp_wfm_synth_destroy(NULL)` and the
    composer's execute/segments entry points are NULL-safe;
    `dp_thread_sleep_us` resumes after a signal; the Windows pacing nap
    falls back to `Sleep()` when its timer will not arm, not only when it
    cannot be created; and the bits-per-symbol of
    a frame's mapping is spelled out rather than read off the enum (#2647).
