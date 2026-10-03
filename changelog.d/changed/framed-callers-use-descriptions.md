- **Framed examples, benches and tests describe their frame.** The burst
    callers that passed `sync=` to a source now pass `frame=` (a `FrameDesc`
    with `add_data`), the form the flat fields are being retired for
    ([#1617](https://github.com/doppler-dsp/doppler/issues/1617)); each
    composes the same samples as before.
