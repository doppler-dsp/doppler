- **`Frame` and `FrameDesc` take bits, and nothing else.** The 38 constructor
    arguments are now three omittable bit arrays and `crc`:
    `Frame(sync=..., payload=field_bits("pn:1024:10"), crc="crc16")`. A
    generated field is its `field_bits` text; a repeated preamble is repeated
    in its bits. `add_field(name, bits)` replaces the 15-argument form, and
    `add_hex` / `add_value` are gone (`field_bits("0x1ACF")`). A field a stage
    fills is `add_derived(name, bits)`; either form of `add_stage` wires it.
    An element that is not 0 or 1 is refused rather than masked. The old
    spellings are refused, not aliased (#853).
