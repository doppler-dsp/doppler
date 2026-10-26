- **The design pages describe the code as it is.** Shipped work no longer
    reads as planned (the RateConverter cascade, the stateful channel,
    `flush()` returning a row, #1620/#1789/#853/#977), the correction is
    Šidák wherever `dp_det_pfa_cell` computes it, the state-serialization
    examples are real code (LO's u32 phase, the DDC composition,
    `DP_DEFINE_RUN`'s leading `STATE_T *s`), the MPSK receiver's fifteen
    probes and fifteen parameters are named, and `mpsk.md` §9.5 reconciles
    its 2.03× differential penalty with the report's 2.17× (≈2.0×; #2603)
    (#2583).
