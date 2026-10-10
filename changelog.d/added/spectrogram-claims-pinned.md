- **The Spectrogram's header claims are pinned in C**: the carry bound, the
    room rule judged as maximal, flush's truth on the hop grid, beta ignored
    outside Kaiser, the -200 dB floor, the state blob's size and contents,
    every set_state refusal leaving the object as it was, a different window
    restoring as that window or beta, and zero latency in samples. Tests, plus
    the header's prose on the state blob and its c-api page; step A4 of #1941
    (#1894 slice 3a).
