- **just-makeit pin 0.75.5 → 0.76.4.** Six doppler-driven changes:
    `header_only`, `borrow`, a borrowed `record_dtype` (the fix for #1346), a
    method on a header-only component, a destructor derived from the declared
    creator, and a portable benchmark timer. Also resolves libm by path rather
    than the shadowable `m`, and `.pyi` stubs gain real default values instead
    of `...`. Per-issue list and the jm links:
    [#1363](https://github.com/doppler-dsp/doppler/pull/1363).
