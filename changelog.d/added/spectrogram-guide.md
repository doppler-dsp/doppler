- **A guide and a C example for the streaming spectrogram.**
    `docs/guide/spectrogram.md` explains rows, sizing the output, the
    room rule and flush, around `native/examples/spectrogram_demo.c`: a
    hopping tone in chunks of 1/37/700/5 samples, built into a waterfall two
    ways that agree bit for bit. Every public `dp_spectrogram_*` function now
    has a C `@code` example, compiled and run by `make test-snippets`.
