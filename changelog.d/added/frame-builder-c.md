- **A frame description can be built by name**: `wfm_frame_add_field`,
    `wfm_frame_add_derived` and `wfm_frame_add_stage(kind, "payload", "crc")`
    append in wire order instead of filling indices positionally. `add_stage`
    wires a derived field's producer itself, which applies the invariant the
    layout already enforces rather than letting a caller state it a second,
    different way. The bounds rise 8 → 16 fields and 6 → 8 stages: the deepest
    description doppler builds today is six fields, so the old ceiling left
    room for two, and the descriptor is still a 2 KB stack local.
