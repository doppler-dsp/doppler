

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
|  size\_t | [**find\_peaks\_f32**](#function-find_peaks_f32) (const float \* db, size\_t db\_len, size\_t n\_peaks, float min\_db, [**dp\_peak\_t**](structdp__peak__t.md) \* result) <br>_Find up to_ `n_peaks` _local maxima in a DC-centred F32 dB spectrum. Three-step algorithm: (1) local-max scan — db[k] &gt; db[k-1] && db[k] &gt;= db[k+1] with db[k] &gt; min\_db; (2) parabolic interpolation on each local maximum to produce sub-bin freq\_norm accuracy; (3) sort descending and return the top_`n_peaks` _. freq\_norm is DC-centred: bin i maps to freq\_norm = (i - N/2) / N so DC (bin N/2) → 0.0 and the first negative frequency bin → −0.5. The spectrum must have at least 3 bins._ |
|  void | [**hann\_window**](#function-hann_window) (float \* w, size\_t w\_len) <br>_Fill_ `w` _with a Hann (raised-cosine) window. Computes w(k) = 0.5\*(1 - cos(2π k/(N-1))) for k = 0..N-1. The window tapers smoothly to zero at both endpoints, providing ~31 dB first-sidelobe rejection. Takes no shape parameter; use Kaiser for adjustable roll-off._ |
|  float | [**kaiser\_enbw**](#function-kaiser_enbw) (const float \* w, size\_t w\_len) <br>_Compute the equivalent noise bandwidth of a window in bins. ENBW = N \* sum(w²) / (sum(w))² quantifies how many noise bins the window smears into the main lobe. A rectangular window has ENBW = 1.0; tapered windows are &gt; 1.0. Works with any window type, not just Kaiser._  |
|  void | [**kaiser\_window**](#function-kaiser_window) (float \* w, size\_t w\_len, float beta) <br>_Fill_ `w` _with a Kaiser window of shape parameter_`beta` _. I0 is computed via the converging power-series expansion. Increasing_`beta` _raises sidelobe attenuation at the cost of a wider main lobe (beta=0 → rectangular, beta≈6 → ~60 dB sidelobe rejection). The output is normalised so that w[0] = w[N-1] = I0(0)/I0(beta)._ |
|  void | [**magnitude\_db\_cf32**](#function-magnitude_db_cf32) (const float complex \* x, size\_t x\_len, float \* out, float lin\_floor, float offset\_db) <br>_Convert a CF32 complex spectrum to F32 dB magnitudes. Computes out(k) = 20\*log10(max(\|x(k)\|, lin\_floor)) + offset\_db for each bin. The_ `lin_floor` _guard prevents log10(0); a value of 1e-12 corresponds to a -240 dB noise floor._`offset_db` _shifts the entire output for calibration (e.g., normalise to 0 dBFS)._ |
|  void | [**magnitude\_db\_cf64**](#function-magnitude_db_cf64) (const double complex \* x, size\_t x\_len, float \* out, double lin\_floor, float offset\_db) <br>_Convert a CF64 complex spectrum to F32 dB magnitudes. Double-precision variant of_ [_**magnitude\_db\_cf32()**_](spectral__core_8h.md#function-magnitude_db_cf32) _. Accepts a CF64 input array and a double_`lin_floor` _; output is still F32 because downstream display code typically works in single precision. The formula and_`offset_db` _semantics are identical._ |




























## Detailed Description


Provides windowing (Kaiser, Hann), ENBW computation, magnitude conversion, and peak finding. These are pure functions with no persistent state. 


    
## Public Functions Documentation




### function find\_peaks\_f32 

_Find up to_ `n_peaks` _local maxima in a DC-centred F32 dB spectrum. Three-step algorithm: (1) local-max scan — db[k] &gt; db[k-1] && db[k] &gt;= db[k+1] with db[k] &gt; min\_db; (2) parabolic interpolation on each local maximum to produce sub-bin freq\_norm accuracy; (3) sort descending and return the top_`n_peaks` _. freq\_norm is DC-centred: bin i maps to freq\_norm = (i - N/2) / N so DC (bin N/2) → 0.0 and the first negative frequency bin → −0.5. The spectrum must have at least 3 bins._
```C++
size_t find_peaks_f32 (
    const float * db,
    size_t db_len,
    size_t n_peaks,
    float min_db,
    dp_peak_t * result
) 
```





**Parameters:**


* `db` F32 dB spectrum, DC-centred, length &gt;= 3. 
* `db_len` Number of elements in `db`. 
* `n_peaks` Maximum number of peaks to return. 
* `min_db` Amplitude gate; local maxima below this are discarded. 
* `result` Caller-supplied [**dp\_peak\_t**](structdp__peak__t.md) array of length &gt;= `n_peaks`; filled with up to `n_peaks` results sorted descending. 



**Returns:**

Number of [**dp\_peak\_t**](structdp__peak__t.md) entries written to `result`. 
```C++
>>> from doppler.spectral import find_peaks_f32
>>> import numpy as np
>>> db = np.full(32, -60.0, dtype=np.float32)
>>> db[7] = -15.0; db[8] = -10.0; db[9] = -15.0
>>> peaks = find_peaks_f32(db, 2, -30.0)
>>> peaks
[(-0.25, -10.0)]
```
 





        

<hr>



### function hann\_window 

_Fill_ `w` _with a Hann (raised-cosine) window. Computes w(k) = 0.5\*(1 - cos(2π k/(N-1))) for k = 0..N-1. The window tapers smoothly to zero at both endpoints, providing ~31 dB first-sidelobe rejection. Takes no shape parameter; use Kaiser for adjustable roll-off._
```C++
void hann_window (
    float * w,
    size_t w_len
) 
```





**Parameters:**


* `w` Output buffer modified in-place; must be length &gt;= 1. 
* `w_len` Number of elements in `w`. 
```C++
>>> from doppler.spectral import hann_window
>>> import numpy as np
>>> w = np.zeros(8, dtype=np.float32)
>>> hann_window(w)
>>> [round(v, 4) for v in w.tolist()]
[0.0, 0.1883, 0.6113, 0.9505, 0.9505, 0.6113, 0.1883, 0.0]
```
 




        

<hr>



### function kaiser\_enbw 

_Compute the equivalent noise bandwidth of a window in bins. ENBW = N \* sum(w²) / (sum(w))² quantifies how many noise bins the window smears into the main lobe. A rectangular window has ENBW = 1.0; tapered windows are &gt; 1.0. Works with any window type, not just Kaiser._ 
```C++
float kaiser_enbw (
    const float * w,
    size_t w_len
) 
```





**Parameters:**


* `w` Float32 window coefficients array; any length &gt;= 1. 
* `w_len` Number of elements in `w`. 



**Returns:**

ENBW in bins (dimensionless). 
```C++
>>> from doppler.spectral import kaiser_enbw, hann_window
>>> import numpy as np
>>> w = np.zeros(8, dtype=np.float32)
>>> hann_window(w)
>>> round(kaiser_enbw(w), 4)
1.7143
```
 





        

<hr>



### function kaiser\_window 

_Fill_ `w` _with a Kaiser window of shape parameter_`beta` _. I0 is computed via the converging power-series expansion. Increasing_`beta` _raises sidelobe attenuation at the cost of a wider main lobe (beta=0 → rectangular, beta≈6 → ~60 dB sidelobe rejection). The output is normalised so that w[0] = w[N-1] = I0(0)/I0(beta)._
```C++
void kaiser_window (
    float * w,
    size_t w_len,
    float beta
) 
```





**Parameters:**


* `w` Output buffer modified in-place; must be length &gt;= 1. 
* `w_len` Number of elements in `w`. 
* `beta` Window shape parameter (float, &gt;= 0). 
```C++
>>> from doppler.spectral import kaiser_window
>>> import numpy as np
>>> w = np.zeros(8, dtype=np.float32)
>>> kaiser_window(w, 6.0)
>>> [round(v, 4) for v in w.tolist()]
[0.0149, 0.1998, 0.5913, 0.9454, 0.9454, 0.5913, 0.1998, 0.0149]
```
 




        

<hr>



### function magnitude\_db\_cf32 

_Convert a CF32 complex spectrum to F32 dB magnitudes. Computes out(k) = 20\*log10(max(\|x(k)\|, lin\_floor)) + offset\_db for each bin. The_ `lin_floor` _guard prevents log10(0); a value of 1e-12 corresponds to a -240 dB noise floor._`offset_db` _shifts the entire output for calibration (e.g., normalise to 0 dBFS)._
```C++
void magnitude_db_cf32 (
    const float complex * x,
    size_t x_len,
    float * out,
    float lin_floor,
    float offset_db
) 
```





**Parameters:**


* `x` CF32 complex spectrum array, length `x_len`. 
* `x_len` Number of elements in `x`. 
* `out` Output F32 buffer, length &gt;= `x_len`; caller-allocated. 
* `lin_floor` Linear amplitude floor (must be &gt; 0, e.g. 1e-12). 
* `offset_db` Calibration offset added to every output bin. 
```C++
>>> from doppler.spectral import magnitude_db_cf32
>>> import numpy as np
>>> x = np.array([1+0j, 0.1+0j, 0+0j], dtype=np.complex64)
>>> magnitude_db_cf32(x, 1e-12, 0.0).tolist()
[0.0, -20.0, -240.0]
```
 




        

<hr>



### function magnitude\_db\_cf64 

_Convert a CF64 complex spectrum to F32 dB magnitudes. Double-precision variant of_ [_**magnitude\_db\_cf32()**_](spectral__core_8h.md#function-magnitude_db_cf32) _. Accepts a CF64 input array and a double_`lin_floor` _; output is still F32 because downstream display code typically works in single precision. The formula and_`offset_db` _semantics are identical._
```C++
void magnitude_db_cf64 (
    const double complex * x,
    size_t x_len,
    float * out,
    double lin_floor,
    float offset_db
) 
```





**Parameters:**


* `x` CF64 complex spectrum array, length `x_len`. 
* `x_len` Number of elements in `x`. 
* `out` Output F32 buffer, length &gt;= `x_len`; caller-allocated. 
* `lin_floor` Linear amplitude floor (double, must be &gt; 0). 
* `offset_db` Calibration offset added to every output bin. 
```C++
>>> from doppler.spectral import magnitude_db_cf64
>>> import numpy as np
>>> x = np.array([1+0j, 10+0j], dtype=np.complex128)
>>> magnitude_db_cf64(x, 1e-12, 0.0).tolist()
[0.0, 20.0]
```
 




        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/spectral/spectral_core.h`

