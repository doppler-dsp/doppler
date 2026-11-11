- **The hand-written Python API pages are gone; the reference is generated.**
    The last two, the loop filters and the waveform library, become guides,
    [Tracking Loops](docs/guide/tracking-loops.md) and [The Python
    Library](docs/guide/wfmgen/python.md); the [API Reference](docs/api/index.md)
    page now points at the generated Python reference and the C API; and
    `make test-api-docs` is retired, since the snippet gate already runs
    every example those pages held (#3000).
