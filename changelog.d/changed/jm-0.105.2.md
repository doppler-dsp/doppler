- **just-makeit pin 0.105.0 → 0.105.2: `close()` on another thread no
    longer frees a generated binding under a running GIL-free call.**
    `Ddcr.close()` during `execute` segfaulted 3 runs in 3; it now waits for
    the call's result, then frees, and a later call raises `RuntimeError`
    (gh-2272). `wfm.Reader.read_follow` has the same guard; the hand-written
    `doppler.stream` binding does not (#2063). An out-of-range `int32_t` to
    `AccQ8(acc=…)`, `AccQ8.set_acc` or `I32ToF32.step` now gets jm's range
    message on Windows too (gh-2269), read from the generated parse rather
    than measured there; the hand-written `Telemetry.emit` is #2280.
