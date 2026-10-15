- **just-makeit pin 0.105.0 → 0.106.0: `close()` on another thread no
    longer frees a generated binding under a running GIL-free call.**
    `Ddcr.close()` during `execute` segfaulted 3 runs in 3; it now waits,
    then frees, and a later call raises `RuntimeError` (gh-2272), as does
    `wfm.Reader.read_follow`; the hand-written `doppler.stream` does not
    (#2063). `help(wfm.Synth)` and the seven source factories now print the
    stub's docs (gh-2283). An out-of-range `int32_t` gets jm's range message
    on Windows too (gh-2269, read from the parse, not run there). Skips
    0.105.1 and 0.105.2, whose `stream()` iterator crashed when freed.
