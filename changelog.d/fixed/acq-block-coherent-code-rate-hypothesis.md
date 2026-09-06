- **The block-coherent searcher lost its depth under a dilated chip
    clock.** At SPEC's 20 ppm the code drifts 3 chips across a D = 154
    block, the coherent sum smeared 13 dB and the depth detected nothing at
    34 dB-Hz. Every window tile now carries the code-rate hypothesis its own
    frequency implies once the engine is told the carrier
    (`Acquisition.set_carrier_freq_hz`), which also moves the hand-off's
    half-dwell advance onto the engine; the block reads as a still one
    (design §12.12).
    [#1256](https://github.com/doppler-dsp/doppler/issues/1256).
