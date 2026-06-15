

# File nprmeas\_core.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**nprmeas**](dir_2ffe7a00bca5d7665b823d0b8c1040c3.md) **>** [**nprmeas\_core.h**](nprmeas__core_8h.md)

[Go to the source code of this file](nprmeas__core_8h_source.md)

_NPRMeasure — notched-noise Noise Power Ratio._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "jm_perf.h"`
* `#include "measure/measure_core.h"`
* `#include "welch/welch_core.h"`
* `#include <complex.h>`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**nprmeas\_state\_t**](structnprmeas__state__t.md) <br>_NPRMeasure state: owned window, FFT plan and one-sided power scratch._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**npr\_meas\_t**](structnpr__meas__t.md) | [**nprmeas\_analyze**](#function-nprmeas_analyze) ([**nprmeas\_state\_t**](structnprmeas__state__t.md) \* state, const float \* x, size\_t n\_in, double active\_lo, double active\_hi, double notch\_lo, double notch\_hi, double guard\_hz) <br>_NPR of a notched-noise capture._  |
|  [**nprmeas\_state\_t**](structnprmeas__state__t.md) \* | [**nprmeas\_create**](#function-nprmeas_create) (size\_t n, double fs, int window, float beta, size\_t pad, double full\_scale) <br>_Create an NPRMeasure analyser._  |
|  void | [**nprmeas\_destroy**](#function-nprmeas_destroy) ([**nprmeas\_state\_t**](structnprmeas__state__t.md) \* state) <br>_Destroy an NPRMeasure analyser._  |
|  void | [**nprmeas\_reset**](#function-nprmeas_reset) ([**nprmeas\_state\_t**](structnprmeas__state__t.md) \* state) <br>_Reset (no-op: each analyze() call is independent)._  |




























## Detailed Description


Drive the system with band-limited noise containing a deep notch; NPR is the ratio of the mean in-band noise PSD to the mean PSD that folds into the notch (distortion + quantisation + intermodulation). The band/notch geometry is an analyze() argument, so one estimator can sweep several notch placements.


Lifecycle: create -&gt; [analyze]\* -&gt; destroy 


    
## Public Functions Documentation




### function nprmeas\_analyze 

_NPR of a notched-noise capture._ 
```C++
npr_meas_t nprmeas_analyze (
    nprmeas_state_t * state,
    const float * x,
    size_t n_in,
    double active_lo,
    double active_hi,
    double notch_lo,
    double notch_hi,
    double guard_hz
) 
```





**Parameters:**


* `state` The analyser. 
* `x` Real time-domain capture. 
* `n_in` Number of input samples. 
* `active_lo` Active noise band lower edge (Hz). 
* `active_hi` Active noise band upper edge (Hz). 
* `notch_lo` Notch lower edge (Hz). 
* `notch_hi` Notch upper edge (Hz). 
* `guard_hz` Keep-out around the notch edges (Hz). 



**Returns:**

the NPR metric record (by value). 





        

<hr>



### function nprmeas\_create 

_Create an NPRMeasure analyser._ 
```C++
nprmeas_state_t * nprmeas_create (
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



### function nprmeas\_destroy 

_Destroy an NPRMeasure analyser._ 
```C++
void nprmeas_destroy (
    nprmeas_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function nprmeas\_reset 

_Reset (no-op: each analyze() call is independent)._ 
```C++
void nprmeas_reset (
    nprmeas_state_t * state
) 
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/nprmeas/nprmeas_core.h`

