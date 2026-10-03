- **The spectrum analyzer's skirt no longer depends on the RBW.** An RBW of
    `fs_out/2^k` got a rectangular window and −13 dB sidelobes; the specan demo
    sat exactly there. Span, transform length and Kaiser beta now follow
    [the design](https://doppler-dsp.github.io/doppler/design/specan/): beta ≈ 12
    or more, about −90 dB, at every RBW.
