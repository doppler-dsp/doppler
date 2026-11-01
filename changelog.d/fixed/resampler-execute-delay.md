- **`Resampler.execute_delay` gives `execute()`'s own delay.** Below a rate
    of one `execute()` runs the decimator, whose group delay is `8.5 / rate`
    input samples (17.0 at 0.5), but `delay` reports the interpolator's 10.5
    at every rate: it is `execute_ctrl()`'s. A caller mapping a position on
    a decimated output back to its input subtracts `execute_delay`. The C
    face is `dp_Resampler_get_execute_delay()`.
