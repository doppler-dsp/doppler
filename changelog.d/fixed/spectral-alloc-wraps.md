- **Three spectral scratch buffers no longer wrap their size.**
    `find_peaks_f32` and `noise_floor_db` sized a scratch copy by an
    unchecked `len * sizeof` product, so a length past `SIZE_MAX / 8` (or
    `/ 4`) got a short buffer and wrote or sorted past it; `ToneMeasure`
    did the same with `(n_harmonics + 1) * sizeof (size_t)` and wrote each
    kept harmonic past its scratch. They now size through `dp_mallocn`
    and refuse: `find_peaks_f32` returns 0 peaks, `noise_floor_db` returns
    0.0, and `ToneMeasure` measures as if it tracked no harmonics.
