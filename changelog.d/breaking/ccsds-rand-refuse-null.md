- **`dp_ccsds_tm_randomise_with`, `_rand_seq_with` and `_rand_init` refuse
    `NULL`.** They return `int` now, `DP_ERR_INVALID` for `NULL` with nothing
    written, where `NULL` used to mean the default. `NULL` is
    `dp_ccsds_tm_rand_select`'s refusal, so it must stay one downstream. For
    the default, call `dp_ccsds_tm_randomise` / `_rand_seq` or pass
    `&dp_CCSDS_TM_RAND` (#1633).
