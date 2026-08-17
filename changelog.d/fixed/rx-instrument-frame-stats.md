- **The receiver instrument declared six frame-statistics fields and measured
    none of them.** `native/tests/dp_rx_test.h` carried `frames`,
    `sync_detected`, `crc_passed`, `fer`, `sync_miss` and `prot_bits`,
    documented FER in its own composition table, included
    `frame_meter_core.h` — and never called `frame_meter_create`. The
    instrument built to report goal 4's four metrics *together* reported
    three, and the missing one is the only truth-free metric that sees a
    false lock.

    The machinery already existed in `native/validation/rx_frame_fer.c`, so
    this **moves** it rather than writing a second one: `dp_rx_score_frames()`
    with the per-frame sync CONFIRMATION at ±`DP_RX_SYNC_SPAN` (a tracking
    window, not a re-acquisition), the truth-free CRC check, and the
    one-sided FER anchor asserted on the interval's **lower** limit.
    `rx_frame_fer.c` is now a caller and its copy is gone; it reproduces its
    committed table **bit for bit**, which is how a move is told apart from a
    rewrite. `framed == 0` prints **n/a**, never `0.0`.

    Measured consequence on the battery: the anchor's SER rises from
    1.087e-03 to 1.090e-03 once the periodic marker's sync words stop being
    scored as data (#793) — the only direction it could move, because symbols
    that could not be wrong stopped being counted as symbols that merely
    happened not to be.
