- **A `Synth`/`Segment` bit field takes bits, never text.** `data=`,
    `fill=`, `sync=`, `acq_code=` and `data_code=` refuse a
    `str` with `ValueError: a bit field takes bits (a uint8 array); build   them from text with field_bits()`. Before, they read `"0101"` and
    `"0xAA55"` with a grammar of their own and refused `pn:`/`*REPS`. Arrays,
    `bytes` and 0/1 sequences are taken as before. Migrate:
    `Segment(data="0xAA55")` → `Segment(data=field_bits("0xAA55"))`.
