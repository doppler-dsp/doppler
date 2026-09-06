- **`AsyncDsssPool` — one object holds the population** (design §8.2,
    §12.13): one searcher, `n_slots` hand-off receivers created idle, the
    assigned table with the exclusion zone keyed on where each emitter is
    now, and the run's `EventLog` by attachment; one `push()` per block
    seeds, feeds across threads, and releases; `status(slot)` by value,
    `symbols(slot)` borrowed; the composition serializes as a whole.
    [#1260](https://github.com/doppler-dsp/doppler/issues/1260).
