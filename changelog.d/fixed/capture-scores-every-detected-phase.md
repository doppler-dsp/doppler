- **`BurstCapture` delivers `pd_burst` at the default `pd=0.9` on
    Zadoff-Chu** ([#1519](https://github.com/doppler-dsp/doppler/issues/1519)).
    Refine scores every code phase the burst's detections carried, and scores
    edge Doppler cells at both aliases. Before, at the edge of the native
    span, it lost a phase 51 samples along the delay-Doppler ridge, 0.026 of
    Pd.
