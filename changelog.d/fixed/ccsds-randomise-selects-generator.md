- **`ccsds_tm_frame_cfg_t.randomise` selects the generator.** `2` used to
    build 10.4.1's sequence silently, while the same value on a description's
    randomise stage meant 10.4.2's legacy one. Encode, decode and describe
    now read 0 as none, 1 as 10.4.1 and 2 as 10.4.2, through one mapping
    (`dp_ccsds_tm_rand_select`). Any other value is refused (#1609).
- **A randomise stage naming no generator is refused.** A `--frame` file or
    scene frame whose randomise stage had depth 3 silently used 10.4.1. It
    now fails to assemble, and wfmgen exits 2 saying why (#1609).
