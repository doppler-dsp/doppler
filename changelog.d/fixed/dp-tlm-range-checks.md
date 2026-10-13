- **`Telemetry.emit`, `.probe` and `.set_decim` raise `OverflowError` for an
    out-of-range integer.** `emit(2**32, 1.0)` used to record into probe 0 and
    `probe("x", decim=2**32 + 3)` registered decimation 3; the hand-owned
    binding never got jm's range test, so each wrapped into a plausible value.
    Now they refuse, as the jm-generated bindings do (#2270, #2271, #2272).
