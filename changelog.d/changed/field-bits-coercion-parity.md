- **A `Source`'s string bit pattern is pinned against `field_bits`.** jm's
    coercion reads binary and hex exactly as the Field grammar does, and a
    test now holds that — and its two gaps: a generated Field or `*REPS` is
    refused (pass `field_bits(text)`), and `""`/`"0x"` read as empty.
    Both close with just-makeit#1709 (#853).
