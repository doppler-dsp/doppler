- **Bench rows that name where a PSD frame's time goes, and what power rows
    cost** (#2094's first step). `psd::accumulate_frame[nfft=N]` times one
    frame of PSD's own accumulate beside `fft`, `frame_power` and
    `frame_db`. `acc_trace::fold[<mode>,nfft=N]` times the fold on its own.
    `spectrogram::push[nfft=N,hop=N/4,mode=power]` at 256, 1024 and 65536
    times the default power rows beside the dB ones (U4 for power, moved
    from #1968). Together they are the baseline #2094 is measured against.
