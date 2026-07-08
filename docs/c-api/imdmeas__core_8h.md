

# File imdmeas\_core.h



[**FileList**](files.md) **>** [**imdmeas**](dir_2f7e0f9e46c443ab8712f0318288e016.md) **>** [**imdmeas\_core.h**](imdmeas__core_8h.md)

[Go to the source code of this file](imdmeas__core_8h_source.md)

_IMDMeasure — two-tone intermodulation (IMD2/IMD3) and intercept._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "jm_perf.h"`
* `#include "measure/measure_core.h"`
* `#include "psd/psd_core.h"`
* `#include <complex.h>`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**imdmeas\_state\_t**](structimdmeas__state__t.md) <br>_IMDMeasure state: owned window, FFT plan and one-sided power scratch._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**imd\_meas\_t**](structimd__meas__t.md) | [**imdmeas\_analyze**](#function-imdmeas_analyze) ([**imdmeas\_state\_t**](structimdmeas__state__t.md) \* state, const float \* x, size\_t n\_in) <br>_Two-tone IMD/TOI of a real capture (finds the two strongest tones)._  |
|  [**imdmeas\_state\_t**](structimdmeas__state__t.md) \* | [**imdmeas\_create**](#function-imdmeas_create) (size\_t n, double fs, double full\_scale, size\_t bits, double dynamic\_range\_db) <br>_Create an IMDMeasure analyser (auto Kaiser window)._  |
|  void | [**imdmeas\_destroy**](#function-imdmeas_destroy) ([**imdmeas\_state\_t**](structimdmeas__state__t.md) \* state) <br>_Destroy an IMDMeasure analyser._  |
|  void | [**imdmeas\_reset**](#function-imdmeas_reset) ([**imdmeas\_state\_t**](structimdmeas__state__t.md) \* state) <br>_Reset (no-op: each analyze() call is independent)._  |
|  size\_t | [**imdmeas\_spectrum\_dbfs**](#function-imdmeas_spectrum_dbfs) ([**imdmeas\_state\_t**](structimdmeas__state__t.md) \* state, const float \* x, size\_t x\_len, float \* out) <br>_DC-centred dBFS magnitude spectrum of a capture (length nfft). The same averaged PSD the metrics use, for an analyzer-display backdrop._  |
|  size\_t | [**imdmeas\_spectrum\_dbfs\_max\_out**](#function-imdmeas_spectrum_dbfs_max_out) ([**imdmeas\_state\_t**](structimdmeas__state__t.md) \* state) <br>_Capacity (== nfft) of the spectrum\_dbfs output buffer._  |




























## Detailed Description


Drive two equal-amplitude tones f1&lt;f2; the analyser finds them as the two strongest lobes, integrates each fundamental and the intermodulation products (2f1-f2, 2f2-f1 for IMD3; f2-f1 for IMD2) over their window main lobes (folded into the analysed band), and reports the third/second-order intercepts.


Lifecycle: create -&gt; `[analyze]*` -&gt; destroy 


    
## Public Functions Documentation




### function imdmeas\_analyze 

_Two-tone IMD/TOI of a real capture (finds the two strongest tones)._ 
```C++
imd_meas_t imdmeas_analyze (
    imdmeas_state_t * state,
    const float * x,
    size_t n_in
) 
```





**Returns:**

the IMD metric record (by value; zeroed if no two tones are found).



```C++
>>> from doppler.measure import IMDMeasure
>>> import numpy as np
>>> t = np.arange(4096)
>>> # two equal tones at 200 & 250 cycles + 3rd-order products 40 dB down
>>> x = (np.cos(2*np.pi*200*t/4096) + np.cos(2*np.pi*250*t/4096)
...      + 0.01*np.cos(2*np.pi*150*t/4096)
...      + 0.01*np.cos(2*np.pi*300*t/4096)).astype(np.float32)
>>> r = IMDMeasure(n=4096, fs=1.0).analyze(x)
>>> round(r.f1, 4), round(r.f2, 4), round(r.imd3_dbc, 0)
(0.0488, 0.061, -40.0)
```
 


        

<hr>



### function imdmeas\_create 

_Create an IMDMeasure analyser (auto Kaiser window)._ 
```C++
imdmeas_state_t * imdmeas_create (
    size_t n,
    double fs,
    double full_scale,
    size_t bits,
    double dynamic_range_db
) 
```



The window is always Kaiser; its shape is auto-selected so the sidelobes sit below the requested dynamic range (see measure\_resolve\_dr()), keeping the resolution bandwidth as fine as `n` allows.




**Parameters:**


* `n` Capture/frame length (&gt;= 2). 
* `fs` Sample rate (Hz, &gt; 0). 
* `full_scale` Amplitude that equals 0 dBFS (&gt; 0). Ignored if bits &gt; 0. 
* `bits` ADC depth: bits&gt;0 sets the 0-dBFS reference to 2^(bits-1) and, unless overridden, the dynamic-range target. 
* `dynamic_range_db` Explicit sidelobe/dynamic-range target (dB); used when &gt; 0, else derived from `bits`. 



**Returns:**

Heap state, or NULL on bad args / allocation failure. 





        

<hr>



### function imdmeas\_destroy 

_Destroy an IMDMeasure analyser._ 
```C++
void imdmeas_destroy (
    imdmeas_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function imdmeas\_reset 

_Reset (no-op: each analyze() call is independent)._ 
```C++
void imdmeas_reset (
    imdmeas_state_t * state
) 
```




<hr>



### function imdmeas\_spectrum\_dbfs 

_DC-centred dBFS magnitude spectrum of a capture (length nfft). The same averaged PSD the metrics use, for an analyzer-display backdrop._ 
```C++
size_t imdmeas_spectrum_dbfs (
    imdmeas_state_t * state,
    const float * x,
    size_t x_len,
    float * out
) 
```




<hr>



### function imdmeas\_spectrum\_dbfs\_max\_out 

_Capacity (== nfft) of the spectrum\_dbfs output buffer._ 
```C++
size_t imdmeas_spectrum_dbfs_max_out (
    imdmeas_state_t * state
) 
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/imdmeas/imdmeas_core.h`

