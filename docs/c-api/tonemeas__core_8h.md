

# File tonemeas\_core.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**tonemeas**](dir_78c9bf326243d2be956f1c1b5de2ee56.md) **>** [**tonemeas\_core.h**](tonemeas__core_8h.md)

[Go to the source code of this file](tonemeas__core_8h_source.md)

_ToneMeasure — single-tone ADC/converter spectral measurement._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "jm_perf.h"`
* `#include "measure/measure_core.h"`
* `#include "psd/psd_core.h"`
* `#include <complex.h>`
* `#include "fft/fft_core.h"`
* `#include "spectral/spectral_core.h"`
* `#include "acc_trace/acc_trace_core.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**tonemeas\_state\_t**](structtonemeas__state__t.md) <br>_ToneMeasure state: owned window, FFT plan and analysis scratch._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**tone\_meas\_t**](structtone__meas__t.md) | [**tonemeas\_analyze**](#function-tonemeas_analyze) ([**tonemeas\_state\_t**](structtonemeas__state__t.md) \* state, const float \* x, size\_t n\_in) <br>_Analyse a real capture into the single-tone metric bag._  |
|  [**tone\_meas\_t**](structtone__meas__t.md) | [**tonemeas\_analyze\_complex**](#function-tonemeas_analyze_complex) ([**tonemeas\_state\_t**](structtonemeas__state__t.md) \* state, const float complex \* x, size\_t n\_in) <br>_Analyse a complex baseband capture (two-sided spectrum)._  |
|  [**tonemeas\_state\_t**](structtonemeas__state__t.md) \* | [**tonemeas\_create**](#function-tonemeas_create) (size\_t n, double fs, size\_t n\_harmonics, double full\_scale, size\_t bits, double dynamic\_range\_db, size\_t dc\_guard) <br>_Create a ToneMeasure analyser (auto Kaiser window)._  |
|  void | [**tonemeas\_destroy**](#function-tonemeas_destroy) ([**tonemeas\_state\_t**](structtonemeas__state__t.md) \* state) <br>_Destroy a ToneMeasure analyser._  |
|  void | [**tonemeas\_reset**](#function-tonemeas_reset) ([**tonemeas\_state\_t**](structtonemeas__state__t.md) \* state) <br>_Reset (no-op: the analyser is stateless between calls)._  |
|  size\_t | [**tonemeas\_spectrum\_dbfs**](#function-tonemeas_spectrum_dbfs) ([**tonemeas\_state\_t**](structtonemeas__state__t.md) \* state, const float \* x, size\_t x\_len, float \* out) <br>_DC-centred dBFS magnitude spectrum of a real capture (length nfft)._  |
|  size\_t | [**tonemeas\_spectrum\_dbfs\_max\_out**](#function-tonemeas_spectrum_dbfs_max_out) ([**tonemeas\_state\_t**](structtonemeas__state__t.md) \* state) <br>_Capacity (== nfft) of the spectrum\_dbfs output buffer._  |
|  [**time\_stats\_t**](structtime__stats__t.md) | [**tonemeas\_time\_stats**](#function-tonemeas_time_stats) ([**tonemeas\_state\_t**](structtonemeas__state__t.md) \* state, const float \* x, size\_t n\_in) <br>_Time-domain statistics of a real capture._  |




























## Detailed Description


Owns a window + zero-padded FFT and analyses one time-domain capture (real or complex) into the full single-tone metric bag ([**tone\_meas\_t**](structtone__meas__t.md)). Each component's power is integrated over its window MAIN LOBE and the noise sum excludes the leakage bins around DC, the fundamental and each harmonic — the IEEE Std 1241 method — so a full-scale tone reads ~0 dBFS regardless of where it lands between FFT bins.


Lifecycle: create -&gt; `[analyze / analyze_complex / time_stats]*` -&gt; destroy



```C++
// 16-bit ADC: window auto-picked for ~100 dB dynamic range.
tonemeas_state_t *m = tonemeas_create(8192, 1.0, 8, 1.0, 16, 0.0, 0);
tone_meas_t r = tonemeas_analyze(m, capture, 8192);  // r.enob, r.sfdr_dbc...
tonemeas_destroy(m);
```
 


    
## Public Functions Documentation




### function tonemeas\_analyze 

_Analyse a real capture into the single-tone metric bag._ 
```C++
tone_meas_t tonemeas_analyze (
    tonemeas_state_t * state,
    const float * x,
    size_t n_in
) 
```





**Returns:**

the metric record (by value).



```C++
>>> from doppler.measure import ToneMeasure
>>> import numpy as np
>>> n, t = 4096, np.arange(4096)
>>> # full-scale tone at 300 cycles + a 2nd harmonic 40 dB down
>>> x = (np.cos(2*np.pi*300*t/n)
...      + 0.01*np.cos(2*np.pi*600*t/n)).astype(np.float32)
>>> r = ToneMeasure(n=n, fs=1.0).analyze(x)
>>> type(r).__name__
'ToneMetrics'
>>> abs(r.fund_dbfs) < 0.1, round(r.thd, 1)   # 0 dBFS tone, THD -40 dBc
(True, -40.0)
```
 


        

<hr>



### function tonemeas\_analyze\_complex 

_Analyse a complex baseband capture (two-sided spectrum)._ 
```C++
tone_meas_t tonemeas_analyze_complex (
    tonemeas_state_t * state,
    const float complex * x,
    size_t n_in
) 
```




```C++
>>> from doppler.measure import ToneMeasure
>>> import numpy as np
>>> i = np.arange(4096)
>>> x = np.exp(2j*np.pi*137*i/4096).astype(np.complex64)
>>> r = ToneMeasure(n=4096, fs=1.0).analyze_complex(x)
>>> round(r.fund_freq, 4), abs(r.fund_dbfs) < 0.2
(0.0334, True)
```
 


        

<hr>



### function tonemeas\_create 

_Create a ToneMeasure analyser (auto Kaiser window)._ 
```C++
tonemeas_state_t * tonemeas_create (
    size_t n,
    double fs,
    size_t n_harmonics,
    double full_scale,
    size_t bits,
    double dynamic_range_db,
    size_t dc_guard
) 
```



The window is always Kaiser; its shape is chosen automatically so the sidelobes sit below the requested dynamic range (see measure\_resolve\_dr()), keeping the resolution bandwidth as fine as `n` allows. The realised RBW is reported back in every result ([**tone\_meas\_t::rbw\_hz**](structtone__meas__t.md#variable-rbw_hz)).




**Parameters:**


* `n` Capture/frame length (&gt;= 2). 
* `fs` Sample rate (Hz, &gt; 0). 
* `n_harmonics` Harmonics to track (k = 2..n\_harmonics). 
* `full_scale` Amplitude that equals 0 dBFS (&gt; 0). Ignored if bits &gt; 0. 
* `bits` ADC depth: bits&gt;0 sets the 0-dBFS reference to 2^(bits-1) and, unless overridden, the dynamic-range target (6.02\*bits + 1.76 + headroom). 
* `dynamic_range_db` Explicit sidelobe/dynamic-range target (dB); used when &gt; 0, else derived from `bits` (or a deep default when both are 0). 
* `dc_guard` Extra bins excluded beyond L around DC. 



**Returns:**

Heap state, or NULL on bad args / allocation failure. 




**Note:**

Caller must [**tonemeas\_destroy()**](tonemeas__core_8h.md#function-tonemeas_destroy) when done. 





        

<hr>



### function tonemeas\_destroy 

_Destroy a ToneMeasure analyser._ 
```C++
void tonemeas_destroy (
    tonemeas_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function tonemeas\_reset 

_Reset (no-op: the analyser is stateless between calls)._ 
```C++
void tonemeas_reset (
    tonemeas_state_t * state
) 
```




<hr>



### function tonemeas\_spectrum\_dbfs 

_DC-centred dBFS magnitude spectrum of a real capture (length nfft)._ 
```C++
size_t tonemeas_spectrum_dbfs (
    tonemeas_state_t * state,
    const float * x,
    size_t x_len,
    float * out
) 
```





**Returns:**

Number of samples written (nfft). 





        

<hr>



### function tonemeas\_spectrum\_dbfs\_max\_out 

_Capacity (== nfft) of the spectrum\_dbfs output buffer._ 
```C++
size_t tonemeas_spectrum_dbfs_max_out (
    tonemeas_state_t * state
) 
```




<hr>



### function tonemeas\_time\_stats 

_Time-domain statistics of a real capture._ 
```C++
time_stats_t tonemeas_time_stats (
    tonemeas_state_t * state,
    const float * x,
    size_t n_in
) 
```




```C++
>>> from doppler.measure import ToneMeasure
>>> import numpy as np
>>> t = np.arange(4096)
>>> x = (0.8*np.cos(2*np.pi*50*t/4096)).astype(np.float32)
>>> ts = ToneMeasure(n=4096, fs=1.0).time_stats(x)
>>> round(ts.crest_db, 2), round(ts.fs_util_pct, 0)   # sine crest ~3.01 dB
(3.01, 80.0)
```
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/tonemeas/tonemeas_core.h`

