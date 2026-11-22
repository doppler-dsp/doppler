- **The tile-edge alias check runs on the search's pool.** A tiled,
    block-coherent search resolves a dwell's picks as one fan of three tasks
    a pick, one wake a dwell, each summing in the serial loop's order, so
    every reported hit is bit-identical at any thread count. It was 61
    ns/sample of the async-DSSS pool's serial floor at K=9 (#3060).
