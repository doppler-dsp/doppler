- **`AsyncDsssPool` as parallel receivers, designed and measured.**
    `docs/design/async-dsss-parallel.md` lays out N independent
    `AsyncDsssReceiver`s under a simple controller. Its numbers, and its
    record's, are rendered from runs of shipped tools: a new validator,
    `validate_async_dsss_receiver_lock`, measures arrival and symbol lock
    through dropouts at 3.069 Mcps on transitioning data, and the C
    benches give what each receiver state costs.
