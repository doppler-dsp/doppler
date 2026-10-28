- **`make bench-interleaved` leaves its exit code in a file.** The hour-long
    release bench run writes `build/bench-runs/<VERSION>.log` and, last,
    `<VERSION>.rc`, on success and on every failure, so a watcher waits on the
    file instead of the process. A remote `pgrep` matched its own ssh shell, and
    v0.77.0's failed run read "running" for hours
    ([#2681](https://github.com/doppler-dsp/doppler/issues/2681)).
