- **What the Spectrogram costs, measured** (#1894's U1–U4). On one pinned
    core of an AMD Ryzen AI 9 465, it sustains about 36 MSa/s at `nfft` 1024
    and `hop` 256, about 141,000 rows per second. The carry's copy is at
    most 3.3% of that, so it gets no bypass. The dB conversion is 70–83% of
    every row; what to do about it is #2074. Record and data:
    `spectrogram-measurements.md` §5.6–5.9.
