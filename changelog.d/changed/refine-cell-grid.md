- **`BurstCapture` refine is ~8x cheaper per burst**
    ([#1538](https://github.com/doppler-dsp/doppler/issues/1538)): 765 → 88 µs
    at the benchmark geometry, with every §2.8 trial choosing the same
    repetition. New `acq_cell_corr_grid()` scores many Doppler cells over
    consecutive epochs, despreading each sample once.
