- **The FFT, DDC, spectral and detection pages say what the code does.**
    Among the fixes: the fine NCO for `Ddcr` is `-(2f + 0.5)`, not
    `2f + 0.5`. A wrong-dtype `out=` is refused, not cast. `execute_cf32` runs
    in float on PFFFT (1.6× faster than cf64 at N = 1024), not in double.
    `CorrDetector`'s threshold is a linear ratio, not dB. Not every detection
    helper fails closed on a bad probability: `det_snr` hangs
    ([#1513](https://github.com/doppler-dsp/doppler/issues/1513)).
