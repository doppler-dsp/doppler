- **`AsyncDsssPool` as parallel receivers, designed and measured.**
    `docs/design/async-dsss-parallel.md` lays out N independent
    `AsyncDsssReceiver`s under a simple controller. Its record is rendered
    from runs of shipped tools: a new validator,
    `validate_async_dsss_receiver_lock`, measures arrival and symbol lock
    through dropouts at 3.069 Mcps on transitioning data, and the C
    benches give the cost: one receiver tracks on 0.07 of real time and
    searches ±1/T_E on 0.23.
