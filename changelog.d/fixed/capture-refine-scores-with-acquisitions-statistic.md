- **`BurstCapture` names the right repetition more often** (#1502). Refine
    now scores each candidate period with acquisition's own statistic
    (`acq_cell_corr`, new in `acq_core.h`) at the settled code phase, mixed
    within every period over the Doppler cells of the engine's bin, instead
    of a slow-time transform across per-period correlations. On Zadoff-Chu
    127 × 8 the wrong-repetition errors roughly halve, and the capture's loss
    behind the engine falls from 0.011–0.051 to 0.006–0.034. It now sits at
    or above `pd_burst` at every depth, and the dwell harness holds it there.
    About 50 µs more per burst.
