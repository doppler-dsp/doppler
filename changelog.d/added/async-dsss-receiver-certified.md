- **`AsyncDsssReceiver` is certified for BPSK** at the SPEC operating point
    (Gold-1023, 3.069 Mcps, 2,700 sym/s):
    `src/doppler/dsss/tests/validation/async_dsss_receiver/results.md`
    gives its EVM loss against Es/N0, its lock times, how fast a lock
    drops, and its false-lock rate on noise, measured, with the limits
    every push asserts. `validate_async_dsss_receiver_lock` gains the
    `# quality` and `# falselock` blocks that measure them.
- **`ber_evm_ideal_db(esn0_db)`**: what the self-referenced EVM meter
    (`ber_evm_db`) reads on an ideal stream. The meter scales to unit total
    power, so a receiver's EVM loss is measured against this, not -Es/N0.
