

# File dp\_complex.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**dp\_complex.h**](dp__complex_8h.md)

[Go to the source code of this file](dp__complex_8h_source.md)

_The complex-math surface, routed so it survives Windows._ [More...](#detailed-description)

* `#include <complex.h>`

































































## Detailed Description


Include this instead of `<complex.h>`. On Linux and macOS it IS `<complex.h>` and nothing changes; on Windows it supplies the same names without touching the platform header.


### Why the indirection



MSVC has no `_Complex` keyword at all. Its `<complex.h>` is built around `_Fcomplex`/`_Dcomplex` STRUCTS, with arithmetic in function form (`_FCmulcc`)  so `crealf` there is declared as taking an `_Fcomplex`, while doppler passes a native `float _Complex`. The two cannot meet.


clang-cl does implement `_Complex` while targeting the MSVC ABI, so the language half is free. The library half is not: pulling in MSVC's `<complex.h>` reintroduces the struct-shaped declarations. So this header never includes it on Windows, and supplies the surface from clang builtins plus REAL-valued libm instead.


Measured on clang 22 targeting `x86_64-pc-windows-msvc` with doppler's own flags (`-O3 -march=x86-64-v2 -ffast-math -fno-finite-math-only`): a complex DSP kernel compiled through this header leaves exactly these undefined symbols.  All five are plain-float UCRT exports. No `_Fcomplex` crosses the ABI, and `__mulsc3`/`__divsc3` inline away under `-ffast-math`, so there is no compiler-rt dependency either. That is why `cabsf`/`cargf`/`cexpf` are routed through `hypotf`/`atan2f`/`expf`+`cosf`/`sinf` rather than being left to lower to the CRT's complex-typed entry points.




**Note:**

On Linux and macOS this is a pure passthrough  the same `<complex.h>` symbols, unchanged  so numerics cannot drift between platforms by way of this header. 






    

------------------------------
The documentation for this class was generated from the following file `native/inc/dp_complex.h`

