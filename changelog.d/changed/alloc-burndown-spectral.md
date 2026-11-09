- **The spectral and measurement cores size their buffers through the
    checked helpers** (`dp_mallocn`, `dp_reallocn`, `dp_xmallocn`,
    `dp_xreallocn`): PSD, Spectrogram, SpecAn, the three `measure`
    analyzers, `find_peaks_f32`, `noise_floor_db`, the event log and the
    telemetry capture. A wrapping byte count is refused (or aborts, on
    trusted paths); 20 sites in 10 files leave
    `scripts/.alloc-product-ratchet`. PSD's size refusal now stops at the
    largest object malloc can serve, not the largest `size_t`: `n * pad`
    of 2^60 is refused up front instead of asking malloc for 2^63 bytes.
