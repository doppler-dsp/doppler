- **The Python extensions get further under clang-cl on Windows.**
    `dp_complex.h` now coexists with the UCRT `<complex.h>`, which numpy
    includes on every MSVC build: the first Windows `make pyext` failed with
    `conflicting types for 'cabsf'` and `float * _Fcomplex`. The stream
    extension is skipped where its core is not built. Step 1 of
    [#1457](https://github.com/doppler-dsp/doppler/issues/1457).
