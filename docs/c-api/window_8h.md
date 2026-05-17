

# File window.h



[**FileList**](files.md) **>** [**c**](dir_1784a01aa976a8c78ef5dfc3737bcac8.md) **>** [**include**](dir_2d10db7395ecfee73f7722e70cabff64.md) **>** [**dp**](dir_11a94baa66ce4f1e4099aa44a4fd2c26.md) **>** [**window.h**](window_8h.md)

[Go to the source code of this file](window_8h_source.md)

_Window functions for spectral analysis and filter design._ [More...](#detailed-description)

* `#include <complex.h>`
* `#include <stddef.h>`





































## Public Functions

| Type | Name |
| ---: | :--- |
|  float | [**dp\_kaiser\_enbw**](#function-dp_kaiser_enbw) (const float \* w, size\_t n) <br>_Compute the equivalent noise bandwidth (ENBW) of a window._  |
|  void | [**dp\_kaiser\_window**](#function-dp_kaiser_window) (float \* w, size\_t n, float beta) <br>_Fill_ `w` _with a Kaiser window of length_`n` _and shape_`beta` _._ |
|  void | [**dp\_magnitude\_db\_cf32**](#function-dp_magnitude_db_cf32) (const float \_Complex \* in, size\_t n, float \* out, float lin\_floor, float offset\_db) <br>_Convert a CF32 spectrum to dB magnitude._  |
|  void | [**dp\_magnitude\_db\_cf64**](#function-dp_magnitude_db_cf64) (const double \_Complex \* in, size\_t n, float \* out, double lin\_floor, float offset\_db) <br>_Convert a CF64 spectrum to dB magnitude (float output)._  |




























## Detailed Description


## Public Functions Documentation




### function dp\_kaiser\_enbw

_Compute the equivalent noise bandwidth (ENBW) of a window._
```C++
float dp_kaiser_enbw (
    const float * w,
    size_t n
)
```



Returns the ENBW in units of FFT bins. Multiply by \(F_s / N\) to convert to Hz.




**Parameters:**


* `w` Window coefficients, length `n`. Must be non-NULL.
* `n` Window length. Must be ≥ 1.



**Returns:**

ENBW in bins.







<hr>



### function dp\_kaiser\_window

_Fill_ `w` _with a Kaiser window of length_`n` _and shape_`beta` _._
```C++
void dp_kaiser_window (
    float * w,
    size_t n,
    float beta
)
```





**Parameters:**


* `w` Output buffer, length `n`. Must be non-NULL.
* `n` Window length (number of samples). Must be ≥ 1.
* `beta` Shape factor \(\beta \geq 0\). 0 → rectangular.






<hr>



### function dp\_magnitude\_db\_cf32

_Convert a CF32 spectrum to dB magnitude._
```C++
void dp_magnitude_db_cf32 (
    const float _Complex * in,
    size_t n,
    float * out,
    float lin_floor,
    float offset_db
)
```



Computes  \(20 \log_{10}(\max(|x_k|, \texttt{lin\_floor})) +
\texttt{offset\_db}\) for each element. The floor prevents \(-\infty\) in noise bins; a typical value is `1e-10f` (`-200` dBFS). `offset_db` applies a global calibration shift, e.g. `-20*log10f`(N) to normalise an FFT by its length.


The loop is left to auto-vectorise under `-ffast-math`; clang and GCC both produce AVX2 code when compiled with `-march=native`.




**Parameters:**


* `in` Input spectrum, length `n`. Must be non-NULL.
* `n` Number of complex samples.
* `out` Output magnitude array, length `n`. May alias `in` (both are float; the cast is safe).
* `lin_floor` Minimum amplitude before log (&gt; 0).
* `offset_db` Additive offset applied after log (e.g. 0.0f).

Example:
```C++
float _Complex spec[1024];
float mag[1024];
// ... fill spec via FFT ...
dp_magnitude_db_cf32(spec, 1024, mag, 1e-10f, -20.0f * log10f(1024));
// mag[k] ≈ dBFS power in bin k, normalised by FFT length
```





<hr>



### function dp\_magnitude\_db\_cf64

_Convert a CF64 spectrum to dB magnitude (float output)._
```C++
void dp_magnitude_db_cf64 (
    const double _Complex * in,
    size_t n,
    float * out,
    double lin_floor,
    float offset_db
)
```



Double-precision version of [**dp\_magnitude\_db\_cf32()**](window_8h.md#function-dp_magnitude_db_cf32). Input is `double` `_Complex`; output is `float` to match the CF32 variant and avoid doubling memory when only display precision is needed.




**Parameters:**


* `in` Input spectrum, length `n`. Must be non-NULL.
* `n` Number of complex samples.
* `out` Output magnitude array, length `n`.
* `lin_floor` Minimum amplitude before log (&gt; 0).
* `offset_db` Additive offset applied after log.

Example:
```C++
double _Complex spec[4096];
float mag[4096];
// ... fill spec via dp_fft1d_execute ...
dp_magnitude_db_cf64(spec, 4096, mag, 1e-15, -20.0 * log10(4096));
```





<hr>

------------------------------
The documentation for this class was generated from the following file `c/include/dp/window.h`
