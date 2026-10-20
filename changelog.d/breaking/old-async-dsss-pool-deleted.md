- **`AsyncDsssPool` and `CellAsyncDsssReceiver` are deleted.** The pool's
    central searcher could not keep up with its design point, and the cell
    receivers existed only to take its seeds. The name `AsyncDsssPool`
    returns on the parallel design
    ([async-dsss-parallel](../../docs/design/async-dsss-parallel.md)): N
    `AsyncDsssReceiver`s, each searching for itself. `AsyncDsssReceiver`'s
    `idle` state and property go with the cell mode (its value, 3, stays
    reserved, so `lost` is still 4), and its state blob is version 10.
