- **The CCSDS coverage table moved to `ccsds_tm`, where the standard is.**
    `wfm/wfm_frame.h` knows what a field and a stage are and deliberately not
    which covers which — its own header says it "knows nothing about CCSDS" —
    so `ccsds_tm_frame_desc_of()` is where 131.0-B-6 10.3.4's rule now lives,
    next to the ASM bits and the RS parity size it needs. The generator's
    bridge is an adapter over it.
