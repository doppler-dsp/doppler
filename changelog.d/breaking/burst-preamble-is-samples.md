- **The burst objects take the preamble as its samples** (#1470).
    `BurstAcquisition(preamble, reps, fs=1.0, …)`, `BurstCapture(preamble,   burst_len, reps, fs=1.0, …)` and `PersistentBurstCapture(path, preamble,   …)` accept any repeated complex preamble: a chirp, Zadoff-Chu, shaped
    PSK. A PN code is one such preamble: `np.repeat(nrz, spc)` after
    `cvt.bin_to_nrz(code, nrz)`, at `fs = chip_rate · spc`. `spc`,
    `chip_rate` and `sf` are gone, and the C constructors change to match.
    The delay model is now the sampled chain's, so code sizing is shallower
    and closer to measured Pd (acq §2.6: 0.690 predicted, 0.740 delivered).
