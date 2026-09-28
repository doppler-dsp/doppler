- **just-makeit pin 0.92.2 → 0.92.3.** It carries just-makeit#1704's fix:
    `check_return` on a self-sizing module function raises on a zero count,
    which is what makes `field_bits()` raise on malformed text rather than
    return an empty array.
