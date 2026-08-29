- **The general frame descriptor now has evidence of its own, not CCSDS's.**
    `wfm_frame_check`, `wfm_frame_ops_t` and `WFM_STAGE_INTERLEAVE` had zero
    mentions in `test_wfm_frame.c` and were exercised only through
    `test_ccsds_tm_frame.c`; `wfm_frame_desc_crc_ok` and the "carries no check
    returns −1, not 1" rule were asserted nowhere in the tree. Six sections
    now pin them over a synthetic two-stage kernel table — CCSDS is one
    configuration of the descriptor, so it cannot also be the thing that
    proves it. Each proven by sabotage; the order and non-identity
    preconditions are what stop two of them passing vacuously.
