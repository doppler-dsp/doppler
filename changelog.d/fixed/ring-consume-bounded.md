- **`consume()` is bounded, and two lines of Python can no longer crash the
    interpreter.** Releasing more than was readable pushed the ring's read
    position past its write position; `space` then exceeded `capacity`,
    `write()` believed it, and copied past the mapping —
    `buf.consume(1_000_000)` then a large `write()` was a SIGSEGV.
    `dp_*_consume()` now returns `DP_ERR_INVALID` and releases nothing (Python:
    `ValueError`), and `write()` / `write_some()` never copy more than
    `capacity` whatever the indices say. Measured free, one thread and two
    ([#1424](https://github.com/doppler-dsp/doppler/issues/1424),
    [measurements](docs/design/ring-buffer-measurements.md)).
