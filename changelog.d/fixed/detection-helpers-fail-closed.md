- **Every detection sizing helper fails closed on a probability outside
    (0, 1).** `det_snr(8, 1.5, 1e-3)` used to never return; it and the other
    threshold helpers now return `NaN`, and the count helpers `-1`.
    `det_q_inv`, `det_threshold_f` and `det_threshold_gauss` returned `0.0`
    before, a threshold every sample clears
    ([#1513](https://github.com/doppler-dsp/doppler/issues/1513)).
