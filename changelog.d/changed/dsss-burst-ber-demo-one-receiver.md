- **The DSSS burst BER Monte Carlo takes Eb/N0 straight and reuses one
    receiver.** `Composer(type="dsss", snr_mode="ebno", …)` lets
    `plan.at(ebn0, seed)` take the number the curve is plotted against, and
    `rx.reset()` (asserted output-identical to a fresh receiver) cuts a trial
    from 25 ms to 7 ms: the same curve, 3.4× faster.
