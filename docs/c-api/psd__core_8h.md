

# File psd\_core.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**psd**](dir_1f3d46873d925f2e533983763479900d.md) **>** [**psd\_core.h**](psd__core_8h.md)

[Go to the source code of this file](psd__core_8h_source.md)

_PSD — averaging power-spectral-density estimator (Welch's method) and spectral measurement suite._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "dp_state.h"`
* `#include "jm_perf.h"`
* `#include "fft/fft_core.h"`
* `#include "acc_trace/acc_trace_core.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**psd\_state\_t**](structpsd__state__t.md) <br>_PSD state. Allocate with_ [_**psd\_create()**_](psd__core_8h.md#function-psd_create) _._ |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  void | [**psd\_accumulate**](#function-psd_accumulate) ([**psd\_state\_t**](structpsd__state__t.md) \* state, const float complex \* x, size\_t x\_len) <br>_Window, FFT and fold complex baseband frames into the average. Processes floor(n\_in / n) full frames; a trailing partial frame is ignored._  |
|  void | [**psd\_accumulate\_real**](#function-psd_accumulate_real) ([**psd\_state\_t**](structpsd__state__t.md) \* state, const float \* x, size\_t x\_len) <br>_Window, zero-pad, FFT and fold real frames into the average. The real-input counterpart to_ [_**psd\_accumulate()**_](psd__core_8h.md#function-psd_accumulate) _: each length-n frame is windowed, zero-padded to nfft, transformed and folded as a DC-centred two-sided power spectrum (a real frame is Hermitian, so the +k and -k bins carry equal power). Read the one-sided fold with_[_**psd\_power\_onesided()**_](psd__core_8h.md#function-psd_power_onesided) _. Processes floor(n\_in / n) full frames._ |
|  size\_t | [**psd\_band\_power**](#function-psd_band_power) ([**psd\_state\_t**](structpsd__state__t.md) \* state, const double \* bands, size\_t bands\_len, float \* out) <br>_Integrated power per band in dB._ `bands` _is a flat array of_`[lo0, hi0, lo1, hi1, ...]` _band edges in Hz; the output holds one dB value per band (n\_bands = bands\_len / 2). Edges are clamped to the analysed span; a band fully outside the span integrates to the dB floor. Returns 0 before any frame is accumulated._ |
|  size\_t | [**psd\_band\_power\_max\_out**](#function-psd_band_power_max_out) ([**psd\_state\_t**](structpsd__state__t.md) \* state) <br>_Output capacity hint for band\_power(); 0 (binding sizes from bands)._  |
|  [**psd\_state\_t**](structpsd__state__t.md) \* | [**psd\_create**](#function-psd_create) (size\_t n, double fs, int window, float beta, size\_t pad, double full\_scale, size\_t bits, int mode, double alpha) <br>_Create an averaging PSD estimator._  |
|  void | [**psd\_destroy**](#function-psd_destroy) ([**psd\_state\_t**](structpsd__state__t.md) \* state) <br>_Destroy a PSD instance and release all memory._  |
|  void | [**psd\_get\_state**](#function-psd_get_state) (const [**psd\_state\_t**](structpsd__state__t.md) \* state, void \* blob) <br> |
|  double | [**psd\_noise\_floor**](#function-psd_noise_floor) ([**psd\_state\_t**](structpsd__state__t.md) \* state) <br>_Noise-floor estimate: median of the averaged dB spectrum._  |
|  double | [**psd\_occupied\_bw**](#function-psd_occupied_bw) ([**psd\_state\_t**](structpsd__state__t.md) \* state, double fraction) <br>_Occupied bandwidth in Hz holding_ `fraction` _of the total power._ |
|  size\_t | [**psd\_power\_onesided**](#function-psd_power_onesided) ([**psd\_state\_t**](structpsd__state__t.md) \* state, size\_t cap, float \* out) <br>_Averaged linear power, one-sided (length nfft/2 + 1). Folds the DC-centred two-sided estimate onto_ `[0, fs/2]` _: the DC and Nyquist bins are kept as-is, every interior bin is the sum of its +k and -k halves (so a real-input tone reads 2\*avg\|_`X[k]` _\|^2 / cg^2 there). Coherent-gain normalised; full\_scale is NOT applied. Returns 0 before any accumulate._ |
|  size\_t | [**psd\_power\_onesided\_max\_out**](#function-psd_power_onesided_max_out) ([**psd\_state\_t**](structpsd__state__t.md) \* state) <br>_Output capacity hint for_ [_**psd\_power\_onesided()**_](psd__core_8h.md#function-psd_power_onesided) _; equals nfft/2+1._ |
|  size\_t | [**psd\_power\_twosided**](#function-psd_power_twosided) ([**psd\_state\_t**](structpsd__state__t.md) \* state, size\_t cap, float \* out) <br>_Averaged linear power, DC-centred two-sided (length nfft). Coherent-gain normalised (_ `out[k]` _= avg\|_`X[k]` _\|^2 / cg^2); full\_scale is NOT applied (callers that want a dBFS reference divide by full\_scale^2). This is the raw spectral estimate the measurement kernels integrate over. Returns 0 (and writes nothing) before any frame is accumulated._ |
|  size\_t | [**psd\_power\_twosided\_max\_out**](#function-psd_power_twosided_max_out) ([**psd\_state\_t**](structpsd__state__t.md) \* state) <br>_Output capacity hint for_ [_**psd\_power\_twosided()**_](psd__core_8h.md#function-psd_power_twosided) _; equals nfft._ |
|  size\_t | [**psd\_psd\_db**](#function-psd_psd_db) ([**psd\_state\_t**](structpsd__state__t.md) \* state, size\_t n, float \* out) <br>_Averaged power spectrum in dB, DC-centred. Normalised by window coherent gain so a full-scale tone reads its true power (a unit-amplitude tone peaks near 0 dB). Returns 0 (Python None) before any frame is accumulated._  |
|  size\_t | [**psd\_psd\_db\_max\_out**](#function-psd_psd_db_max_out) ([**psd\_state\_t**](structpsd__state__t.md) \* state) <br>_Output capacity hint for psd\_db(); equals nfft._  |
|  size\_t | [**psd\_psd\_dbhz**](#function-psd_psd_dbhz) ([**psd\_state\_t**](structpsd__state__t.md) \* state, size\_t n, float \* out) <br>_Averaged power spectral density in dB/Hz, DC-centred. Normalised by_ `fs * sum(w^2)` _(ENBW-aware), the standard one-sided-free PSD scaling. Differs from psd\_db() by the constant_`10*log10(cg^2/(fs*s2))` _. Returns 0 (Python None) before any frame is accumulated._ |
|  size\_t | [**psd\_psd\_dbhz\_max\_out**](#function-psd_psd_dbhz_max_out) ([**psd\_state\_t**](structpsd__state__t.md) \* state) <br>_Output capacity hint for psd\_dbhz(); equals n._  |
|  void | [**psd\_reset**](#function-psd_reset) ([**psd\_state\_t**](structpsd__state__t.md) \* state) <br>_Discard the running average; the next accumulate re-seeds it._  |
|  int | [**psd\_set\_state**](#function-psd_set_state) ([**psd\_state\_t**](structpsd__state__t.md) \* state, const void \* blob) <br> |
|  double | [**psd\_sfdr**](#function-psd_sfdr) ([**psd\_state\_t**](structpsd__state__t.md) \* state, float min\_db) <br>_Spurious-free dynamic range in dB from the two strongest peaks._  |
|  double | [**psd\_snr**](#function-psd_snr) ([**psd\_state\_t**](structpsd__state__t.md) \* state, double lo\_hz, double hi\_hz) <br>_In-band SNR in dB: peak level in_ `[lo_hz, hi_hz]` _minus the noise floor._ |
|  size\_t | [**psd\_state\_bytes**](#function-psd_state_bytes) (const [**psd\_state\_t**](structpsd__state__t.md) \* state) <br> |
|  double | [**psd\_total\_band\_power**](#function-psd_total_band_power) ([**psd\_state\_t**](structpsd__state__t.md) \* state, const double \* bands, size\_t bands\_len) <br>_Total integrated power across all bands in dB._  |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**PSD\_STATE\_MAGIC**](psd__core_8h.md#define-psd_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('P','S','D',' ')`<br> |
| define  | [**PSD\_STATE\_VERSION**](psd__core_8h.md#define-psd_state_version)  `1u`<br> |

## Detailed Description


A stateful, C-first periodogram averager (Welch's method). It composes the existing pieces of the library rather than re-implementing them: an [**fft\_state\_t**](structfft__state__t.md) forward plan, a spectral window (Hann or Kaiser) with its coherent gain and ENBW, an [**acc\_trace\_state\_t**](structacc__trace__state__t.md) per-bin power averager (mean / EMA / max-hold / min-hold), and the spectral free functions ([**magnitude\_db\_cf32**](spectral__core_8h.md#function-magnitude_db_cf32), [**find\_peaks\_f32**](spectral__core_8h.md#function-find_peaks_f32), [**obw\_from\_power**](spectral__core_8h.md#function-obw_from_power), [**noise\_floor\_db**](spectral__core_8h.md#function-noise_floor_db)) for the derived measurements.


Feed complex baseband frames with [**psd\_accumulate()**](psd__core_8h.md#function-psd_accumulate); each length-n frame is windowed, FFT'd, converted to power, fftshifted to DC-centred order and folded into the running average. Then read:
* [**psd\_psd\_db()**](psd__core_8h.md#function-psd_psd_db) : averaged power spectrum, dB (peak reads tone power)
* [**psd\_psd\_dbhz()**](psd__core_8h.md#function-psd_psd_dbhz) : averaged PSD, dB/Hz (ENBW / fs normalised)
* [**psd\_band\_power()**](psd__core_8h.md#function-psd_band_power) / [**psd\_total\_band\_power()**](psd__core_8h.md#function-psd_total_band_power) : integrated band power, dB
* [**psd\_occupied\_bw()**](psd__core_8h.md#function-psd_occupied_bw) : occupied bandwidth, Hz
* [**psd\_noise\_floor()**](psd__core_8h.md#function-psd_noise_floor) / [**psd\_snr()**](psd__core_8h.md#function-psd_snr) / [**psd\_sfdr()**](psd__core_8h.md#function-psd_sfdr) : level statistics, dB




All spectra are DC-centred (fftshift), matching find\_peaks\_f32's bin -&gt; frequency convention (bin i maps to (i - n/2)/n in normalised frequency, so spectral peaks are obtained idiomatically with `find_peaks_f32 (w.psd_db(), n_peaks, min_db)`).


Lifecycle: create -&gt; (accumulate / reset)\* -&gt; (measurement getters)\* -&gt; destroy 


    
## Public Functions Documentation




### function psd\_accumulate 

_Window, FFT and fold complex baseband frames into the average. Processes floor(n\_in / n) full frames; a trailing partial frame is ignored._ 
```C++
void psd_accumulate (
    psd_state_t * state,
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
>>> from doppler.spectral import PSD
>>> n = 64
>>> w = PSD(n=n, fs=1.0, window="hann", mode="mean")
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



### function psd\_accumulate\_real 

_Window, zero-pad, FFT and fold real frames into the average. The real-input counterpart to_ [_**psd\_accumulate()**_](psd__core_8h.md#function-psd_accumulate) _: each length-n frame is windowed, zero-padded to nfft, transformed and folded as a DC-centred two-sided power spectrum (a real frame is Hermitian, so the +k and -k bins carry equal power). Read the one-sided fold with_[_**psd\_power\_onesided()**_](psd__core_8h.md#function-psd_power_onesided) _. Processes floor(n\_in / n) full frames._
```C++
void psd_accumulate_real (
    psd_state_t * state,
    const float * x,
    size_t x_len
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `x` Real samples (f32). 
* `x_len` Number of samples in `x`. 




        

<hr>



### function psd\_band\_power 

_Integrated power per band in dB._ `bands` _is a flat array of_`[lo0, hi0, lo1, hi1, ...]` _band edges in Hz; the output holds one dB value per band (n\_bands = bands\_len / 2). Edges are clamped to the analysed span; a band fully outside the span integrates to the dB floor. Returns 0 before any frame is accumulated._
```C++
size_t psd_band_power (
    psd_state_t * state,
    const double * bands,
    size_t bands_len,
    float * out
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `bands` Flat `[lo,hi,...]` band edges, Hz. 
* `bands_len` Number of edge values (2 \* n\_bands). 
* `out` Destination, at least n\_bands float32 elements. 



**Returns:**

n\_bands, or 0 if empty.



```C++
>>> import numpy as np
>>> from doppler.spectral import PSD
>>> w = PSD(n=64, fs=1.0, window="hann", mode="mean")
>>> w.accumulate(np.ones(64, dtype=np.complex64))
>>> pb = w.band_power(np.array([-0.5, 0.0, 0.0, 0.5]))
>>> pb.shape
(2,)
```
 


        

<hr>



### function psd\_band\_power\_max\_out 

_Output capacity hint for band\_power(); 0 (binding sizes from bands)._ 
```C++
size_t psd_band_power_max_out (
    psd_state_t * state
) 
```




<hr>



### function psd\_create 

_Create an averaging PSD estimator._ 
```C++
psd_state_t * psd_create (
    size_t n,
    double fs,
    int window,
    float beta,
    size_t pad,
    double full_scale,
    size_t bits,
    int mode,
    double alpha
) 
```





**Parameters:**


* `n` Window / frame length in samples. Must be &gt;= 2. 
* `fs` Sample rate in Hz (used for dB/Hz and band frequencies). 
* `window` Window index: 0 = Hann, 1 = Kaiser, 2 = Blackman-Harris. 
* `beta` Kaiser beta (ignored for Hann/Blackman-Harris). 
* `pad` Zero-pad factor (&gt;= 1); nfft = next\_pow2(n \* pad). 
* `full_scale` Amplitude that reads 0 dBFS in the dB getters (&gt; 0). Ignored when `bits` &gt; 0. 
* `bits` ADC depth: when &gt; 0, sets full\_scale = 2^(bits-1) (the single definition of the dBFS reference); 0 = use `full_scale` directly. 
* `mode` Averaging mode index (0=mean, 1=exp, 2=maxhold, 3=minhold). 
* `alpha` EMA smoothing factor (exp mode only). 



**Returns:**

Heap-allocated state, or NULL on invalid argument or OOM. 




**Note:**

Caller must call [**psd\_destroy()**](psd__core_8h.md#function-psd_destroy) when done.



```C++
>>> from doppler.spectral import PSD
>>> w = PSD(n=1024, fs=1.0e6, window="kaiser", beta=8.0, mode="mean")
>>> w.n, w.fs
(1024, 1000000.0)
>>> round(w.rbw / (w.fs / w.n), 3) == round(w.enbw, 3)
True
```
 


        

<hr>



### function psd\_destroy 

_Destroy a PSD instance and release all memory._ 
```C++
void psd_destroy (
    psd_state_t * state
) 
```





**Parameters:**


* `state` May be NULL (no-op). 




        

<hr>



### function psd\_get\_state 

```C++
void psd_get_state (
    const psd_state_t * state,
    void * blob
) 
```




<hr>



### function psd\_noise\_floor 

_Noise-floor estimate: median of the averaged dB spectrum._ 
```C++
double psd_noise_floor (
    psd_state_t * state
) 
```





**Returns:**

Median dB level (0 if empty). 





        

<hr>



### function psd\_occupied\_bw 

_Occupied bandwidth in Hz holding_ `fraction` _of the total power._
```C++
double psd_occupied_bw (
    psd_state_t * state,
    double fraction
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `fraction` Power fraction in (0, 1], e.g. 0.99. 



**Returns:**

Occupied bandwidth in Hz (0 if empty or no power). 





        

<hr>



### function psd\_power\_onesided 

_Averaged linear power, one-sided (length nfft/2 + 1). Folds the DC-centred two-sided estimate onto_ `[0, fs/2]` _: the DC and Nyquist bins are kept as-is, every interior bin is the sum of its +k and -k halves (so a real-input tone reads 2\*avg\|_`X[k]` _\|^2 / cg^2 there). Coherent-gain normalised; full\_scale is NOT applied. Returns 0 before any accumulate._
```C++
size_t psd_power_onesided (
    psd_state_t * state,
    size_t cap,
    float * out
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `cap` Caller buffer capacity (must be &gt;= nfft/2 + 1). 
* `out` Destination, at least nfft/2 + 1 float32 elements. 



**Returns:**

nfft/2 + 1, or 0 if empty. 





        

<hr>



### function psd\_power\_onesided\_max\_out 

_Output capacity hint for_ [_**psd\_power\_onesided()**_](psd__core_8h.md#function-psd_power_onesided) _; equals nfft/2+1._
```C++
size_t psd_power_onesided_max_out (
    psd_state_t * state
) 
```




<hr>



### function psd\_power\_twosided 

_Averaged linear power, DC-centred two-sided (length nfft). Coherent-gain normalised (_ `out[k]` _= avg\|_`X[k]` _\|^2 / cg^2); full\_scale is NOT applied (callers that want a dBFS reference divide by full\_scale^2). This is the raw spectral estimate the measurement kernels integrate over. Returns 0 (and writes nothing) before any frame is accumulated._
```C++
size_t psd_power_twosided (
    psd_state_t * state,
    size_t cap,
    float * out
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `cap` Caller buffer capacity (must be &gt;= nfft). 
* `out` Destination, at least nfft float32 elements. 



**Returns:**

nfft, or 0 if empty. 





        

<hr>



### function psd\_power\_twosided\_max\_out 

_Output capacity hint for_ [_**psd\_power\_twosided()**_](psd__core_8h.md#function-psd_power_twosided) _; equals nfft._
```C++
size_t psd_power_twosided_max_out (
    psd_state_t * state
) 
```




<hr>



### function psd\_psd\_db 

_Averaged power spectrum in dB, DC-centred. Normalised by window coherent gain so a full-scale tone reads its true power (a unit-amplitude tone peaks near 0 dB). Returns 0 (Python None) before any frame is accumulated._ 
```C++
size_t psd_psd_db (
    psd_state_t * state,
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



### function psd\_psd\_db\_max\_out 

_Output capacity hint for psd\_db(); equals nfft._ 
```C++
size_t psd_psd_db_max_out (
    psd_state_t * state
) 
```




<hr>



### function psd\_psd\_dbhz 

_Averaged power spectral density in dB/Hz, DC-centred. Normalised by_ `fs * sum(w^2)` _(ENBW-aware), the standard one-sided-free PSD scaling. Differs from psd\_db() by the constant_`10*log10(cg^2/(fs*s2))` _. Returns 0 (Python None) before any frame is accumulated._
```C++
size_t psd_psd_dbhz (
    psd_state_t * state,
    size_t n,
    float * out
) 
```




```C++
>>> import numpy as np
>>> from doppler.spectral import PSD
>>> w = PSD(n=32, fs=2.0, window="hann", mode="mean")
>>> w.accumulate(np.ones(32, dtype=np.complex64))
>>> a = w.psd_db(); b = w.psd_dbhz()
>>> bool(np.allclose(a - b, (a - b)[0]))   # offset is a constant
True
```
 


        

<hr>



### function psd\_psd\_dbhz\_max\_out 

_Output capacity hint for psd\_dbhz(); equals n._ 
```C++
size_t psd_psd_dbhz_max_out (
    psd_state_t * state
) 
```




<hr>



### function psd\_reset 

_Discard the running average; the next accumulate re-seeds it._ 
```C++
void psd_reset (
    psd_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 




        

<hr>



### function psd\_set\_state 

```C++
int psd_set_state (
    psd_state_t * state,
    const void * blob
) 
```




<hr>



### function psd\_sfdr 

_Spurious-free dynamic range in dB from the two strongest peaks._ 
```C++
double psd_sfdr (
    psd_state_t * state,
    float min_db
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `min_db` Minimum peak level considered, dB. 



**Returns:**

Carrier-minus-highest-spur level in dB (0 if fewer than two peaks). 





        

<hr>



### function psd\_snr 

_In-band SNR in dB: peak level in_ `[lo_hz, hi_hz]` _minus the noise floor._
```C++
double psd_snr (
    psd_state_t * state,
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



### function psd\_state\_bytes 

```C++
size_t psd_state_bytes (
    const psd_state_t * state
) 
```




<hr>



### function psd\_total\_band\_power 

_Total integrated power across all bands in dB._ 
```C++
double psd_total_band_power (
    psd_state_t * state,
    const double * bands,
    size_t bands_len
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `bands` Flat `[lo,hi,...]` band edges, Hz. 
* `bands_len` Number of edge values (2 \* n\_bands). 



**Returns:**

Total band power in dB (dB floor if empty). 





        

<hr>
## Macro Definition Documentation





### define PSD\_STATE\_MAGIC 

```C++
#define PSD_STATE_MAGIC `DP_FOURCC ('P','S','D',' ')`
```




<hr>



### define PSD\_STATE\_VERSION 

```C++
#define PSD_STATE_VERSION `1u`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/psd/psd_core.h`

