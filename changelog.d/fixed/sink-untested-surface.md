- **`StreamSink`'s gain/peak/clip surface is tested now** — six of its ten
    public entry points (`available`, `send_eos`, `set_gain`, `peak`,
    `clip_fraction`, `track_clipping`) were mentioned in no C test anywhere,
    and the file ended in `return 0`, so a `DP_CHECK` could not have failed
    it. Each new section is proven by sabotage. Certifying it produced
    [#1117](https://github.com/doppler-dsp/doppler/issues/1117) (three
    private float→int copies truncate, costing 6.0 dB) and
    [#1118](https://github.com/doppler-dsp/doppler/issues/1118).
