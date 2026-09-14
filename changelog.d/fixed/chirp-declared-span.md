- **A standalone chirp was three different waveforms from one configuration**
    ([#1115](https://github.com/doppler-dsp/doppler/issues/1115)) — its sweep
    span locked to the length of the first `steps()` call, `step()` never
    locked and emitted a flat tone, and a mid-sweep state resume re-locked to
    whatever the resumed instance read first. `Synth`/`chirp()` now take a
    declared `span=` (samples); a sweeping standalone chirp without one raises
    on first generation instead of guessing. In a `Segment` the span still
    defaults to `num_samples`, and a declared `span` overrides it and survives
    `to_json()`. `wfm_synth_steps()` no longer self-pins: an unpinned engine
    holds the start frequency on both read paths.
