- **`native/examples/wfmgen_demo.c` — the composed scene from C.** The spine
    asks for C *and* Python; wfmgen's API was reached only incidentally, by
    examples whose subject was something else. Shows what the Python face
    hides: the caller owns the output buffer, the stream ends with a short
    read rather than an error, `wfm_compose_segments()` is borrowed until
    `destroy()`, and a declaration composes byte-identically twice. Six
    checks, no `assert()` (examples build Release), exit non-zero on any
    failure.
    [#1056](https://github.com/doppler-dsp/doppler/pull/1056)
