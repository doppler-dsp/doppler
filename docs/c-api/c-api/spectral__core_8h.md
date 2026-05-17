

# File spectral\_core.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**spectral**](dir_2aadf81c4f49e887d76ad198d657298d.md) **>** [**spectral\_core.h**](spectral__core_8h.md)

[Go to the source code of this file](spectral__core_8h_source.md)

_Spectral module — public C API._

* `#include "clib_common.h"`





































## Public Functions

| Type | Name |
| ---: | :--- |
|  void | [**hann\_window**](#function-hann_window) (float \* w, size\_t w\_len) <br>_Fill_ `w` _with a Hann window of length_`w_len` _._ |
|  float | [**kaiser\_enbw**](#function-kaiser_enbw) (const float \* w, size\_t w\_len) <br> |
|  void | [**kaiser\_window**](#function-kaiser_window) (float \* w, size\_t w\_len, float beta) <br> |
|  void | [**magnitude\_db\_cf32**](#function-magnitude_db_cf32) (const float \_Complex \* in, size\_t n, float \* out, float lin\_floor, float offset\_db) <br>_Convert a CF32 spectrum to F32 dB, bin by bin._  |
|  void | [**magnitude\_db\_cf64**](#function-magnitude_db_cf64) (const double \_Complex \* in, size\_t n, float \* out, double lin\_floor, float offset\_db) <br>_Convert a CF64 spectrum to F32 dB, bin by bin._  |




























## Public Functions Documentation




### function hann\_window

_Fill_ `w` _with a Hann window of length_`w_len` _._
```C++
void hann_window (
    float * w,
    size_t w_len
)
```



w[k] = 0.5 \* (1 - cos(2π k / (N-1))), k = 0..N-1


Peak side-lobe −31.5 dB, ENBW = 1.5 bins. Zero-parameter alternative to Kaiser when moderate rejection is sufficient.




**Parameters:**


* `w` Output buffer, length `w_len`.
* `w_len` Window length ≥ 1.






<hr>



### function kaiser\_enbw

```C++
float kaiser_enbw (
    const float * w,
    size_t w_len
)
```




<hr>



### function kaiser\_window

```C++
void kaiser_window (
    float * w,
    size_t w_len,
    float beta
)
```




<hr>



### function magnitude\_db\_cf32

_Convert a CF32 spectrum to F32 dB, bin by bin._
```C++
void magnitude_db_cf32 (
    const float _Complex * in,
    size_t n,
    float * out,
    float lin_floor,
    float offset_db
)
```



out[k] = 20\*log10(max(\|in[k]\|, lin\_floor)) + offset\_db


Avoids per-call Python overhead on the hot FFT→display path.




**Parameters:**


* `in` CF32 spectrum, length `n`.
* `n` Number of bins.
* `out` F32 output, length `n`.
* `lin_floor` Amplitude floor before log10 (e.g. 1e-12f).
* `offset_db` Calibration/level offset added to every bin.






<hr>



### function magnitude\_db\_cf64

_Convert a CF64 spectrum to F32 dB, bin by bin._
```C++
void magnitude_db_cf64 (
    const double _Complex * in,
    size_t n,
    float * out,
    double lin_floor,
    float offset_db
)
```



Same as [**magnitude\_db\_cf32()**](spectral__core_8h.md#function-magnitude_db_cf32) but accepts double-precision input. Output is F32 (sufficient for display).




**Parameters:**


* `in` CF64 spectrum, length `n`.
* `n` Number of bins.
* `out` F32 output, length `n`.
* `lin_floor` Amplitude floor before log10 (e.g. 1e-12).
* `offset_db` Calibration/level offset added to every bin.






<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/spectral/spectral_core.h`
