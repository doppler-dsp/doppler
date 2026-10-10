- **The Spectrogram's dB floor and end of stream, characterized** (#1894's
    U5 and U6). A bin reads no lower than −200 dB, so an all-zero frame and a
    signal under the floor give the same row, and noise reaches the floor
    about `10·log10(nfft)` sooner than a tone does. `flush` stays explicit.
    The header and guide now state the floor; the numbers come from the new
    `validate_spectrogram_certify` harness and are recorded in
    `spectrogram-measurements.md` §5.4–5.5.
