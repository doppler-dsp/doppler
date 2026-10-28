- **`BurstDespreader`'s `set_acq` preamble now sets the payload's absolute
    phase, carrier seeded or not.** An unseeded offset the preamble had not
    pulled in left the payload's Costas detector to lock at π, and every
    bit came out complemented: 24 of 64 phase × offset × seed cells at five
    512-chip reps. At the handover the NCO now turns to the last preamble
    prompt's phase, so soft symbols and statistics are corrected too.
    Design and the cost of differential encoding instead:
    `docs/design/mpsk.md` §9.5 (#2664).
