- **The spectral and measurement cores size their buffers through the
    checked helpers** (`dp_mallocn`, `dp_xmallocn`, `dp_xreallocn`,
    `dp_size_mul`): PSD, Spectrogram, SpecAn, the three `measure`
    analyzers, `find_peaks_f32`, `noise_floor_db`, the event log and the
    telemetry capture. A byte count that would wrap is now refused (or
    aborts, on the trusted paths) instead of buying a short buffer;
    nothing else changes. Their 20 entries leave
    `scripts/.alloc-product-ratchet`.
