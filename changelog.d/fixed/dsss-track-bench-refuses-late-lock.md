- **The DSSS receiver's benchmarks refuse a lock in the wrong place.**
    The `track` row shared its timing array with the `search` row and
    stopped early when the signal ran out, so a receiver that locked late
    recorded search timings as tracking, and one that never locked (the
    signal used up) ran no tracking rounds and published a copy of the
    search row, both without a word. It now checks for a lock with every
    round's signal still available, and `search` refuses a lock on silence
    ([#2363](https://github.com/doppler-dsp/doppler/issues/2363)).
