- **Corr2D and DelayDoppler batches in C** (#2683):
    `dp_corr2d_execute_all`, `_coherent`, `_power_sum`, `_power_max` and
    `_peaks` take many frames in one call and return every map or one
    reduction of them, on one fold loop that C callers also reach with
    their own reducer (`dp_corr2d_execute_fold`). Map h is what a fresh
    dwell-1 object returns for frame h, and a batch never touches the dwell.
    Python bindings follow.
