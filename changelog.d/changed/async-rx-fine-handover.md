- **`AsyncDsssReceiver` hands over a fine carrier estimate.** At the end of
    the refine it squares the refine `Dll`'s partial dumps, `K` sums a code
    period, and hands over their tone's frequency
    (`PolynomialPhaseEstimator`, `ho_src` 3) when it clears its check;
    otherwise it falls back to `CarrierAcquisition`'s estimate or the seed.
    At the 415.1-B-1 point its median error is 0.10 / 0.06 Hz at the floor
    (2.2 / 2.8 GHz), and its worst over 720 draws is 1.68 Hz. Its check is
    set for a false-alarm rate of 10⁻³ on noise and runs above that (about
    1.5 to 2 times, #3293), because the estimator's interpolated peak
    searches more cells than the bound counts. The state
    blob is version 23
    ([async-dsss-receiver §4.11](docs/design/async-dsss-receiver.md)).
