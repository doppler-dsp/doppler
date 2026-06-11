

# File welch\_core.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**welch**](dir_aeb9e26b0edb1fd5fc61c8cd35fcdcfb.md) **>** [**welch\_core.h**](welch__core_8h.md)

[Go to the source code of this file](welch__core_8h_source.md)

_Welch — averaging PSD estimator and spectral measurement suite._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "jm_perf.h"`
* `#include "fft/fft_core.h"`
* `#include "acc_trace/acc_trace_core.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**welch\_state\_t**](structwelch__state__t.md) <br>_Welch state. Allocate with_ [_**welch\_create()**_](welch__core_8h.md#function-welch_create) _._ |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  void | [**welch\_accumulate**](#function-welch_accumulate) ([**welch\_state\_t**](structwelch__state__t.md) \* state, const float complex \* x, size\_t x\_len) <br>_Window, FFT and fold complex baseband frames into the average. Processes floor(n\_in / n) full frames; a trailing partial frame is ignored._  |
|  size\_t | [**welch\_band\_power**](#function-welch_band_power) ([**welch\_state\_t**](structwelch__state__t.md) \* state, const double \* bands, size\_t bands\_len, float \* out) <br>_Integrated power per band in dB._ `bands` _is a flat array of [lo0, hi0, lo1, hi1, ...] band edges in Hz; the output holds one dB value per band (n\_bands = bands\_len / 2). Edges are clamped to the analysed span; a band fully outside the span integrates to the dB floor. Returns 0 before any frame is accumulated._ |
|  size\_t | [**welch\_band\_power\_max\_out**](#function-welch_band_power_max_out) ([**welch\_state\_t**](structwelch__state__t.md) \* state) <br>_Output capacity hint for band\_power(); 0 (binding sizes from bands)._  |
|  [**welch\_state\_t**](structwelch__state__t.md) \* | [**welch\_create**](#function-welch_create) (size\_t n, double fs, int window, float beta, int mode, double alpha) <br>_Create an averaging PSD estimator._  |
|  void | [**welch\_destroy**](#function-welch_destroy) ([**welch\_state\_t**](structwelch__state__t.md) \* state) <br>_Destroy a Welch instance and release all memory._  |
|  double | [**welch\_noise\_floor**](#function-welch_noise_floor) ([**welch\_state\_t**](structwelch__state__t.md) \* state) <br>_Noise-floor estimate: median of the averaged dB spectrum._  |
|  double | [**welch\_occupied\_bw**](#function-welch_occupied_bw) ([**welch\_state\_t**](structwelch__state__t.md) \* state, double fraction) <br>_Occupied bandwidth in Hz holding_ `fraction` _of the total power._ |
|  size\_t | [**welch\_psd\_db**](#function-welch_psd_db) ([**welch\_state\_t**](structwelch__state__t.md) \* state, size\_t n, float \* out) <br>_Averaged power spectrum in dB, DC-centred. Normalised by window coherent gain so a full-scale tone reads its true power (a unit-amplitude tone peaks near 0 dB). Returns 0 (Python None) before any frame is accumulated._  |
|  size\_t | [**welch\_psd\_db\_max\_out**](#function-welch_psd_db_max_out) ([**welch\_state\_t**](structwelch__state__t.md) \* state) <br>_Output capacity hint for psd\_db(); equals n._  |
|  size\_t | [**welch\_psd\_dbhz**](#function-welch_psd_dbhz) ([**welch\_state\_t**](structwelch__state__t.md) \* state, size\_t n, float \* out) <br>_Averaged power spectral density in dB/Hz, DC-centred. Normalised by_ `fs * sum(w^2)` _(ENBW-aware), the standard one-sided-free PSD scaling. Differs from psd\_db() by the constant_`10*log10(cg^2/(fs*s2))` _. Returns 0 (Python None) before any frame is accumulated._ |
|  size\_t | [**welch\_psd\_dbhz\_max\_out**](#function-welch_psd_dbhz_max_out) ([**welch\_state\_t**](structwelch__state__t.md) \* state) <br>_Output capacity hint for psd\_dbhz(); equals n._  |
|  void | [**welch\_reset**](#function-welch_reset) ([**welch\_state\_t**](structwelch__state__t.md) \* state) <br>_Discard the running average; the next accumulate re-seeds it._  |
|  double | [**welch\_sfdr**](#function-welch_sfdr) ([**welch\_state\_t**](structwelch__state__t.md) \* state, float min\_db) <br>_Spurious-free dynamic range in dB from the two strongest peaks._  |
|  double | [**welch\_snr**](#function-welch_snr) ([**welch\_state\_t**](structwelch__state__t.md) \* state, double lo\_hz, double hi\_hz) <br>_In-band SNR in dB: peak level in [lo\_hz, hi\_hz] minus the noise floor._  |
|  double | [**welch\_total\_band\_power**](#function-welch_total_band_power) ([**welch\_state\_t**](structwelch__state__t.md) \* state, const double \* bands, size\_t bands\_len) <br>_Total integrated power across all bands in dB._  |




























## Detailed Description


A stateful, C-first periodogram averager. It composes the existing pieces of the library rather than re-implementing them: an [**fft\_state\_t**](structfft__state__t.md) forward plan, a spectral window (Hann or Kaiser) with its coherent gain and ENBW, an [**acc\_trace\_state\_t**](structacc__trace__state__t.md) per-bin power averager (mean / EMA / max-hold / min-hold), and the spectral free functions ([**magnitude\_db\_cf32**](spectral__core_8h.md#function-magnitude_db_cf32), [**find\_peaks\_f32**](spectral__core_8h.md#function-find_peaks_f32), [**obw\_from\_power**](spectral__core_8h.md#function-obw_from_power), [**noise\_floor\_db**](spectral__core_8h.md#function-noise_floor_db)) for the derived measurements.


Feed complex baseband frames with [**welch\_accumulate()**](welch__core_8h.md#function-welch_accumulate); each length-n frame is windowed, FFT'd, converted to power, fftshifted to DC-centred order and folded into the running average. Then read:
* [**welch\_psd\_db()**](welch__core_8h.md#function-welch_psd_db) : averaged power spectrum, dB (peak reads tone power)
* [**welch\_psd\_dbhz()**](welch__core_8h.md#function-welch_psd_dbhz) : averaged PSD, dB/Hz (ENBW / fs normalised)
* [**welch\_band\_power()**](welch__core_8h.md#function-welch_band_power) / [**welch\_total\_band\_power()**](welch__core_8h.md#function-welch_total_band_power) : integrated band power, dB
* [**welch\_occupied\_bw()**](welch__core_8h.md#function-welch_occupied_bw) : occupied bandwidth, Hz
* [**welch\_noise\_floor()**](welch__core_8h.md#function-welch_noise_floor) / [**welch\_snr()**](welch__core_8h.md#function-welch_snr) / [**welch\_sfdr()**](welch__core_8h.md#function-welch_sfdr) : level statistics, dB




All spectra are DC-centred (fftshift), matching find\_peaks\_f32's bin -&gt; frequency convention (bin i maps to (i - n/2)/n in normalised frequency, so spectral peaks are obtained idiomatically with `find_peaks_f32 (w.psd_db(), n_peaks, min_db)`).


Lifecycle: create -&gt; (accumulate / reset)\* -&gt; (measurement getters)\* -&gt; destroy 


    
## Public Functions Documentation




### function welch\_accumulate 

_Window, FFT and fold complex baseband frames into the average. Processes floor(n\_in / n) full frames; a trailing partial frame is ignored._ 
```C++
void welch_accumulate (
    welch_state_t * state,
    const float complex * x,
    size_t x_len
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `x` Complex baseband samples (cf32). 
* `x_len` Number of samples in `x`.


```C++
>>> import numpy as np
>>> from doppler.spectral import Welch
>>> n = 64
>>> w = Welch(n=n, fs=1.0, window="hann", mode="mean")
>>> k = 8
>>> x = np.exp(2j*np.pi*k*np.arange(n)/n).astype(np.complex64)
>>> for _ in range(4):
...     w.accumulate(x)
>>> psd = w.psd_db()
>>> psd.shape
(64,)
>>> int(np.argmax(psd)) == n // 2 + k
True
>>> w.count
4
```
 


        

<hr>



### function welch\_band\_power 

_Integrated power per band in dB._ `bands` _is a flat array of [lo0, hi0, lo1, hi1, ...] band edges in Hz; the output holds one dB value per band (n\_bands = bands\_len / 2). Edges are clamped to the analysed span; a band fully outside the span integrates to the dB floor. Returns 0 before any frame is accumulated._
```C++
size_t welch_band_power (
    welch_state_t * state,
    const double * bands,
    size_t bands_len,
    float * out
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `bands` Flat [lo,hi,...] band edges, Hz. 
* `bands_len` Number of edge values (2 \* n\_bands). 
* `out` Destination, at least n\_bands float32 elements. 



**Returns:**

n\_bands, or 0 if empty.



```C++
>>> import numpy as np
>>> from doppler.spectral import Welch
>>> w = Welch(n=64, fs=1.0, window="hann", mode="mean")
>>> w.accumulate(np.ones(64, dtype=np.complex64))
>>> pb = w.band_power(np.array([-0.5, 0.0, 0.0, 0.5]))
>>> pb.shape
(2,)
```
 


        

<hr>



### function welch\_band\_power\_max\_out 

_Output capacity hint for band\_power(); 0 (binding sizes from bands)._ 
```C++
size_t welch_band_power_max_out (
    welch_state_t * state
) 
```




<hr>



### function welch\_create 

_Create an averaging PSD estimator._ 
```C++
welch_state_t * welch_create (
    size_t n,
    double fs,
    int window,
    float beta,
    int mode,
    double alpha
) 
```





**Parameters:**


* `n` FFT / frame length in samples. Must be &gt;= 2. 
* `fs` Sample rate in Hz (used for dB/Hz and band frequencies). 
* `window` Window index: 0 = Hann, 1 = Kaiser. 
* `beta` Kaiser beta (ignored for Hann). 
* `mode` Averaging mode index (0=mean, 1=exp, 2=maxhold, 3=minhold). 
* `alpha` EMA smoothing factor (exp mode only). 



**Returns:**

Heap-allocated state, or NULL on invalid argument or OOM. 




**Note:**

Caller must call [**welch\_destroy()**](welch__core_8h.md#function-welch_destroy) when done.



```C++
>>> from doppler.spectral import Welch
>>> w = Welch(n=1024, fs=1.0e6, window="kaiser", beta=8.0, mode="mean")
>>> w.n, w.fs
(1024, 1000000.0)
>>> round(w.rbw / (w.fs / w.n), 3) == round(w.enbw, 3)
True
```
 


        

<hr>



### function welch\_destroy 

_Destroy a Welch instance and release all memory._ 
```C++
void welch_destroy (
    welch_state_t * state
) 
```





**Parameters:**


* `state` May be NULL (no-op). 




        

<hr>



### function welch\_noise\_floor 

_Noise-floor estimate: median of the averaged dB spectrum._ 
```C++
double welch_noise_floor (
    welch_state_t * state
) 
```





**Returns:**

Median dB level (0 if empty). 





        

<hr>



### function welch\_occupied\_bw 

_Occupied bandwidth in Hz holding_ `fraction` _of the total power._
```C++
double welch_occupied_bw (
    welch_state_t * state,
    double fraction
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `fraction` Power fraction in (0, 1], e.g. 0.99. 



**Returns:**

Occupied bandwidth in Hz (0 if empty or no power). 





        

<hr>



### function welch\_psd\_db 

_Averaged power spectrum in dB, DC-centred. Normalised by window coherent gain so a full-scale tone reads its true power (a unit-amplitude tone peaks near 0 dB). Returns 0 (Python None) before any frame is accumulated._ 
```C++
size_t welch_psd_db (
    welch_state_t * state,
    size_t n,
    float * out
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `n` Caller buffer capacity (ignored; buffer is pre-sized to n). 
* `out` Destination, at least n float32 elements. 



**Returns:**

n, or 0 if empty. 





        

<hr>



### function welch\_psd\_db\_max\_out 

_Output capacity hint for psd\_db(); equals n._ 
```C++
size_t welch_psd_db_max_out (
    welch_state_t * state
) 
```




<hr>



### function welch\_psd\_dbhz 

_Averaged power spectral density in dB/Hz, DC-centred. Normalised by_ `fs * sum(w^2)` _(ENBW-aware), the standard one-sided-free PSD scaling. Differs from psd\_db() by the constant_`10*log10(cg^2/(fs*s2))` _. Returns 0 (Python None) before any frame is accumulated._
```C++
size_t welch_psd_dbhz (
    welch_state_t * state,
    size_t n,
    float * out
) 
```




```C++
>>> import numpy as np
>>> from doppler.spectral import Welch
>>> w = Welch(n=32, fs=2.0, window="hann", mode="mean")
>>> w.accumulate(np.ones(32, dtype=np.complex64))
>>> a = w.psd_db(); b = w.psd_dbhz()
>>> bool(np.allclose(a - b, (a - b)[0]))   # offset is a constant
True
```
 


        

<hr>



### function welch\_psd\_dbhz\_max\_out 

_Output capacity hint for psd\_dbhz(); equals n._ 
```C++
size_t welch_psd_dbhz_max_out (
    welch_state_t * state
) 
```




<hr>



### function welch\_reset 

_Discard the running average; the next accumulate re-seeds it._ 
```C++
void welch_reset (
    welch_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 




        

<hr>



### function welch\_sfdr 

_Spurious-free dynamic range in dB from the two strongest peaks._ 
```C++
double welch_sfdr (
    welch_state_t * state,
    float min_db
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `min_db` Minimum peak level considered, dB. 



**Returns:**

Carrier-minus-highest-spur level in dB (0 if fewer than two peaks). 





        

<hr>



### function welch\_snr 

_In-band SNR in dB: peak level in [lo\_hz, hi\_hz] minus the noise floor._ 
```C++
double welch_snr (
    welch_state_t * state,
    double lo_hz,
    double hi_hz
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `lo_hz` Band lower edge, Hz. 
* `hi_hz` Band upper edge, Hz. 



**Returns:**

SNR in dB (0 if empty). 





        

<hr>



### function welch\_total\_band\_power 

_Total integrated power across all bands in dB._ 
```C++
double welch_total_band_power (
    welch_state_t * state,
    const double * bands,
    size_t bands_len
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `bands` Flat [lo,hi,...] band edges, Hz. 
* `bands_len` Number of edge values (2 \* n\_bands). 



**Returns:**

Total band power in dB (dB floor if empty). 





        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/welch/welch_core.h`

