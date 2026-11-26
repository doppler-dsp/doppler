- **`settle_time`: when a loop's output has settled, read from the object.**
    `RateSync`, `MpskReceiver` (and its BPSK and real-input flavors) and
    `AsyncDsssReceiver` (and `Ccsds415Receiver`) report the symbols they emit
    before their loops have settled, from the bandwidths and damping in use:
    2.59 / bn at damping 0.707, the timing and carrier loops' figures added.
    A lock flag declares while the loops are still moving; this is where a
    steady-state measurement may start. In C,
    `dp_loop_filter_settle_time()` is the primitive
    ([loop filter](docs/design/loop-filter.md)).
