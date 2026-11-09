- **Four spectral buffers no longer wrap their size.** `SpecAn.execute`
    sized its scratch by `x_len * 8`: at 2^61 it freed the scratch and
    kept the dangling pointer, at 2^61 + 1 the DDC wrote past 8 bytes.
    `find_peaks_f32`, `noise_floor_db` and `ToneMeasure` (by
    `n_harmonics + 1` slots) wrote or sorted past a short scratch the same
    way. Each now refuses: `SpecAn.execute` returns 0 and keeps its state,
    `find_peaks_f32` returns 0 peaks, `noise_floor_db` returns 0.0, and
    `dp_tonemeas_create` returns NULL for an `n_harmonics` it cannot hold.
