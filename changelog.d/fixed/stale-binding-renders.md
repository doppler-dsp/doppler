- **37 Python bindings receive the generator fixes they had been missing.**
    A per-object binding file is rendered once and never refreshed, so these
    had drifted from their manifests. Visible effects: `DelayCf64.ptr()` /
    `push_ptr()` return an array that owns its data (a later call no longer
    rewrites an earlier result) and `ptr(count=k, out=...)` needs only `k`
    elements; the accumulators' `madd`/`add2d`/`madd2d` accept keywords
    ([#1446](https://github.com/doppler-dsp/doppler/issues/1446)).
