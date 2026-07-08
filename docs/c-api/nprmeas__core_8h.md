

# File nprmeas\_core.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**nprmeas**](dir_2ffe7a00bca5d7665b823d0b8c1040c3.md) **>** [**nprmeas\_core.h**](nprmeas__core_8h.md)

[Go to the source code of this file](nprmeas__core_8h_source.md)

_NPRMeasure — notched-noise Noise Power Ratio._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "jm_perf.h"`
* `#include "measure/measure_core.h"`
* `#include "psd/psd_core.h"`
* `#include <complex.h>`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**nprmeas\_state\_t**](structnprmeas__state__t.md) <br>_NPRMeasure state: owned window, FFT plan and one-sided power scratch._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**npr\_meas\_t**](structnpr__meas__t.md) | [**nprmeas\_analyze**](#function-nprmeas_analyze) ([**nprmeas\_state\_t**](structnprmeas__state__t.md) \* state, const float \* x, size\_t n\_in, double active\_lo, double active\_hi, double notch\_lo, double notch\_hi, double guard\_hz) <br>_NPR of a notched-noise capture._  |
|  [**nprmeas\_state\_t**](structnprmeas__state__t.md) \* | [**nprmeas\_create**](#function-nprmeas_create) (size\_t n, double fs, double full\_scale, size\_t bits, double dynamic\_range\_db) <br>_Create an NPRMeasure analyser (auto Kaiser window)._  |
|  void | [**nprmeas\_destroy**](#function-nprmeas_destroy) ([**nprmeas\_state\_t**](structnprmeas__state__t.md) \* state) <br>_Destroy an NPRMeasure analyser._  |
|  void | [**nprmeas\_reset**](#function-nprmeas_reset) ([**nprmeas\_state\_t**](structnprmeas__state__t.md) \* state) <br>_Reset (no-op: each analyze() call is independent)._  |
|  size\_t | [**nprmeas\_spectrum\_dbfs**](#function-nprmeas_spectrum_dbfs) ([**nprmeas\_state\_t**](structnprmeas__state__t.md) \* state, const float \* x, size\_t x\_len, float \* out) <br>_DC-centred dBFS magnitude spectrum of a capture (length nfft). The same averaged PSD the metrics use, for an analyzer-display backdrop._  |
|  size\_t | [**nprmeas\_spectrum\_dbfs\_max\_out**](#function-nprmeas_spectrum_dbfs_max_out) ([**nprmeas\_state\_t**](structnprmeas__state__t.md) \* state) <br>_Capacity (== nfft) of the spectrum\_dbfs output buffer._  |




























## Detailed Description


Drive the system with band-limited noise containing a deep notch; NPR is the ratio of the mean in-band noise PSD to the mean PSD that folds into the notch (distortion + quantisation + intermodulation). The band/notch geometry is an analyze() argument, so one estimator can sweep several notch placements.


Lifecycle: create -&gt; `[analyze]*` -&gt; destroy 


    
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



```C++
>>> from doppler.measure import NPRMeasure
>>> import numpy as np
>>> rng = np.random.default_rng(0)
>>> n = 1 << 15
>>> F = np.fft.rfft(rng.standard_normal(n))
>>> f = np.fft.rfftfreq(n)
>>> F[(f < 0.05) | (f > 0.45)] = 0                 # band-limit to [0.05,0.45]
>>> F[(f >= 0.20) & (f <= 0.25)] *= 10**(-50/20)   # notch 50 dB deep
>>> x = np.fft.irfft(F, n)
>>> x = (0.3*x/np.std(x)).astype(np.float32)
>>> r = NPRMeasure(n=n, fs=1.0).analyze(x, 0.05, 0.45, 0.20, 0.25, 0.01)
>>> 45 < r.npr_db < 55, r.notch_psd_dbfs < r.inband_psd_dbfs
(True, True)
```
 


        

<hr>



### function nprmeas\_create 

_Create an NPRMeasure analyser (auto Kaiser window)._ 
```C++
nprmeas_state_t * nprmeas_create (
    size_t n,
    double fs,
    double full_scale,
    size_t bits,
    double dynamic_range_db
) 
```



The window is always Kaiser; its shape is auto-selected so the sidelobes sit below the requested dynamic range (see measure\_resolve\_dr()). The chosen window also sets a minimum notch keep-out so active-band noise cannot leak into the notch average through the window skirt.




**Parameters:**


* `n` Capture/frame length (&gt;= 2). 
* `fs` Sample rate (Hz, &gt; 0). 
* `full_scale` Amplitude that equals 0 dBFS (&gt; 0). Ignored if bits &gt; 0. 
* `bits` ADC depth: bits&gt;0 sets the 0-dBFS reference to 2^(bits-1) and, unless overridden, the dynamic-range target. 
* `dynamic_range_db` Explicit sidelobe/dynamic-range target (dB); used when &gt; 0, else derived from `bits`. 



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



### function nprmeas\_spectrum\_dbfs 

_DC-centred dBFS magnitude spectrum of a capture (length nfft). The same averaged PSD the metrics use, for an analyzer-display backdrop._ 
```C++
size_t nprmeas_spectrum_dbfs (
    nprmeas_state_t * state,
    const float * x,
    size_t x_len,
    float * out
) 
```




<hr>



### function nprmeas\_spectrum\_dbfs\_max\_out 

_Capacity (== nfft) of the spectrum\_dbfs output buffer._ 
```C++
size_t nprmeas_spectrum_dbfs_max_out (
    nprmeas_state_t * state
) 
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/nprmeas/nprmeas_core.h`

