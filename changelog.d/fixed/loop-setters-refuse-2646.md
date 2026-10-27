- **`RateSync.bn` and the Costas / CarrierMpsk `bn_fll` refuse a bad
    value.** `dp_ratesync_configure`, `_set_bn` and `_loop_configure` now
    return `int` and refuse through `dp_loop_filter_params_ok`, so
    `rs.bn = float("nan")` raises `ValueError` instead of being ignored;
    `set_bn_fll` and both `create()`s refuse a negative, NaN or infinite FLL
    bandwidth through the new `dp_loop_filter_fll_bn_ok` (#2646).
