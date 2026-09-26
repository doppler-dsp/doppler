

# File nprmeas\_core.h



[**FileList**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**nprmeas**](dir_df189228e030028408da81bd0afea7e3.md) **>** [**nprmeas\_core.h**](nprmeas__core_8h.md)

[Go to the source code of this file](nprmeas__core_8h_source.md)

_NPRMeasure — notched-noise Noise Power Ratio._ [More...](#detailed-description)

* `#include "doppler/clib_common.h"`
* `#include "doppler/jm_perf.h"`
* `#include "doppler/measure/measure_core.h"`
* `#include "doppler/psd/psd_core.h"`
* `#include "doppler/dp_complex.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**dp\_nprmeas\_state\_t**](structdp__nprmeas__state__t.md) <br>_NPRMeasure state: owned window, FFT plan and one-sided power scratch._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**npr\_meas\_t**](structnpr__meas__t.md) | [**dp\_nprmeas\_analyze**](#function-dp_nprmeas_analyze) ([**dp\_nprmeas\_state\_t**](structdp__nprmeas__state__t.md) \* state, const float \* x, size\_t n\_in, double active\_lo, double active\_hi, double notch\_lo, double notch\_hi, double guard\_hz) <br>_NPR of a notched-noise capture._  |
|  [**dp\_nprmeas\_state\_t**](structdp__nprmeas__state__t.md) \* | [**dp\_nprmeas\_create**](#function-dp_nprmeas_create) (size\_t n, double fs, double full\_scale, size\_t bits, double dynamic\_range\_db) <br>_Create an NPRMeasure analyser (auto Kaiser window)._  |
|  void | [**dp\_nprmeas\_destroy**](#function-dp_nprmeas_destroy) ([**dp\_nprmeas\_state\_t**](structdp__nprmeas__state__t.md) \* state) <br>_Destroy an NPRMeasure analyser._  |
|  void | [**dp\_nprmeas\_reset**](#function-dp_nprmeas_reset) ([**dp\_nprmeas\_state\_t**](structdp__nprmeas__state__t.md) \* state) <br>_Reset the analyser (a no-op: each analyze() call is independent)._  |
|  size\_t | [**dp\_nprmeas\_spectrum\_dbfs**](#function-dp_nprmeas_spectrum_dbfs) ([**dp\_nprmeas\_state\_t**](structdp__nprmeas__state__t.md) \* state, const float \* x, size\_t x\_len, float \* out, size\_t max\_out) <br>_DC-centred dBFS magnitude spectrum of a capture (length nfft)._  |
|  size\_t | [**dp\_nprmeas\_spectrum\_dbfs\_max\_out**](#function-dp_nprmeas_spectrum_dbfs_max_out) ([**dp\_nprmeas\_state\_t**](structdp__nprmeas__state__t.md) \* state) <br>_Capacity (== nfft) of the spectrum\_dbfs output buffer._  |




























## Detailed Description


Drive the system with band-limited noise containing a deep notch; NPR is the ratio of the mean in-band noise PSD to the mean PSD that folds into the notch (distortion + quantisation + intermodulation). The band/notch geometry is an analyze() argument, so one estimator can sweep several notch placements.


Lifecycle: create -&gt; `[analyze]*` -&gt; destroy 


    
## Public Functions Documentation




### function dp\_nprmeas\_analyze 

_NPR of a notched-noise capture._ 
```C++
npr_meas_t dp_nprmeas_analyze (
    dp_nprmeas_state_t * state,
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
>>> F[(f < 0.05) | (f > 0.45)] = 0  # band-limit to [0.05,0.45]
>>> F[(f >= 0.20) & (f <= 0.25)] *= 10**(-50/20)   # notch 50 dB deep
>>> x = np.fft.irfft(F, n)
>>> x = (0.3*x/np.std(x)).astype(np.float32)
>>> r = NPRMeasure(n=n, fs=1.0).analyze(
...     x, 0.05, 0.45, 0.20, 0.25, 0.01)
>>> 45 < r.npr_db < 55, r.notch_psd_dbfs < r.inband_psd_dbfs
(True, True)
```
 


        

<hr>



### function dp\_nprmeas\_create 

_Create an NPRMeasure analyser (auto Kaiser window)._ 
```C++
dp_nprmeas_state_t * dp_nprmeas_create (
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



### function dp\_nprmeas\_destroy 

_Destroy an NPRMeasure analyser._ 
```C++
void dp_nprmeas_destroy (
    dp_nprmeas_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function dp\_nprmeas\_reset 

_Reset the analyser (a no-op: each analyze() call is independent)._ 
```C++
void dp_nprmeas_reset (
    dp_nprmeas_state_t * state
) 
```



Every analyze() / spectrum\_dbfs() call re-windows and re-transforms its own capture from scratch, so nothing is carried between calls to clear. The method exists only so NPRMeasure honours the same reset() contract as every other doppler object, letting a generic pipeline reset each stage uniformly.




**Parameters:**


* `state` The analyser (left unchanged).


```C++
>>> from doppler.measure import NPRMeasure
>>> m = NPRMeasure(n=8192, fs=1.0)
>>> m.reset()            # stateless: provided only for API uniformity
>>> m.reset() is None    # returns nothing; safe to call anytime
True
```
 


        

<hr>



### function dp\_nprmeas\_spectrum\_dbfs 

_DC-centred dBFS magnitude spectrum of a capture (length nfft)._ 
```C++
size_t dp_nprmeas_spectrum_dbfs (
    dp_nprmeas_state_t * state,
    const float * x,
    size_t x_len,
    float * out,
    size_t max_out
) 
```



The same windowed, zero-padded PSD the NPR metrics are read off, laid out DC-centred (fftshifted) and normalised to dBFS for an analyzer-display backdrop. Use it to see the notch and the active band that analyze() integrates over.




**Parameters:**


* `state` The analyser. 
* `x` Real time-domain capture (length `x_len`). 
* `x_len` Number of input samples. 
* `out` Destination buffer (length &gt;= `max_out`). 
* `max_out` Capacity of `out` (== nfft). 



**Returns:**

DC-centred dBFS magnitude spectrum, one value per FFT bin (nfft).



```C++
>>> from doppler.measure import NPRMeasure
>>> import numpy as np
>>> rng = np.random.default_rng(0)
>>> x = (0.3*rng.standard_normal(8192)).astype(np.float32)  # noise
>>> s = NPRMeasure(n=8192, fs=1.0).spectrum_dbfs(x)  # DC-centred dBFS
>>> s.shape                                          # zero-padded nfft
(16384,)
>>> round(float(np.median(s)), 0)   # broadband floor, below 0 dBFS
-48.0
```
 


        

<hr>



### function dp\_nprmeas\_spectrum\_dbfs\_max\_out 

_Capacity (== nfft) of the spectrum\_dbfs output buffer._ 
```C++
size_t dp_nprmeas_spectrum_dbfs_max_out (
    dp_nprmeas_state_t * state
) 
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/nprmeas/nprmeas_core.h`

