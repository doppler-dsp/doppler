- **The Spectrogram is certified** (#1894 slice 3a, #1941's A4): its
    header's 27 claims inventoried against their C pins, measured at scale
    by `native/validation/spectrogram_certify.c` against an oracle built
    without the object, and asserted as limits in
    `src/doppler/tests/validation/spectrogram/results.md`. The inventory
    pinned two claims the C test held only by luck: `get_state` writing
    every byte (heap contents), and the refusal of another hop's blob
    before its first row (masked by the counter check).
