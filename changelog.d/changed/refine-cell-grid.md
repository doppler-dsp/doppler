- **`BurstCapture` refine is ~14x cheaper per burst**
    ([#1538](https://github.com/doppler-dsp/doppler/issues/1538)): 0.71 →
    0.05 ms at the benchmark geometry on the fast cores, with every §2.8
    trial choosing the same repetition. New `acq_cell_corr_grid()` scores
    many Doppler cells over consecutive epochs in one pass.
