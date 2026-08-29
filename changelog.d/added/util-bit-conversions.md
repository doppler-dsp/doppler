- **`util` gains the bit conversions a frame field needs:** `int_to_bin` /
    `bin_to_int` for a literal that fits in 64 bits, and `hex_to_bin` /
    `bin_to_hex` for one that does not, or that arrives as text from a CLI
    flag or a JSON record. Bit order is numpy's `bitorder` (`big`/`little`,
    default `big`), deliberately NOT the BLUE writer's `endian` (`le`/`be`,
    the `"EEEI"`/`"IEEE"` field) — those are bit order and byte order, two
    axes that share a word. The two expansions are cross-checked against each
    other on a value both can express, so a marker cannot be expanded two
    ways. A bad hex digit is a refusal, never a silently shortened field.
