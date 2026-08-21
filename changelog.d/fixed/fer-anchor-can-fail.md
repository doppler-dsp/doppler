- **The FER anchor in the receiver battery can fail again.** It could not,
    at any operating point in the standard battery: the sabotage it exists to
    catch — corrupting every other frame's CRC — left `rx_battery --check`
    reporting **OK**. Now it fails at four points, and the reported slack at
    the anchor drops from **1.64x to 1.32x**.

    The cause was not the tolerance and not the interval width. Corrupting
    every other frame adds `0.5 x (1 - FER)`, so the *relative* change shrinks
    as the baseline rises — and `RX_FRAME_CONT` protected 1040 bits, at which
    the battery's SER=1e-3 anchor already fails two thirds of frames on noise
    alone. A gross fault therefore moved the measurement only 0.68 → 0.84, a
    factor of 1.23, under the anchor's floor of about 1.7. **A high baseline
    FER hides faults rather than exposing them.**

    The payload is now 304 bits (320 protected), where the baseline is 0.30
    and the same sabotage reads 0.65 — a factor of 2.18. `RX_FRAME_NONE` and
    `RX_FRAME_GOLD` move with it, keeping the pairings that make them
    comparable (same payload verbatim, same geometry); `test_dp_frame`
    asserts both and its bit-count table moves too. Shortening the frame
    rather than raising Es/N0 keeps **one** Es/N0 across all four battery
    metrics, which is what makes them comparable at all.

    A second point came back for free: `rate_odd` previously **REFUSED** its
    framed half — "no burst aligned, the marker never detected" — and now
    measures 111 frames. `qpsk` and `psk8` still refuse (their frame bits do
    not divide into whole symbols), unchanged.
