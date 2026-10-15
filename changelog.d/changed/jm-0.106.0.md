- **just-makeit pin 0.105.0 → 0.106.0: `close()` on another thread no
    longer frees a generated binding under a running GIL-free call.**
    `Ddcr.close()` during `execute` segfaulted; it now waits, then frees, and
    a later call raises `RuntimeError` (gh-2272), as does
    `wfm.Reader.read_follow`. `doppler.stream` does not yet: closing a `Pull`
    while another thread is inside `recv()` can still crash, so join that
    thread first (#2063). `help(wfm.Synth)` and the seven source factories
    print the stub's docs (gh-2283). An out-of-range `int32_t` gets jm's
    range message on Windows too (gh-2269; not yet run there), but
    `Telemetry.emit` does not yet (#2280). Skips 0.105.1 and 0.105.2.
