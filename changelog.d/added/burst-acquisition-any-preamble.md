- **`BurstAcquisition` acquires any repeated complex preamble**, not only a
    PN code. Pass a `complex64` array of one period's samples: `BurstAcquisition(zc, reps=8, fs=1e6)`. A `uint8` code works exactly as before. The first array's
    dtype picks the constructor: a preamble ignores `spc`/`chip_rate`, a code
    ignores the new `fs`. `fs` defaults to 1, normalized units, where Doppler is
    in cycles/sample and `cn0_dbhz` is the per-sample SNR in dB. The
    under-powered `UserWarning` is now declared rather than hand-written, so the
    binding is generated again. Phase 3 of
    [#1470](https://github.com/doppler-dsp/doppler/issues/1470).
