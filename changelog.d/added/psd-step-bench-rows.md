- **Bench rows that name where a PSD frame's time goes** (#2094's first
    step). `psd::accumulate_frame[nfft=N]` times one frame of PSD's own
    accumulate in the same interleaved loop as `fft`, `frame_power` and
    `frame_db`. `acc_trace::fold[<mode>,nfft=N]` times the fold on its own,
    all four modes at the same five sizes. Together they give the baseline
    the SIMD fold and the fused passes are measured against.
