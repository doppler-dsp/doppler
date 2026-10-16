- **The DSSS receiver's `track` benchmark refuses a late or missing
    lock.** It shared its timing array with the `search` row and stopped
    early when the signal ran out, so a receiver that locked late would have
    recorded search timings as tracking, and one that never locked would
    have timed acquisition under the `track` name, both without a word. It
    now checks for a lock with every round's signal still available, and
    fails the benchmark otherwise
    ([#2363](https://github.com/doppler-dsp/doppler/issues/2363)).
