- **`DsssBurstReceiver.events()` is deterministic to the byte** (#1699).
    Each row ends in a `uint8_t`, and its seven bytes of struct padding were
    copied out of an unzeroed heap buffer, so two receivers fed the same
    input could return different `tobytes()` for equal events. Rows are now
    zeroed before they are filled.
