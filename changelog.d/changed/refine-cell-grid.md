- **`BurstCapture` refine is ~20x cheaper per burst**
    ([#1538](https://github.com/doppler-dsp/doppler/issues/1538)): 0.71 →
    0.036 ms at the benchmark geometry (published native numbers), and the
    DSSS burst receiver's per-burst cost falls 0.84 → 0.16 ms, with every
    §2.8 trial choosing the same repetition. New `acq_cell_corr_grid()`
    scores many Doppler cells over consecutive epochs in one pass.
