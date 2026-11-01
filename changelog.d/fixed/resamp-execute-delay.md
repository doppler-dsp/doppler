- **`dp_resamp_get_delay()` named the wrong delay for a decimating
    `dp_resamp_execute()`.** Below a rate of one, execute runs the
    transposed structure, whose delay is (centre - 1) / rate input samples
    (9.34 at rate 0.91, 16.98 at 0.50), not the 10.5 the ctrl and push
    forms have at every rate. `dp_resamp_get_execute_delay()` gives the
    execute path's own; `dp_resamp_get_delay()` keeps naming the ctrl and
    push forms'.
