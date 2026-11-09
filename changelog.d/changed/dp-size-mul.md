- **A size product is checked in one place, `dp_size_mul()`** in
    `clib_common.h` (with `dp_size_mul_sat()` for an upper bound). The
    hand-written `n > SIZE_MAX / k` guards in acc_trace, dp_tlm,
    spectrogram, wfm and the spectrogram validator now call it, and
    `make lint-size-mul` fails on a new one; correlator_core.c's seven are
    a shrink-only ratchet (`scripts/.size-mul-ratchet`).
