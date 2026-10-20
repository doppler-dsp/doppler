- **The `AsyncDsssReceiver` certification measures the error rate against the
    BPSK bound.** At the SPEC operating point, over the bursts that settle, the
    BER sits 0.65 to 0.89 dB from `Q(sqrt(2 Es/N0))` from 5 to 8 dB Es/N0. The
    bursts that do not settle (F3, a known defect) are counted and ratcheted.
    The receiver, the stimulus and the measurement are the certification's own,
    and the spot check asserts the loss within 1.1 dB and the ratchet
    ([the record](../../src/doppler/dsss/tests/validation/async_dsss_receiver/results.md),
    §2.6). Where the loss goes, stage by stage, is the record's §2.7 (#2495).
