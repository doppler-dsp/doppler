- **`test_resamp_core` failed under GCC 15** because it held
    `execute_ctrl_push`, `execute_ctrl` and `execute` to bit-identical outputs
    (#2697). Under `-ffast-math`, GCC 15 sums each call site's inlined dot
    product in a different order, so the forms differed by one or two float
    ULPs. The checks now allow 8 ULPs. A timing defect moves these outputs by
    about 0.14, and sabotages of each form still fail. The header's promise
    for the push form now says the same: same outputs, same count, same
    ticks, and values that agree to a few ULPs.
