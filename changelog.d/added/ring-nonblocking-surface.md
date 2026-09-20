- **The ring buffer gains its non-blocking surface:** `dp_*_peek`
    (`wait()` that never blocks), `dp_*_write_some` (takes what fits — the
    only way to feed a chunk larger than the ring), `dp_*_space`,
    `dp_*_reset` and `dp_*_wait_status`. Every C consumer was rebuilding
    these from the struct by hand. ~3% over `write`+`wait` at a 1024 frame.
    See [The Ring Buffer](docs/design/ring-buffer.md).
