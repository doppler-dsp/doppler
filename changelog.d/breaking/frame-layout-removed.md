- **`Frame.layout()` and `FrameLayout` are removed; read a field by name.**
    `f.field_off(f.field_index("payload"))` replaces
    `f.layout().payload_off`. A `Frame` now holds only the fields it was
    given, so `Frame(sync=..., payload=..., crc="crc16")` has 3 fields, not
    4 -- an index that assumed an absent preamble shifts by one (#853).
