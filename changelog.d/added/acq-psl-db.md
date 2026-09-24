- **`psl_db` on `BurstAcquisition`, `BurstCapture` and
    `PersistentBurstCapture`**: the preamble's peak sidelobe level, from its
    own periodic autocorrelation outside the mainlobe. A burst that clears
    the threshold by more than `-psl_db` also lists its own sidelobe as a
    second peak. It reads −29.8 dB for a 31-chip m-sequence, about −15 dB
    for a random 96-symbol QPSK, and `-inf` for Zadoff-Chu. In C it is
    `acq_psl_db()`
    ([#1470](https://github.com/doppler-dsp/doppler/issues/1470)).
