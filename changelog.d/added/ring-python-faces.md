- **The ring's non-blocking surface reaches Python:** `peek(n)` (the view
    `wait(n)` would return, or `None` for *not yet* — a closed ring raises
    `EOFError` instead), `write_some(arr)`, `space` and `reset()` on
    `F32Buffer` / `F64Buffer` / `I16Buffer`. A single-threaded caller could
    not use the ring at all: `wait` deadlocks it. ~174 ns per 1024-sample
    frame against ~240 ns for `write`+`wait`. See
    [Ring Buffers](docs/examples/python-buffers.md)
    ([#1427](https://github.com/doppler-dsp/doppler/issues/1427)).
