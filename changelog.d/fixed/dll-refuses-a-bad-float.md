- **The DSSS receivers, `Dll`, `Despreader` and the shared loop-filter loops
    refuse a bad float or out-of-domain argument instead of aborting or
    running NaN gains.** A refused retune, a forged blob or a seed past fs/2
    changes nothing. Chain sizes past allocation still abort (#2112).
    `RateSync` `configure`/`set_bn` stay void (#2112).
- **`Dll.set_symbol_period` refuses a period above 2^20 partials and a
    negative period.** A negative period used to be taken as "no symbol
    timing", the same as zero; only exactly zero means that now. `Dll`
    `spacing` at or below zero, or at or above half the code length, is
    refused at create.
- **Not yet guarded:** `burst_despreader` (`burst_despreader_core.c:78-79,418,428`)
    is a sixth embedder of the loop filter and still takes a non-finite or
    negative `bn`. Tracked on #2112 and #2139.
