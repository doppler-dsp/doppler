- **The Spectrogram is certified** (#1894 slice 3a, #1941's A4): its
    header's 24 claims inventoried against their C pins, measured at scale
    by `native/validation/spectrogram_certify.c` against an oracle built
    without the object, and asserted as limits in
    `src/doppler/tests/validation/spectrogram/results.md`. The inventory
    caught one claim pinned only by heap contents (`get_state` writes every
    byte), now pinned by two fills.
