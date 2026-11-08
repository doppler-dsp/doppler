- **`dp_correlator_set_surface_sink()` returns `int`** and refuses a
    flavor that decides no dwell (a map or a detector) with
    `DP_ERR_INVALID`, where it used to write through a NULL search record.
    A caller that ignored the old `void` compiles unchanged (#2752).
