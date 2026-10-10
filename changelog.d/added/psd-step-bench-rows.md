- **Bench rows that name where a PSD frame's time goes, and what power rows
    cost** (#2094's first step). In `psd`, `accumulate_frame[nfft=N]` and
    `frame_linear[nfft=N]` are timed beside `fft`, `frame_power` and
    `frame_db`, splitting the normalisation from log10 and the floor.
    `acc_trace::fold[<mode>,nfft=N]` is the fold on its own.
    `spectrogram::push[nfft=N,hop=N/4,mode=power]` times the default power
    rows beside their dB twins (U4 for power, moved from #1968). Together
    they are the baseline #2094 is measured against.
