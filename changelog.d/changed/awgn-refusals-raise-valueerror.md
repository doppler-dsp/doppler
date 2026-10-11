- **`AWGN` refuses a bad argument with a `ValueError`, not a `MemoryError`,
    and the C setter reports its refusal.** A `NaN`, infinite or negative
    `amplitude` given to the constructor used to report running out of memory.
    Code catching `MemoryError` around `AWGN(...)` must catch `ValueError` now.
    `dp_awgn_set_amplitude` changed from `void` to `int`: it returns `DP_OK`,
    or `DP_ERR_INVALID` for a refused value, and every caller checks it (#2084).
