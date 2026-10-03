- **The wfmgen guide says which waveforms have a receiver.** Unspread
    preamble bursts, `chirp` and `symbols` (QAM, APSK, pi/4-QPSK) are
    stimulus only: no demodulator, though `BurstAcquisition` and
    `BurstCapture` can still detect and cut the bursts out
    ([#1620](https://github.com/doppler-dsp/doppler/issues/1620)).
