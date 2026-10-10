- **`dp_det_verify_count` saturates at `INT_MAX` instead of overflowing.**
    A per-look probability with `1 - p_look < -ln(p_target) / INT_MAX`
    (3.2e-9 at a 1e-3 budget) needs more looks than an `int` holds, and the cast to `int` was undefined (`INT_MIN` on
    x86, reached from `Dll.configure_lock(0.9999999999999999, 1)`). It
    now returns `INT_MAX`, as a certain look already did (#2112, item 2).
