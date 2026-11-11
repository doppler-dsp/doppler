- **`HalfbandDecimatorR2C` is generated from its manifest.** It is now a
    jm object, a thin forwarder over the `hbdecim_r2c` core like
    `HalfbandDecimator`, so it has a type stub, a reference page, state
    serialization and a benchmark. One behavior changed: float64 input or
    taps raise `TypeError` where the hand-written binding narrowed them
    silently, so cast them with `.astype(np.float32)` (#3002).
