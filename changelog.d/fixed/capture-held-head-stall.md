- **A held detection no longer stalls the bursts queued behind it**
    ([#1534](https://github.com/doppler-dsp/doppler/issues/1534)). With
    bursts longer than `refine_span`, a whole-capture push dropped 52,596
    samples and one burst of four, where 1000-sample blocks lost nothing.
    `BurstCapture` now emits the first burst that is not held.
