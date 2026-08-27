- **`coding.Deinterleaver` — the receive face**, a view over the same core with
    `interleave` deliberately absent. Someone working the rx side reaches for
    this name; sharing one core means the geometry both ends must agree on has
    exactly one definition. Plus `docs/design/interleaving.md` and a
    self-validating example.
    [#1031](https://github.com/doppler-dsp/doppler/issues/1031)
