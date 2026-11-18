- **`KalmanStateFilter`, the third-order loop with Kalman gains.**
    `doppler.track.KalmanStateFilter(sigma_e, sigma_a)` (or `bn` alone) sizes
    the loop from the detector's rms error and the rms change of the carrier's
    rate per update, and its gains come from a covariance recursion, so an
    initial frequency uncertainty gives a high-gain start that settles a
    frequency step about 1.7 times as fast as `StateFilter` at the same final
    bandwidth. Its own component, calling `StateFilter`. See the
    [tracking loops guide](docs/guide/tracking-loops.md#kalmanstatefilter-a-loop-sized-from-the-signal).
