- **At the floor from a 50 kHz seed the async-DSSS receiver's refine gave
    up, and the carrier never locked.** The searcher's hit reports the code
    phase at the middle of its non-coherent dwell, and under 20 ppm that is
    0.9 chip behind the code by the time the seed is applied, past the
    refine Dll's pull-in. `acq_build_handoff()` takes the carrier and
    advances the phase by the drift over half the dwell; the floor now
    settles 10 of 10 (design §12.11).
    [#1254](https://github.com/doppler-dsp/doppler/issues/1254).
