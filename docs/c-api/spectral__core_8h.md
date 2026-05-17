

# File spectral\_core.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**spectral**](dir_2aadf81c4f49e887d76ad198d657298d.md) **>** [**spectral\_core.h**](spectral__core_8h.md)

[Go to the source code of this file](spectral__core_8h_source.md)

_Spectral module — public C API._ [More...](#detailed-description)

* `#include "clib_common.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**dp\_peak\_t**](structdp__peak__t.md) <br>_One spectral peak returned by_ [_**find\_peaks\_f32()**_](spectral__core_8h.md#function-find_peaks_f32) _._ |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  size\_t | [**find\_peaks\_f32**](#function-find_peaks_f32) (const float \* db, size\_t n, size\_t n\_peaks, float min\_db, [**dp\_peak\_t**](structdp__peak__t.md) \* out) <br>_Find up to_ `n_peaks` _local maxima in a DC-centred F32 dB spectrum._ |
|  void | [**hann\_window**](#function-hann_window) (float \* w, size\_t w\_len) <br>_Fill_ `w` _with a Hann window._ |
|  float | [**kaiser\_enbw**](#function-kaiser_enbw) (const float \* w, size\_t w\_len) <br>_Equivalent noise bandwidth of window_ `w` _._ |
|  void | [**kaiser\_window**](#function-kaiser_window) (float \* w, size\_t w\_len, float beta) <br>_Fill_ `w` _with a Kaiser window of shape parameter_`beta` _._ |
|  void | [**magnitude\_db\_cf32**](#function-magnitude_db_cf32) (const float complex \* in, size\_t n, float \* out, float lin\_floor, float offset\_db) <br>_Convert CF32 spectrum to F32 dB._  |
|  void | [**magnitude\_db\_cf64**](#function-magnitude_db_cf64) (const double complex \* in, size\_t n, float \* out, double lin\_floor, float offset\_db) <br>_Convert CF64 spectrum to F32 dB._  |




























## Detailed Description


Provides windowing (Kaiser, Hann), ENBW computation, magnitude conversion, and peak finding. These are pure functions with no persistent state. 


    
## Public Functions Documentation




### function find\_peaks\_f32 

_Find up to_ `n_peaks` _local maxima in a DC-centred F32 dB spectrum._
```C++
size_t find_peaks_f32 (
    const float * db,
    size_t n,
    size_t n_peaks,
    float min_db,
    dp_peak_t * out
) 
```



Algorithm:
* Local-max scan: db(k) &gt; db(k-1) && db(k) &gt;= db(k+1), above min\_db.
* Parabolic interpolation for sub-bin frequency accuracy.
* Sort descending by amplitude; return top n\_peaks.






**Parameters:**


* `db` F32 dB spectrum, DC-centred, length `n`. Must be &gt;= 3. 
* `n` Number of bins. 
* `n_peaks` Maximum number of peaks to return. 
* `min_db` Amplitude threshold; bins below this are ignored. 
* `out` Caller-allocated output array, length &gt;= `n_peaks`. 



**Returns:**

Number of peaks written (&lt;= n\_peaks). 





        

<hr>



### function hann\_window 

_Fill_ `w` _with a Hann window._
```C++
void hann_window (
    float * w,
    size_t w_len
) 
```



w(k) = 0.5 \* (1 - cos(2π k / (N-1))), k = 0..N-1.




**Parameters:**


* `w` Output buffer, length `w_len` (modified in-place). 
* `w_len` Window length &gt;= 1. 




        

<hr>



### function kaiser\_enbw 

_Equivalent noise bandwidth of window_ `w` _._
```C++
float kaiser_enbw (
    const float * w,
    size_t w_len
) 
```



ENBW = N \* sum(w^2) / sum(w)^2




**Parameters:**


* `w` Float32 window coefficients, length `w_len`. 
* `w_len` Number of window samples. 



**Returns:**

ENBW in bins. 





        

<hr>



### function kaiser\_window 

_Fill_ `w` _with a Kaiser window of shape parameter_`beta` _._
```C++
void kaiser_window (
    float * w,
    size_t w_len,
    float beta
) 
```



Uses the Bessel function I0 via a converging power series.




**Parameters:**


* `w` Output buffer, length `w_len` (modified in-place). 
* `w_len` Window length &gt;= 1. 
* `beta` Shape parameter (higher = more attenuation, wider main lobe). 




        

<hr>



### function magnitude\_db\_cf32 

_Convert CF32 spectrum to F32 dB._ 
```C++
void magnitude_db_cf32 (
    const float complex * in,
    size_t n,
    float * out,
    float lin_floor,
    float offset_db
) 
```



out(k) = 20\*log10(max(\|in(k)\|, lin\_floor)) + offset\_db




**Parameters:**


* `in` CF32 spectrum, length `n`. 
* `n` Number of bins. 
* `out` F32 output, length `n` (caller-allocated). 
* `lin_floor` Amplitude floor before log10 (e.g. 1e-12f). 
* `offset_db` Calibration offset added to every bin. 




        

<hr>



### function magnitude\_db\_cf64 

_Convert CF64 spectrum to F32 dB._ 
```C++
void magnitude_db_cf64 (
    const double complex * in,
    size_t n,
    float * out,
    double lin_floor,
    float offset_db
) 
```



Same as [**magnitude\_db\_cf32()**](spectral__core_8h.md#function-magnitude_db_cf32) but accepts double-precision input.




**Parameters:**


* `in` CF64 spectrum, length `n`. 
* `n` Number of bins. 
* `out` F32 output, length `n` (caller-allocated). 
* `lin_floor` Amplitude floor (double precision). 
* `offset_db` Calibration offset added to every bin. 




        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/spectral/spectral_core.h`

