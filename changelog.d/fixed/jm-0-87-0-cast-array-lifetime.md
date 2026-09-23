- **just-makeit pin 0.86.0 → 0.87.0: a module function no longer reads its
    array after freeing it** (jm#1490). When numpy had to cast the argument (a
    `bool` array to `ber_lock_symbol`'s `uint8_t[]`, and 16 functions across
    `arith`, `ber`, `cvt`, `snr`, `spectral` and `wfm` share the shape), the
    generated binding freed the temporary before the call read it. Windows CI
    crashed one run in three; on Linux the answer was silently wrong (a lock
    dated at 0 instead of 150000). Closes
    [#1477](https://github.com/doppler-dsp/doppler/issues/1477). The same
    release bounds-checks string-enum getters.
