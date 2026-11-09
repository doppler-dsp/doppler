- **The composers' `configure_search_raw()` return the library's codes**:
    `DP_ERR_INVALID` (-4) for a bad grid, where they returned -1, and
    `DP_ERR_MEMORY` (-6) when rebuilding the engine cannot allocate. This
    applies to `dp_burst_capture_`, `dp_dsss_receiver_`,
    `dp_async_dsss_receiver_` and `dp_dsss_burst_receiver_configure_search_raw`.
    A C caller testing `== -1` stops catching the refusal, so test `!= 0`.
    In Python, `BurstCapture` raises `MemoryError` for the allocation
    failure; the three receivers raise `ValueError`, whose `(rc=…)` reads
    -4 or -6 where it read -1 (#2752).
