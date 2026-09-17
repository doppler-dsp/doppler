- **just-makeit pin 0.75.5 → 0.76.4.** Ships six doppler-driven changes:
    `header_only` ([jm#1311](https://github.com/just-buildit/just-makeit/issues/1311)),
    `borrow` ([jm#1312](https://github.com/just-buildit/just-makeit/issues/1312)),
    a borrowed `record_dtype` ([jm#1310](https://github.com/just-buildit/just-makeit/issues/1310) —
    the fix for #1346), a method on a header-only component
    ([jm#1321](https://github.com/just-buildit/just-makeit/issues/1321)), a
    destructor derived from the declared creator
    ([jm#1323](https://github.com/just-buildit/just-makeit/issues/1323)) and a
    portable benchmark timer
    ([jm#1341](https://github.com/just-buildit/just-makeit/issues/1341)). Also
    resolves libm by path rather than the shadowable `m`
    ([jm#1305](https://github.com/just-buildit/just-makeit/issues/1305)), and
    `.pyi` stubs gain real default values instead of `...`.
