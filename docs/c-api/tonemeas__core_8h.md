

# File tonemeas\_core.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**tonemeas**](dir_78c9bf326243d2be956f1c1b5de2ee56.md) **>** [**tonemeas\_core.h**](tonemeas__core_8h.md)

[Go to the source code of this file](tonemeas__core_8h_source.md)

_ToneMeasure — single-tone ADC/converter spectral measurement._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "jm_perf.h"`
* `#include "measure/measure_core.h"`
* `#include "fft/fft_core.h"`
* `#include <complex.h>`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**tonemeas\_state\_t**](structtonemeas__state__t.md) <br>_ToneMeasure state: owned window, FFT plan and analysis scratch._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  size\_t | [**tonemeas\_analyze**](#function-tonemeas_analyze) ([**tonemeas\_state\_t**](structtonemeas__state__t.md) \* state, const float \* x, size\_t n\_in, [**tone\_meas\_t**](structtone__meas__t.md) \* out, size\_t max\_out) <br>_Analyse a real capture into the single-tone metric bag._  |
|  size\_t | [**tonemeas\_analyze\_complex**](#function-tonemeas_analyze_complex) ([**tonemeas\_state\_t**](structtonemeas__state__t.md) \* state, const float complex \* x, size\_t n\_in, [**tone\_meas\_t**](structtone__meas__t.md) \* out, size\_t max\_out) <br>_Analyse a complex baseband capture (two-sided spectrum)._  |
|  [**tonemeas\_state\_t**](structtonemeas__state__t.md) \* | [**tonemeas\_create**](#function-tonemeas_create) (size\_t n, double fs, int window, float beta, size\_t pad, size\_t n\_harmonics, double full\_scale, size\_t dc\_guard) <br>_Create a ToneMeasure analyser._  |
|  void | [**tonemeas\_destroy**](#function-tonemeas_destroy) ([**tonemeas\_state\_t**](structtonemeas__state__t.md) \* state) <br>_Destroy a ToneMeasure analyser._  |
|  void | [**tonemeas\_reset**](#function-tonemeas_reset) ([**tonemeas\_state\_t**](structtonemeas__state__t.md) \* state) <br>_Reset (no-op: the analyser is stateless between calls)._  |
|  size\_t | [**tonemeas\_spectrum\_dbfs**](#function-tonemeas_spectrum_dbfs) ([**tonemeas\_state\_t**](structtonemeas__state__t.md) \* state, const float \* x, size\_t x\_len, float \* out) <br>_DC-centred dBFS magnitude spectrum of a real capture (length nfft)._  |
|  size\_t | [**tonemeas\_spectrum\_dbfs\_max\_out**](#function-tonemeas_spectrum_dbfs_max_out) ([**tonemeas\_state\_t**](structtonemeas__state__t.md) \* state) <br>_Capacity (== nfft) of the spectrum\_dbfs output buffer._  |
|  size\_t | [**tonemeas\_time\_stats**](#function-tonemeas_time_stats) ([**tonemeas\_state\_t**](structtonemeas__state__t.md) \* state, const float \* x, size\_t n\_in, [**time\_stats\_t**](structtime__stats__t.md) \* out, size\_t max\_out) <br>_Time-domain statistics of a real capture._  |




























## Detailed Description


Owns a window + zero-padded FFT and analyses one time-domain capture (real or complex) into the full single-tone metric bag ([**tone\_meas\_t**](structtone__meas__t.md)). Each component's power is integrated over its window MAIN LOBE and the noise sum excludes the leakage bins around DC, the fundamental and each harmonic — the IEEE Std 1241 method — so a full-scale tone reads ~0 dBFS regardless of where it lands between FFT bins.


Lifecycle: create -&gt; [analyze / analyze\_complex / time\_stats]\* -&gt; destroy



```C++
tonemeas_state_t *m = tonemeas_create(8192, 1.0, 1, 12.0f, 2, 8, 1.0, 0);
tone_meas_t r;
tonemeas_analyze(m, capture, 8192, &r, 1);   // r.enob, r.sfdr_dbc, ...
tonemeas_destroy(m);
```
 


    
## Public Functions Documentation




### function tonemeas\_analyze 

_Analyse a real capture into the single-tone metric bag._ 
```C++
size_t tonemeas_analyze (
    tonemeas_state_t * state,
    const float * x,
    size_t n_in,
    tone_meas_t * out,
    size_t max_out
) 
```





**Returns:**

1 (one result written to out[0]); 0 if max\_out == 0. 





        

<hr>



### function tonemeas\_analyze\_complex 

_Analyse a complex baseband capture (two-sided spectrum)._ 
```C++
size_t tonemeas_analyze_complex (
    tonemeas_state_t * state,
    const float complex * x,
    size_t n_in,
    tone_meas_t * out,
    size_t max_out
) 
```




<hr>



### function tonemeas\_create 

_Create a ToneMeasure analyser._ 
```C++
tonemeas_state_t * tonemeas_create (
    size_t n,
    double fs,
    int window,
    float beta,
    size_t pad,
    size_t n_harmonics,
    double full_scale,
    size_t dc_guard
) 
```





**Parameters:**


* `n` Capture/frame length (&gt;= 2). 
* `fs` Sample rate (Hz, &gt; 0). 
* `window` 0 = Hann, 1 = Kaiser. 
* `beta` Kaiser shape (ignored for Hann). 
* `pad` Zero-pad factor (&gt;= 1); nfft = next\_pow2(n\*pad). 
* `n_harmonics` Harmonics to track (k = 2..n\_harmonics). 
* `full_scale` Amplitude that equals 0 dBFS (&gt; 0). 
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
size_t tonemeas_time_stats (
    tonemeas_state_t * state,
    const float * x,
    size_t n_in,
    time_stats_t * out,
    size_t max_out
) 
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/tonemeas/tonemeas_core.h`

