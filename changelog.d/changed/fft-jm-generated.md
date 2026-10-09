- **`spectral.FFT` is jm-generated** (doppler#1886). `execute_ci16()` and
    `execute_ci8()` stay hand-written, in `spectral_ext_fft_extra.c`, with
    their #1933 odd-count refusal, and are declared as `extra_methods`. The
    four complex executes' one-frame refusal (#1925) moves from hand code
    into the manifest (`error_sentinel = "SIZE_MAX"`, as `FFT2D` already
    declares it), with the same `ValueError` and message. jm's render also
    refuses a `str` where an array goes, an output size past `NPY_MAX_INTP`
    and a kernel count past the buffer. The `iq` argument of
    `execute_ci16`/`execute_ci8` is positional-only in the stub, as the
    binding always was. The fragment leaves the `-Wall -Wextra` exempt
    list.
