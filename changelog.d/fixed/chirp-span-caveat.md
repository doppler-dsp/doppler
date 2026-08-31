- **A standalone chirp's sweep depends on how you read it**
    ([#1115](https://github.com/doppler-dsp/doppler/issues/1115)) — `step()`
    never pins the span, so it emits a constant tone at `--freq`. The CLI and
    `Segment`/`Composer` pin up front and are unaffected; the waveform guide
    now warns where a Python caller meets it.
