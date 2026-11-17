- **A search's state blob carries its thread request.** `set_threads()`'s
    count is a mutator's value (#2022), so a restored search now runs on
    the workers the writer asked for, capped by the target's own grid and
    cores; the output is the same at every count. The `Correlator` state
    version is 4, and the blobs that nest a search (`AsyncDsssReceiver`,
    `AsyncDsssPool`, `DsssReceiver`, `BurstCapture`) grow by 8 bytes
    ([what goes in the blob](docs/design/state-serialization.md), #3055).
