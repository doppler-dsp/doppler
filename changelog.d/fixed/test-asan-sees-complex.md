- **`make test-asan` sees an out-of-bounds read through a complex
    sample.** GCC's AddressSanitizer does not instrument a `float _Complex`
    access (`crealf(b[i])`, a whole-complex load or store), which is how
    the DSP kernels read samples, so a gcc-built run passed while resamp
    read 19 samples past its delay line. `test-asan` now builds with clang
    (`ASAN_CC`), and a canary complex read must draw an ASan report or the
    gate fails (#2130).
