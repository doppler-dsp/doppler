- **`ccsds_tm_frame_spec_t` and `dp_ccsds_tm_frame_desc_of()` are deleted.**
    They were a second derivation of which stage covers which fields, for
    literal fields only. Build a description with `dp_wfm_frame_add_field` /
    `add_derived` / `add_stage`, or `dp_ccsds_tm_frame_describe` for a
    CADU (#853).
