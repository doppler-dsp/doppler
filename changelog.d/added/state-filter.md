- **`StateFilter`, a third-order loop filter for a drifting carrier.**
    `doppler.track.StateFilter(bn, t)` keeps a rate estimate beside the
    frequency estimate, so a frequency ramp is tracked with no steady phase
    error (a second-order loop settles to `r/ki`), at the same noise
    bandwidth, from `bn` and `t` alone. The bandwidth asked for is the one
    delivered, and `bandwidth(delay, gain)` and `stable(delay, gain)` give
    the exact answer for a detector delay and loop gain. See the
    [tracking loops guide](docs/guide/tracking-loops.md#statefilter-a-loop-filter-for-a-drifting-carrier).
