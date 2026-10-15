- **just-makeit pin 0.105.0 → 0.105.2: `close()` on another thread no
    longer frees an object under a running GIL-free call.** `Ddcr.close()`
    during an `execute` freed the state under it, a segfault 3 runs in 3
    here. It now waits for the running call to return its result, then frees,
    and a later call raises `RuntimeError` (gh-2272). The same guard covers
    `wfm.Reader.read_follow`. On Windows an out-of-range `int32_t` argument
    (`AccQ8(acc=…)`, `AccQ8.set_acc`, `I32ToF32.step`) now gets jm's range
    message, as it already did elsewhere (gh-2269).
