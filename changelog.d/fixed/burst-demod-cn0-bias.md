- **`BurstDemod` reports a channel C/N0 that a link budget can use.**
    Measured on the decoded symbols and lifted by the symbol rate, it is
    within 0.3 dB of the truth from 5 to 30 dB Es/N0, where its predecessor
    was 34 dB high and compressed. It is also flat across the receiver's own
    timing: acquisition resolves a burst start to one SAMPLE, so the
    demodulator now removes the whole-sample error, measures the rest into
    the new `est_timing_chips`, and takes that loss back out — a realized
    SNR fell 4.8 dB at half a chip
    ([#1304](https://github.com/doppler-dsp/doppler/issues/1304)).
