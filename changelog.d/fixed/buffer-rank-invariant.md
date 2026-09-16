- **The three ring buffers' `wait()` shapes are now pinned against each other**
    ([#1346](https://github.com/doppler-dsp/doppler/issues/1346)).
    `I16Buffer.wait()` returns 2-D `(n, 2)` while `F32Buffer`/`F64Buffer`
    return 1-D, so `len()` agrees across the three while the *element* does
    not — a sample for two of them, an I/Q pair row for the third. A strict
    xfail records the divergence and flips CI red when
    [just-makeit#1310](https://github.com/just-buildit/just-makeit/issues/1310)
    lands the generated template that returns `[('i','<i2'),('q','<i2')]`.
