- **`acq`'s docs called its per-cell Pfa "Bonferroni"; the code computes
    Šidák**, `1 − (1 − pfa)^(1/N)`. The name now matches the formula
    (`pfa_cell`, the thresholds, and the BER/rx harnesses that inherit it).
