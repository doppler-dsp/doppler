- **The DSSS burst benchmarks run 2.7x faster by choosing the acquisition
    length on the TRANSFORM, not just the code.** `acq` calls
    `fft_create(sf * spc)` verbatim — the code axis is a circular
    correlation, so it cannot pad — and a 127-chip m-sequence at `spc=4`
    gives 508 = 2²·**127**, where pocketfft falls to Bluestein: **9.70 µs
    against 0.75 µs**. 255 chips at `spc=2` gives 510 = 2·3·5·17, smooth
    *and* twice the autocorrelation ratio — better on both axes, not a
    trade. `BurstAcquisition` goes 19.9 → 53.3 MSa/s. Not rounded to 512:
    no binary code of that length has good periodic autocorrelation.
