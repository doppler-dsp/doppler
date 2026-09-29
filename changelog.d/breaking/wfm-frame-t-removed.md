- **`wfm_frame_t` and its four-field API are deleted; a frame is a
    description.** `wfm_frame_layout_t`, `dp_wfm_frame_describe`,
    `dp_wfm_frame_nbits` / `_layout` / `_bits` / `_crc_ok`,
    `dp_wfm_frame_dsss_nchips` / `_chips` and `dp_wfm_synth_set_dsss` go.
    `dp_wfm_frame_fixed` describes the common `[preamble x reps | sync |   payload | crc]` frame with fields named `"preamble"`, `"sync"`,
    `"payload"` and `"crc"`; assemble it with `dp_wfm_frame_assemble`, check
    it with `dp_wfm_frame_desc_crc_ok`, spread it with
    `dp_wfm_dsss_desc_chips` and install the chips with
    `dp_wfm_synth_set_dsss_chips`. Every replacement is byte-identical (#853).
