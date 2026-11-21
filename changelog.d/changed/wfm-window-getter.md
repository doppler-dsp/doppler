- **`Synth.window` reads the count the source renders with.** An unset window
    reads 2, 4 or 8 at one, two or four times the bandwidth, not
    `2**64 - 1`, through `get_fn = "dp_wfm_source_window"` (jm 0.134.0); an
    `fs` with no oversampling still reads `2**64 - 1`; a source that is not ofdm reads 0. The setter, `to_json`
    and the stored value are unchanged. A `Segment`'s flat `window` still
    reads the unset constant, awaiting just-makeit#2717 ([#3211](https://github.com/doppler-dsp/doppler/issues/3211)).
