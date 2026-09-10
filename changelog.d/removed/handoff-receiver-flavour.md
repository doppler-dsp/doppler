- **`HandoffAsyncDsssReceiver`** (C: `async_dsss_receiver_create_handoff`)
    is retired: the same seed into the receiver's own refine chain, replaced
    by `CellAsyncDsssReceiver` once the cell mode matched it on the
    lifecycle soak and the pull-in curve (design §11.1, §12.27–12.28). The
    base `AsyncDsssReceiver` keeps its refine for its own search; a seeded
    refine is `AsyncDsssReceiver.seed()`. Receiver blob v7, pool blob v3.
