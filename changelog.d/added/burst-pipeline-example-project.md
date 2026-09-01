- **`example-projects/burst-pipeline/` — a downstream project to copy**, taking
    a burst waveform the whole way: a caller-built frame, a sixty-burst train,
    a 24-point SNR sweep, out to BLUE and back through a consumer, with both
    halves timed. It **measures** what it teaches — that sixty listed segments
    all carry *identical* noise while `repeats` draws fresh noise per instance,
    that the gap carries the floor so burst-over-gap power recovers the
    declared SNR, that a prepared `Plan` beats re-composing every point, and
    that cf32 through BLUE is byte-exact. `make burst-pipeline-check` builds
    and runs it in both link modes against a scratch install, in CI.
