- **`I16Buffer` speaks a record, and the ring's Python binding is generated.**
    `wait()` / `peek()` lend — and `write()` / `write_some()` take — a 1-D
    `[("i", "<i2"), ("q", "<i2")]` array, one element per sample like
    `F32Buffer` / `F64Buffer`, instead of `int16` of shape `(n, 2)`
    ([#1346](https://github.com/doppler-dsp/doppler/issues/1346)); convert
    either way with `.view()`, no copy. Also: a wrong-dtype or non-contiguous
    input is refused on every width rather than cast, a bare `consume()` with
    no view outstanding raises `RuntimeError`, and the array keyword is `x`.
    1,900 hand-written lines become three manifests
    ([#1358](https://github.com/doppler-dsp/doppler/pull/1358)).
