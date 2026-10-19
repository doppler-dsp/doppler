- **`AsyncDsssReceiver` is certified for BPSK** at the SPEC operating point
    (Gold-1023, 3.069 Mcps, 2,700 sym/s):
    `src/doppler/dsss/tests/validation/async_dsss_receiver/results.md`.
    A locked receiver's EVM reads within about 0.5 dB of -Es/N0 from 5 to
    15 dB. At 10 dB a signal is tracked within about 45 ms of arriving.
    A receiver on noise never declares symbol lock, and a signal that
    leaves drops it within tens of ms. 18 limits are asserted on every
    push. `validate_async_dsss_receiver_lock` gains the `# quality` and
    `# falselock` blocks that measure them.
