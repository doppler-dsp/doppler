

# File dp\_complex.h



[**FileList**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**dp\_complex.h**](dp__complex_8h.md)

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






### From C++



A C++ translation unit gets the TYPES and nothing else. `float _Complex` and `double _Complex` are accepted by g++ and clang++ as the same builtin types the C compiler uses, so every prototype means in C++ exactly what it means in C, for a pointer and for a complex passed or returned by value alike. That is why the prototypes do not switch to `std::complex<T>` under `__cplusplus`: a class returned by value from a C function is not returned the way `float _Complex` is on every target (32-bit x86 and the Windows x64 convention differ), and [**dp\_square\_clip()**](util__core_8h.md#function-dp_square_clip) returns one.


What C++ does not get is the C99 complex LIBRARY: `crealf`, `cimagf`, `conjf`, `cabsf`, `cexpf`, `CMPLXF` and `I`. `<complex.h>` in C++ is `<complex>`, which declares none of them, and the macros the Windows branch below defines (`I`, `complex`, `conj`) would rewrite a C++ program's own names. So under `__cplusplus` this header includes nothing and defines nothing, and a function a header DEFINES with that surface is C-only there:



* a per-sample fast path (`fir_step`, `lo_step`) is not declared to C++ at all; call the object's block function instead;
* a function that is part of an object's API and has an out-of-line copy in the library is declared to C++ as a prototype, and the call links that copy. Where the API function is header-only ([**dp\_cic\_decimate()**](cic__core_8h.md#function-dp_cic_decimate)), the library emits that copy from the same inline definition with an `extern` declaration, so there is still one body.




`std::complex<T>` is layout-compatible with `T _Complex` (the C++ standard, clause complex.numbers), so a C++ caller hands its buffers over by pointer:





That snippet is `tests/install/cxx-consumer/main.cpp`, which the gate below compiles, runs and compares with its C twin.


`make cxx-headers-check` holds this: every installed header must parse as C++, bar the private and Python-only headers in `scripts/.cxx-headers-not-cxx`, which no consumer includes (the shrinking list `scripts/.cxx-headers-exempt` is empty). It also reads each header's linkage: a function a header declares outside `extern "C"` that is neither `static` nor `inline` fails it, because C++ would link a mangled name the library does not define. 



    

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/dp_complex.h`

