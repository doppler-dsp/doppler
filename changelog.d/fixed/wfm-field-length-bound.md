- **A huge Field length is refused, not an abort.** `pn:4000000000:5`
    killed `wfmgen` with SIGABRT. A Field's `LEN * REPS` is now bounded at
    `WFM_FIELD_MAX_BITS` (261120), derived in the frame design's limits
    table, and the refusal names it (#1622).
