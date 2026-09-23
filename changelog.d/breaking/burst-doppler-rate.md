- **A Doppler rate caps the burst engine's coherent depth** (#1482).
    `BurstAcquisition` takes `doppler_rate` (Hz/s, default 0 = no bound) and
    sizes at most `f_epoch / sqrt(2 * doppler_rate)` repetitions. Under a
    1.5 MHz/s ramp, an engine not told the rate predicted Pd 0.92 and
    delivered 0.65. Told the rate, it predicts the 0.33 it delivers and
    reports under-powered. **Breaking for C:** `acq_create_burst()`,
    `acq_create_burst_template()`, `burst_acq_create()` and
    `burst_acq_create_template()` gain a trailing `double doppler_rate`;
    append `0.0` to keep today's depth. `BurstCapture` follows in #1490.
