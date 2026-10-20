- **The continuous search's coherent depth is a fast FFT length.** `D` is
    now rounded down to a 7-smooth length (`dp_fft_smooth_floor()`, new in
    `fft_core.h`), because the slow-time transform's cost follows its
    factors: at the async-DSSS design point D = 94 (2·47) cost 1.46× D = 90.
    The rate bound's 154 now runs as 150 and 61 as 60, the bench rows renamed
    to match: 1.15× faster at `op5M_U50k_t1` and 1.95× at `op2M_U50k_t1`,
    where 61 is prime (#1247).
