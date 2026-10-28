- **Failures that used to pass silently are now reported or cannot happen.**
    `BurstDespreader.set_acq` returns a status and raises `MemoryError`
    instead of quietly disabling the preamble; `Acquisition.set_telemetry`
    registers all ten probes or none; `Ddcr.execute`'s scratch growth aborts
    on OOM like every library allocation, so its 0 only means "no output";
    `design_lowpass` needs no scratch; `dp_wfm_synth_destroy(NULL)` and the
    composer's execute/segments entry points are NULL-safe;
    `dp_thread_sleep_us` resumes after a signal; the Windows pacing nap
    falls back to `Sleep()` when its timer will not arm, not only when it
    cannot be created; and a frame's bits per symbol are spelled out (#2647).
