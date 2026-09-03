- **`HandoffAsyncDsssReceiver`, `seed()` and the lost state.** The tracking
    receiver a pool holds: no search of its own, it takes a searcher's
    detection through `seed()` (assigned once -- a second seed is refused
    until `reset()`, which returns to idle) and reports its emitter gone
    (`lost`) when both lock flags stay down past `lost_confirm_s`. A view over
    the same core, so the chain past the seed is `AsyncDsssReceiver`'s
    verbatim; §11.1–11.2 of
    [the design page](https://doppler-dsp.github.io/doppler/design/async-dsss-receiver/).
