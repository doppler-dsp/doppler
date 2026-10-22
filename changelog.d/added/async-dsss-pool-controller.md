- **`AsyncDsssPool` returns as N `AsyncDsssReceiver`s under a controller.**
    Each receiver searches the whole range for itself; `push(x)` steps all
    of them on one block in parallel (`threads`), then releases the lost,
    resets the later of two receivers within a chip of one code phase, and
    hands out exclusions and ranks so they hold different signals.
    `status(i)` and `symbols(i)` report each one. Results do not depend on
    the thread count (`async-dsss-parallel.md` §4).
