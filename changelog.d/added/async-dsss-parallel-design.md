- **`AsyncDsssPool` as parallel receivers, designed and measured.**
    `docs/design/async-dsss-parallel.md` lays out N independent
    `AsyncDsssReceiver`s under a simple controller, and `make characterize`
    gains the run behind its numbers: at 3.069 Mcps one receiver tracks on
    0.07–0.08 of a CPU and searches the whole ±1/T_E range on 0.64.
