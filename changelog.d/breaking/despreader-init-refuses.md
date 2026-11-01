- **`dp_despreader_init()` returns `int` and refuses what `create` refuses.**
    The by-value door now checks the code loop's and carrier loop's domains
    (`dp_dll_params_ok()`, `dp_loop_filter_params_ok()`,
    `dp_loop_filter_fll_bn_ok()`) and returns `DP_ERR_INVALID`, or
    `DP_ERR_MEMORY` when its bit-sync histogram cannot be allocated, with
    nothing written either way. It used to build a loop with NaN gains. C
    callers that ignored it still compile; check it.
