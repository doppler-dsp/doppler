- **`DsssBurstReceiver.llrs()` and `BurstDemod.llrs()` — the soft bits**
    ([#1018](https://github.com/doppler-dsp/doppler/issues/1018)).
    `Re(sym · derot)` IS the log-likelihood ratio up to a scale, and it was
    computed, sliced to one bit and freed on every burst; a hard decision
    costs roughly 2 dB of the coding gain a soft-input decoder exists to
    deliver. One LLR per frame symbol now, in `mpsk_soft_demap`'s convention
    (`L < 0` reproduces `push()`'s bits, asserted in both languages), scaled
    by the burst's own noise estimate so bursts are comparable — measured
    17.9 → 67.2 → 259.2 mean |L| at 6, 12 and 18 dB Es/N0.
