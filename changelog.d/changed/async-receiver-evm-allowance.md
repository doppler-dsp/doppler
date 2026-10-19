- **The `AsyncDsssReceiver` certification's EVM limit is two-sided and set
    per Es/N0.** The flat "at most 2.0 dB over the ideal" let about 0.8 dB
    of new loss through at 10 and 15 dB. The bound is now the ideal plus a
    measured implementation floor plus the estimator's counting noise:
    +0.47 dB at 10 dB, +0.78 at 15 and +1.65 at 20, and no more than
    0.30 dB under the ideal. The C harness computes it, emits it and asserts
    it. The report also stops calling a missed hand-off a below-10-dB
    effect (1 of 21 dropout draws at 10 dB), and
    `make validate-async-receiver-sweep` re-runs the full sweep.
