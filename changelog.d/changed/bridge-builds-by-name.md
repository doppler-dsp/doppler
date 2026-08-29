- **wfmgen's frames are described by name, not through a CCSDS spec struct.**
    `wfm_source_describe_frame` filled `ccsds_tm_frame_spec_t` and asked
    `ccsds_tm` to translate it, which made a standard's vocabulary the only
    vocabulary — a frame doppler had never seen had to be spelled in CCSDS's
    slots or not at all. It now builds through the general by-name builder.
    `ccsds_tm` keeps what was always the right direction: the kernels, and
    the marker's one expansion. Pinned by assembling both ways over nine
    source shapes and comparing the bits.
