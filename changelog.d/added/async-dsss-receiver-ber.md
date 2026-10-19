- **The `AsyncDsssReceiver` certification measures the error rate against the
    BPSK bound.** At the SPEC operating point, over the bursts that settle, the
    BER sits 0.89 to 1.00 dB from `Q(sqrt(2 Es/N0))` from 5 to 8 dB Es/N0. A
    burst that does not settle is held to F3's mechanism: it must have been
    handed over past the carrier loop's reliable pull-in. The receiver, the
    stimulus and the measurement are the certification's own, and the spot
    check asserts both, with the loss within 1.2 dB
    ([the record](../../src/doppler/dsss/tests/validation/async_dsss_receiver/results.md),
    §2.6). Where the 0.9 dB goes is #2495.
