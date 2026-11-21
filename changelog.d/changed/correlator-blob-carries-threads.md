- **A search's state blob carries its thread request.** `set_threads()`'s
    count is a mutator's value (#2022), so a restored search now runs on
    the workers the writer asked for: an explicit count as asked, and 0 as
    the target's own online cores, capped at the tiles only at D = 1; the
    output is the same at every count. The `Correlator` state version is
    4, and the blobs that nest a search (`DsssReceiver`, `BurstCapture`)
    grow by 8 bytes. `AsyncDsssReceiver` drops its own copy of the count
    the writer ran, which overrode the request on restore; its state
    version is 18, and its blob and `AsyncDsssPool`'s keep their size
    ([what goes in the blob](docs/design/state-serialization.md), #3055).
