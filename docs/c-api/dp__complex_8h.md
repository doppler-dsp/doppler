

# File dp\_complex.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**dp\_complex.h**](dp__complex_8h.md)

[Go to the source code of this file](dp__complex_8h_source.md)

_The complex-math surface, routed so it survives Windows._ [More...](#detailed-description)

* `#include <complex.h>`

































































## Detailed Description


Include this instead of `<complex.h>`. On Linux and macOS it IS `<complex.h>` and nothing changes; on Windows it supplies the same names without touching the platform header.


### Why the indirection



MSVC has no `_Complex` keyword at all. Its `<complex.h>` is built around `_Fcomplex`/`_Dcomplex` STRUCTS, with arithmetic in function form (`_FCmulcc`)  so `crealf` there is declared as taking an `_Fcomplex`, while doppler passes a native `float _Complex`. The two cannot meet.


clang-cl does implement `_Complex` while targeting the MSVC ABI, so the language half is free. The library half is not: MSVC's `<complex.h>` declares every name struct-shaped. So this header supplies the surface from clang builtins plus REAL-valued libm, under doppler-owned names, and points the C99 names at them by macro.


It does include MSVC's `<complex.h>`  FIRST, and only so that nothing else includes it later. numpy's `npy_common.h` includes it on every MSVC build, so every Python extension pulled it in AFTER this header: its `cabsf(_Fcomplex)` then collided with the definition here, and its `I`  a `_Fcomplex` struct  replaced this header's, so `x * I` in any inline header function stopped compiling (doppler#1457, the first Windows `make pyext`). Included first, its declarations are made before any name below is a macro, and its include guard makes every later include a no-op.


Measured on clang 22 targeting `x86_64-pc-windows-msvc` with doppler's own flags (`-O3 -march=x86-64-v2 -ffast-math -fno-finite-math-only`): a complex DSP kernel compiled through this header leaves exactly these undefined symbols.  All five are plain-float UCRT exports. No `_Fcomplex` crosses the ABI, and `__mulsc3`/`__divsc3` inline away under `-ffast-math`, so there is no compiler-rt dependency either. That is why `cabsf`/`cargf`/`cexpf` are routed through `hypotf`/`atan2f`/`expf`+`cosf`/`sinf` rather than being left to lower to the CRT's complex-typed entry points.




**Note:**

On Linux and macOS this is a pure passthrough  the same `<complex.h>` symbols, unchanged  so numerics cannot drift between platforms by way of this header. 






    

------------------------------
The documentation for this class was generated from the following file `native/inc/dp_complex.h`

