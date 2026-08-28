- **`--interleave` refused valid CCSDS arrangements.** Its guard measured
    payload + CRC and omitted the outer code's check symbols, so it validated
    a different span from the one the stage permutes: 223 octets under
    RS(255,223) at depth 5, unit 8 was rejected though 2040 bits divides by
    40 exactly. Both failures were pinned in the flag-matrix golden as
    expected `exit: 2`.
