- **The coverage job re-ran the `sweep` validators under instrumentation and
    was cancelled at its 90-minute cap**, blocking every open PR for a day.
    They are 94.3% of the instrumented ctest CPU and one of them is the whole
    critical path; the leg now excludes them and runs in 55.7 s, at a cost of
    nine covered lines in 29738. `COV_SWEEP=1` restores them.
    [#1292](https://github.com/doppler-dsp/doppler/issues/1292).
