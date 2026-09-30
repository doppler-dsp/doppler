- **A `Synth`/`Segment` bit field reads a `str` as a Field.** `payload=`,
    `sync=`, `acq_code=` and `data_code=` now parse text with
    `dp_wfm_field_bits()`, the one Field parser (just-makeit#1709), so
    `Segment(payload="pn:31:5")` and `"0101*2"` work as they do in `wfmgen`
    and a scene. `""` and a bare `"0x"` are now refused with the parser's
    reason instead of reading as an empty pattern.
