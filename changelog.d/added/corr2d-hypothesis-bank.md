- **A hypothesis bank on `Corr2D` and `DelayDoppler`** (#2683): one frame
    through every carrier offset on a `1/(L nx)` grid times every rate, as if
    each were a hand-corrected copy, with the row transforms shared:
    `bank_power_max` and, C-only, `dp_corr2d_bank_fold`. `bank_detect`
    searches on a coarse grid and refines each candidate on a fine one, which
    holds the fine grid's sensitivity for half the bank (L = 2 refined to
    L = 4: 0.71 ms against 1.11 ms at 64 x 512).
