- **just-makeit pin 0.125.0 → 0.126.1.** A count method can report a
    status (`status_out`, jm#2584), so `Correlator.bank_power_max` and
    `bank_detect` raise `MemoryError` for an allocation failure;
    `make drift-check` fails on a hand-owned fragment missing a guard jm
    emits (jm#2581); and a header `@param` documents its argument by name,
    so 40-odd "Input." placeholders now carry their real text (jm#2223).
