- **A standalone sweeping chirp must declare `span=`**
    ([#1115](https://github.com/doppler-dsp/doppler/issues/1115)).
    `chirp(f_start=…, f_end=…).steps(N)` used to take its sweep length from
    `N`; it now raises on first generation, because the length of a read is not
    a property of the waveform. Write `chirp(f_start=…, f_end=…, span=N)`. A
    chirp inside a `Segment`, the CLI, and a flat chirp (`f_end == freq`) are
    unaffected. At the C level, `wfm_synth_steps()` no longer pins the span:
    call `wfm_synth_set_chirp_span()` before generating, or the engine holds
    the start frequency.
