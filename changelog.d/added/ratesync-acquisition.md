- **`RateSync` acquires, tracks and coasts.** `configure_acquisition(symbols,   quality)` turns on a feedforward timing estimate that seeds the loop, so
    the time to a closed loop is a count; `seed(eye, rate)` is the same
    hand-over for a caller that already has the timing. `lifecycle` reads
    `"acquiring"`, `"tracking"` or `"coasting"`, `valid` is the flag beside
    each symbol, and `presence` says where a lost lock goes. Off by default:
    with no acquisition configured the object is as it was. The state blob
    is version 3 ([design](docs/design/ratesync-timing.md), #3371 for what
    is left).
