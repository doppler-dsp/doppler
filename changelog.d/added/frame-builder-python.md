- **A frame can be described by name from Python**: `add_hex("asm",   "1ACFFC1D")`, `add_value("sync", 0xABC, 12)`, `add_derived("crc", 16)`,
    `add_stage_over(0, "payload", "crc")`, plus `field_index` and
    `name_field`. jm generates the binding; the hex and value expansions are
    `cvt`'s `hex_to_bin` / `int_to_bin` rather than a second parser, so a
    marker cannot be expanded two ways. This is where the 38-argument
    constructor stops being the only way to describe a frame.
