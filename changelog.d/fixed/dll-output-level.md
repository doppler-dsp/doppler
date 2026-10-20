- **`Dll(segments > 1)` divides its output by a slowly tracked level, not a
    period's own noisy magnitude.** `AsyncDsssReceiver`'s error rate moves
    from 0.89–1.00 dB to 0.65–0.89 dB off the BPSK bound, and its EVM and
    M2M4 stop flattering it (#2495).
