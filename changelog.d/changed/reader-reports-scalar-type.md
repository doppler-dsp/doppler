- **`Reader.sample_type` now names the mode it found.** A real float capture
    reported `"cf32"` alongside `mode == "scalar"` — two contradictory answers.
    It reports `"f32"`, which is also exactly what the constructor accepts as a
    hint, so what you pass for a headerless file is what you get back.
    [#1032](https://github.com/doppler-dsp/doppler/issues/1032)
