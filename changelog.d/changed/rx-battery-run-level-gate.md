- **`rx_battery --check` still does not gate a receiver that refuses EVERY
    point**, and the file now says so where the gate would go. A per-point
    refusal is a result and stays uncounted — a `qpsk`/`psk8` frame-geometry
    refusal is the harness working — but nine of them is a receiver that does
    not work, which is precisely how the `mf_in` pin exited 0 while measuring
    nothing. Closing it needs a run-level rather than a per-point gate, so it
    is being added to this same loop as `dp_rx_witness_t` by doppler#794
    rather than raced here.

### Removed
