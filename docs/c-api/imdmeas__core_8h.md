

# File imdmeas\_core.h



[**FileList**](files.md) **>** [**imdmeas**](dir_2f7e0f9e46c443ab8712f0318288e016.md) **>** [**imdmeas\_core.h**](imdmeas__core_8h.md)

[Go to the source code of this file](imdmeas__core_8h_source.md)

_IMDMeasure — two-tone intermodulation (IMD2/IMD3) and intercept._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "jm_perf.h"`
* `#include "measure/measure_core.h"`
* `#include "welch/welch_core.h"`
* `#include <complex.h>`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**imdmeas\_state\_t**](structimdmeas__state__t.md) <br>_IMDMeasure state: owned window, FFT plan and one-sided power scratch._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**imd\_meas\_t**](structimd__meas__t.md) | [**imdmeas\_analyze**](#function-imdmeas_analyze) ([**imdmeas\_state\_t**](structimdmeas__state__t.md) \* state, const float \* x, size\_t n\_in) <br>_Two-tone IMD/TOI of a real capture (finds the two strongest tones)._  |
|  [**imdmeas\_state\_t**](structimdmeas__state__t.md) \* | [**imdmeas\_create**](#function-imdmeas_create) (size\_t n, double fs, int window, float beta, size\_t pad, double full\_scale) <br>_Create an IMDMeasure analyser._  |
|  void | [**imdmeas\_destroy**](#function-imdmeas_destroy) ([**imdmeas\_state\_t**](structimdmeas__state__t.md) \* state) <br>_Destroy an IMDMeasure analyser._  |
|  void | [**imdmeas\_reset**](#function-imdmeas_reset) ([**imdmeas\_state\_t**](structimdmeas__state__t.md) \* state) <br>_Reset (no-op: each analyze() call is independent)._  |




























## Detailed Description


Drive two equal-amplitude tones f1&lt;f2; the analyser finds them as the two strongest lobes, integrates each fundamental and the intermodulation products (2f1-f2, 2f2-f1 for IMD3; f2-f1 for IMD2) over their window main lobes (folded into the analysed band), and reports the third/second-order intercepts.


Lifecycle: create -&gt; [analyze]\* -&gt; destroy 


    
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





        

<hr>



### function imdmeas\_create 

_Create an IMDMeasure analyser._ 
```C++
imdmeas_state_t * imdmeas_create (
    size_t n,
    double fs,
    int window,
    float beta,
    size_t pad,
    double full_scale
) 
```





**Parameters:**


* `n` Capture/frame length (&gt;= 2). 
* `fs` Sample rate (Hz, &gt; 0). 
* `window` 0 = Hann, 1 = Kaiser. 
* `beta` Kaiser shape (ignored for Hann). 
* `pad` Zero-pad factor (&gt;= 1); nfft = next\_pow2(n\*pad). 
* `full_scale` Amplitude that equals 0 dBFS (&gt; 0). 



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

------------------------------
The documentation for this class was generated from the following file `native/inc/imdmeas/imdmeas_core.h`

