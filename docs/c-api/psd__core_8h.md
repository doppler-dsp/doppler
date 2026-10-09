

# File psd\_core.h



[**FileList**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**psd**](dir_80b4a8b440284bffd6bf0fbeb7bfe41f.md) **>** [**psd\_core.h**](psd__core_8h.md)

[Go to the source code of this file](psd__core_8h_source.md)

_PSD — averaging power-spectral-density estimator (Welch's method) and spectral measurement suite._ [More...](#detailed-description)

* `#include "doppler/clib_common.h"`
* `#include "doppler/dp_state.h"`
* `#include "doppler/jm_perf.h"`
* `#include "doppler/fft/fft_core.h"`
* `#include "doppler/acc_trace/acc_trace_core.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**dp\_psd\_state\_t**](structdp__psd__state__t.md) <br>_PSD state. Allocate with_ [_**dp\_psd\_create()**_](psd__core_8h.md#function-dp_psd_create) _._ |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  void | [**dp\_psd\_accumulate**](#function-dp_psd_accumulate) ([**dp\_psd\_state\_t**](structdp__psd__state__t.md) \* state, const float \_Complex \* x, size\_t x\_len) <br>_Window, FFT and fold complex baseband frames into the average. Processes floor(n\_in / n) full frames; a trailing partial frame is ignored._  |
|  void | [**dp\_psd\_accumulate\_real**](#function-dp_psd_accumulate_real) ([**dp\_psd\_state\_t**](structdp__psd__state__t.md) \* state, const float \* x, size\_t x\_len) <br>_Window, zero-pad, FFT and fold real frames into the average. The real-input counterpart to_ [_**dp\_psd\_accumulate()**_](psd__core_8h.md#function-dp_psd_accumulate) _: each length-n frame is windowed, zero-padded to nfft, transformed and folded as a DC-centred two-sided power spectrum (a real frame is Hermitian, so the +k and -k bins carry equal power). Read the one-sided fold with_[_**dp\_psd\_power\_onesided()**_](psd__core_8h.md#function-dp_psd_power_onesided) _. Processes floor(n\_in / n) full frames._ |
|  size\_t | [**dp\_psd\_band\_power**](#function-dp_psd_band_power) ([**dp\_psd\_state\_t**](structdp__psd__state__t.md) \* state, const double \* bands, size\_t bands\_len, float \* out, size\_t max\_out) <br>_Integrated power per band in dB._ `bands` _is a flat array of_`[lo0, hi0, lo1, hi1, ...]` _band edges in Hz; the output holds one dB value per band (n\_bands = bands\_len / 2). Edges are clamped to the analysed span; a band fully outside the span integrates to the dB floor. Returns 0 (Python None) when there is nothing to report: before any frame is accumulated, or when_`bands` _holds no complete lo/hi pair._ |
|  size\_t | [**dp\_psd\_band\_power\_max\_out**](#function-dp_psd_band_power_max_out) ([**dp\_psd\_state\_t**](structdp__psd__state__t.md) \* state) <br>_Output capacity hint for band\_power(); 0 (binding sizes from bands)._  |
|  [**dp\_psd\_state\_t**](structdp__psd__state__t.md) \* | [**dp\_psd\_create**](#function-dp_psd_create) (size\_t n, double fs, int window, float beta, size\_t pad, double full\_scale, size\_t bits, int mode, double alpha) <br>_Create an averaging PSD estimator._  |
|  void | [**dp\_psd\_destroy**](#function-dp_psd_destroy) ([**dp\_psd\_state\_t**](structdp__psd__state__t.md) \* state) <br>_Destroy a PSD instance and release all memory._  |
|  void | [**dp\_psd\_frame\_db**](#function-dp_psd_frame_db) ([**dp\_psd\_state\_t**](structdp__psd__state__t.md) \* state, const float \_Complex \* x, float \* out) <br>_One frame in dBFS, against the estimator's own reference._  |
|  void | [**dp\_psd\_frame\_linear**](#function-dp_psd_frame_linear) ([**dp\_psd\_state\_t**](structdp__psd__state__t.md) \* state, const float \_Complex \* x, float \* out) <br>_One frame in linear full-scale^2 units, against the estimator's own reference._  |
|  void | [**dp\_psd\_frame\_power**](#function-dp_psd_frame_power) ([**dp\_psd\_state\_t**](structdp__psd__state__t.md) \* state, const float \_Complex \* x, float \* pwr) <br>_One frame's DC-centred two-sided power: window, zero-pad, FFT, \|X\|^2._  |
|  void | [**dp\_psd\_get\_state**](#function-dp_psd_get_state) (const [**dp\_psd\_state\_t**](structdp__psd__state__t.md) \* state, void \* blob) <br> |
|  double | [**dp\_psd\_noise\_floor**](#function-dp_psd_noise_floor) ([**dp\_psd\_state\_t**](structdp__psd__state__t.md) \* state) <br>_Noise-floor estimate: median of the averaged dB spectrum._  |
|  double | [**dp\_psd\_occupied\_bw**](#function-dp_psd_occupied_bw) ([**dp\_psd\_state\_t**](structdp__psd__state__t.md) \* state, double fraction) <br>_Occupied bandwidth in Hz holding_ `fraction` _of the total power._ |
|  size\_t | [**dp\_psd\_power\_onesided**](#function-dp_psd_power_onesided) ([**dp\_psd\_state\_t**](structdp__psd__state__t.md) \* state, size\_t cap, float \* out, size\_t max\_out) <br>_Averaged linear power, one-sided (length nfft/2 + 1). Folds the DC-centred two-sided estimate onto_ `[0, fs/2]` _: the DC and Nyquist bins are kept as-is, every interior bin is the sum of its +k and -k halves (so a real-input tone reads 2\*avg\|_`X[k]` _\|^2 / cg^2 there). Coherent-gain normalised; full\_scale is NOT applied. Returns 0 before any accumulate._ |
|  size\_t | [**dp\_psd\_power\_onesided\_max\_out**](#function-dp_psd_power_onesided_max_out) ([**dp\_psd\_state\_t**](structdp__psd__state__t.md) \* state) <br>_Output capacity hint for_ [_**dp\_psd\_power\_onesided()**_](psd__core_8h.md#function-dp_psd_power_onesided) _; equals nfft/2+1._ |
|  size\_t | [**dp\_psd\_power\_twosided**](#function-dp_psd_power_twosided) ([**dp\_psd\_state\_t**](structdp__psd__state__t.md) \* state, size\_t cap, float \* out, size\_t max\_out) <br>_Averaged linear power, DC-centred two-sided (length nfft). Coherent-gain normalised (_ `out[k]` _= avg\|_`X[k]` _\|^2 / cg^2); full\_scale is NOT applied (callers that want a dBFS reference divide by full\_scale^2). This is the raw spectral estimate the measurement kernels integrate over. Returns 0 (and writes nothing) before any frame is accumulated._ |
|  size\_t | [**dp\_psd\_power\_twosided\_max\_out**](#function-dp_psd_power_twosided_max_out) ([**dp\_psd\_state\_t**](structdp__psd__state__t.md) \* state) <br>_Output capacity hint for_ [_**dp\_psd\_power\_twosided()**_](psd__core_8h.md#function-dp_psd_power_twosided) _; equals nfft._ |
|  size\_t | [**dp\_psd\_psd\_db**](#function-dp_psd_psd_db) ([**dp\_psd\_state\_t**](structdp__psd__state__t.md) \* state, size\_t n, float \* out, size\_t max\_out) <br>_Averaged power spectrum in dB, DC-centred. Normalised by window coherent gain so a full-scale tone reads its true power (a unit-amplitude tone peaks near 0 dB). Returns 0 (Python None) before any frame is accumulated._  |
|  size\_t | [**dp\_psd\_psd\_db\_max\_out**](#function-dp_psd_psd_db_max_out) ([**dp\_psd\_state\_t**](structdp__psd__state__t.md) \* state) <br>_Output capacity hint for psd\_db(); equals nfft._  |
|  size\_t | [**dp\_psd\_psd\_dbhz**](#function-dp_psd_psd_dbhz) ([**dp\_psd\_state\_t**](structdp__psd__state__t.md) \* state, size\_t n, float \* out, size\_t max\_out) <br>_Averaged power spectral density in dB/Hz, DC-centred. Normalised by_ `fs * sum(w^2)` _(ENBW-aware), the standard one-sided-free PSD scaling. Differs from psd\_db() by the constant_`10*log10(cg^2/(fs*s2))` _. Returns 0 (Python None) before any frame is accumulated._ |
|  size\_t | [**dp\_psd\_psd\_dbhz\_max\_out**](#function-dp_psd_psd_dbhz_max_out) ([**dp\_psd\_state\_t**](structdp__psd__state__t.md) \* state) <br>_Output capacity hint for psd\_dbhz(); equals nfft._  |
|  void | [**dp\_psd\_reset**](#function-dp_psd_reset) ([**dp\_psd\_state\_t**](structdp__psd__state__t.md) \* state) <br>_Discard the running average; the next accumulate re-seeds it._  |
|  int | [**dp\_psd\_set\_state**](#function-dp_psd_set_state) ([**dp\_psd\_state\_t**](structdp__psd__state__t.md) \* state, const void \* blob) <br> |
|  double | [**dp\_psd\_sfdr**](#function-dp_psd_sfdr) ([**dp\_psd\_state\_t**](structdp__psd__state__t.md) \* state, float min\_db) <br>_Spurious-free dynamic range in dB from the two strongest peaks._  |
|  double | [**dp\_psd\_snr**](#function-dp_psd_snr) ([**dp\_psd\_state\_t**](structdp__psd__state__t.md) \* state, double lo\_hz, double hi\_hz) <br>_In-band SNR in dB: peak level in_ `[lo_hz, hi_hz]` _minus the noise floor._ |
|  size\_t | [**dp\_psd\_state\_bytes**](#function-dp_psd_state_bytes) (const [**dp\_psd\_state\_t**](structdp__psd__state__t.md) \* state) <br> |
|  double | [**dp\_psd\_total\_band\_power**](#function-dp_psd_total_band_power) ([**dp\_psd\_state\_t**](structdp__psd__state__t.md) \* state, const double \* bands, size\_t bands\_len) <br>_Total integrated power across all bands in dB._  |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**PSD\_STATE\_MAGIC**](psd__core_8h.md#define-psd_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('P','S','D',' ')`<br> |
| define  | [**PSD\_STATE\_VERSION**](psd__core_8h.md#define-psd_state_version)  `1u`<br> |

## Detailed Description


A stateful, C-first periodogram averager (Welch's method). It composes the existing pieces of the library rather than re-implementing them: an [**dp\_fft\_state\_t**](structdp__fft__state__t.md) forward plan, a spectral window (Hann, Kaiser, Blackman-Harris or rectangular) with its coherent gain and ENBW, an [**dp\_acc\_trace\_state\_t**](structdp__acc__trace__state__t.md) per-bin power averager (mean / EMA / max-hold / min-hold), and the spectral free functions ([**dp\_magnitude\_db\_cf32**](spectral__core_8h.md#function-dp_magnitude_db_cf32), [**dp\_find\_peaks\_f32**](spectral__core_8h.md#function-dp_find_peaks_f32), [**dp\_obw\_from\_power**](spectral__core_8h.md#function-dp_obw_from_power), [**dp\_noise\_floor\_db**](spectral__core_8h.md#function-dp_noise_floor_db)) for the derived measurements.


Feed complex baseband frames with [**dp\_psd\_accumulate()**](psd__core_8h.md#function-dp_psd_accumulate); each length-n frame is windowed, FFT'd, converted to power, fftshifted to DC-centred order and folded into the running average. Then read:
* [**dp\_psd\_psd\_db()**](psd__core_8h.md#function-dp_psd_psd_db) : averaged power spectrum, dB (peak reads tone power)
* [**dp\_psd\_psd\_dbhz()**](psd__core_8h.md#function-dp_psd_psd_dbhz) : averaged PSD, dB/Hz (ENBW / fs normalised)
* [**dp\_psd\_band\_power()**](psd__core_8h.md#function-dp_psd_band_power) / [**dp\_psd\_total\_band\_power()**](psd__core_8h.md#function-dp_psd_total_band_power) : integrated band power, dB
* [**dp\_psd\_occupied\_bw()**](psd__core_8h.md#function-dp_psd_occupied_bw) : occupied bandwidth, Hz
* [**dp\_psd\_noise\_floor()**](psd__core_8h.md#function-dp_psd_noise_floor) / [**dp\_psd\_snr()**](psd__core_8h.md#function-dp_psd_snr) / [**dp\_psd\_sfdr()**](psd__core_8h.md#function-dp_psd_sfdr) : level statistics, dB




All spectra are DC-centred (fftshift), matching find\_peaks\_f32's bin -&gt; frequency convention (bin i maps to (i - nfft/2)/nfft in normalised frequency, so spectral peaks are obtained idiomatically with `find_peaks_f32(w.psd_db(), n_peaks, min_db)`).


Lifecycle: create -&gt; (accumulate / reset)\* -&gt; (measurement getters)\* -&gt; destroy 


    
## Public Functions Documentation




### function dp\_psd\_accumulate 

_Window, FFT and fold complex baseband frames into the average. Processes floor(n\_in / n) full frames; a trailing partial frame is ignored._ 
```C++
void dp_psd_accumulate (
    dp_psd_state_t * state,
    const float _Complex * x,
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



### function dp\_psd\_accumulate\_real 

_Window, zero-pad, FFT and fold real frames into the average. The real-input counterpart to_ [_**dp\_psd\_accumulate()**_](psd__core_8h.md#function-dp_psd_accumulate) _: each length-n frame is windowed, zero-padded to nfft, transformed and folded as a DC-centred two-sided power spectrum (a real frame is Hermitian, so the +k and -k bins carry equal power). Read the one-sided fold with_[_**dp\_psd\_power\_onesided()**_](psd__core_8h.md#function-dp_psd_power_onesided) _. Processes floor(n\_in / n) full frames._
```C++
void dp_psd_accumulate_real (
    dp_psd_state_t * state,
    const float * x,
    size_t x_len
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `x` Real samples (f32). 
* `x_len` Number of samples in `x`. 




        

<hr>



### function dp\_psd\_band\_power 

_Integrated power per band in dB._ `bands` _is a flat array of_`[lo0, hi0, lo1, hi1, ...]` _band edges in Hz; the output holds one dB value per band (n\_bands = bands\_len / 2). Edges are clamped to the analysed span; a band fully outside the span integrates to the dB floor. Returns 0 (Python None) when there is nothing to report: before any frame is accumulated, or when_`bands` _holds no complete lo/hi pair._
```C++
size_t dp_psd_band_power (
    dp_psd_state_t * state,
    const double * bands,
    size_t bands_len,
    float * out,
    size_t max_out
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `bands` Flat `[lo,hi,...]` band edges, Hz. 
* `bands_len` Number of edge values (2 \* n\_bands). 
* `out` Destination, at least n\_bands float32 elements. 
* `max_out` Capacity of `out` in elements. Emission stops there, so the return value is the number actually written. 



**Returns:**

The number of bands written, min(n\_bands, max\_out); 0 (Python None) when there is nothing to report.



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



### function dp\_psd\_band\_power\_max\_out 

_Output capacity hint for band\_power(); 0 (binding sizes from bands)._ 
```C++
size_t dp_psd_band_power_max_out (
    dp_psd_state_t * state
) 
```




<hr>



### function dp\_psd\_create 

_Create an averaging PSD estimator._ 
```C++
dp_psd_state_t * dp_psd_create (
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
* `fs` Sample rate in Hz (used for dB/Hz and band frequencies); finite and &gt; 0. 
* `window` Window index: 0 = Hann, 1 = Kaiser, 2 = Blackman-Harris, 3 = rectangular (no taper: best resolution, worst leakage). 
* `beta` Kaiser beta (ignored by every other window). One that makes the window non-finite  NaN, or large enough to overflow it  is refused. 
* `pad` Zero-pad factor (&gt;= 1; 0 is refused); nfft = next\_pow\_two(n \* pad). 
* `full_scale` Amplitude that reads 0 dBFS in the dB getters; finite and &gt; 0. Ignored when `bits` &gt; 0. 
* `bits` ADC depth: when &gt; 0, sets full\_scale = 2^(bits-1) (the single definition of the dBFS reference); 0 = use `full_scale` directly. At most 64. 
* `mode` Averaging mode index (0=mean, 1=exp, 2=maxhold, 3=minhold). 
* `alpha` EMA smoothing factor, exp mode only, where it must lie in (0, 1]: the rule is the AccTrace averager's, whose create refuses anything else, so this does too. Ignored, and so not checked, in the other modes. 



**Returns:**

Heap-allocated state, or NULL on invalid argument or OOM. A window whose coherent gain sum(w) is 0  Hann at n = 2, whose symmetric form is all zeros  is refused too: every reading divides by it. So is an `n` or `n` \* `pad` too large for its buffers to be sized, rather than wrapped to a small one. 




**Note:**

Caller must call [**dp\_psd\_destroy()**](psd__core_8h.md#function-dp_psd_destroy) when done.



```C++
>>> from doppler.spectral import PSD
>>> w = PSD(n=1024, fs=1.0e6, window="kaiser", beta=8.0, mode="mean")
>>> w.n, w.fs
(1024, 1000000.0)
>>> round(w.rbw / (w.fs / w.n), 3) == round(w.enbw, 3)
True
```
 


        

<hr>



### function dp\_psd\_destroy 

_Destroy a PSD instance and release all memory._ 
```C++
void dp_psd_destroy (
    dp_psd_state_t * state
) 
```





**Parameters:**


* `state` May be NULL (no-op). 




        

<hr>



### function dp\_psd\_frame\_db 

_One frame in dBFS, against the estimator's own reference._ 
```C++
void dp_psd_frame_db (
    dp_psd_state_t * state,
    const float _Complex * x,
    float * out
) 
```



[**dp\_psd\_frame\_power()**](psd__core_8h.md#function-dp_psd_frame_power) divided by the window coherent gain squared and the full-scale reference, as 10\*log10 with the -200 dB floor, so a full-scale tone on a bin reads 0 dB whatever the window. The result equals [**dp\_psd\_psd\_db()**](psd__core_8h.md#function-dp_psd_psd_db) after accumulating that one frame, bit for bit (pinned by test\_psd\_core.c). Does not touch the running average.




**Parameters:**


* `state` Must be non-NULL. 
* `x` Exactly `state->n` complex samples (cf32). 
* `out` Output, `state->nfft` floats, DC-centred.


```C++
dp_psd_state_t *p = dp_psd_create (8, 1.0, 3, 0.0f, 1, 1.0, 0, 0, 0.0);
float _Complex x[8];
float db[8];
for (int i = 0; i < 8; i++)
  x[i] = cexpf (I * 2.0f * 3.14159265f * 2.0f * (float)i / 8.0f);
dp_psd_frame_db (p, x, db);        // unit tone on bin 2, rectangular
const int bad = fabsf (db[4 + 2]) > 1e-4f; // bin 2 is at nfft/2 + 2
dp_psd_destroy (p);                // freed on either path
if (bad)
  return 1;
```
 


        

<hr>



### function dp\_psd\_frame\_linear 

_One frame in linear full-scale^2 units, against the estimator's own reference._ 
```C++
void dp_psd_frame_linear (
    dp_psd_state_t * state,
    const float _Complex * x,
    float * out
) 
```



[**dp\_psd\_frame\_power()**](psd__core_8h.md#function-dp_psd_frame_power) divided by the window coherent gain squared and the full-scale reference squared, cg^2 \* full\_scale^2: a full-scale tone on a bin reads 1.0 whatever the window. [**dp\_psd\_frame\_db()**](psd__core_8h.md#function-dp_psd_frame_db) is 10\*log10 of the same quotient, with the -200 dB floor  the two are read from one double, so they cannot disagree about the reference. This is the frame a consumer that averages for itself should take: the raw [**dp\_psd\_frame\_power()**](psd__core_8h.md#function-dp_psd_frame_power) sits 20\*log10(sum(w)) above it, 54.18 dB for Hann at 1024 points (docs/design/spectrogram-measurements.md section 5.3). Does not touch the running average; not thread-safe on one state.




**Parameters:**


* `state` Must be non-NULL. 
* `x` Exactly `state->n` complex samples (cf32). 
* `out` Output, `state->nfft` floats, DC-centred.


```C++
dp_psd_state_t *p = dp_psd_create (8, 1.0, 0, 0.0f, 1, 1.0, 0, 0, 0.0);
float _Complex x[8];
float lin[8];
for (int i = 0; i < 8; i++)
  x[i] = cexpf (I * 2.0f * 3.14159265f * 2.0f * (float)i / 8.0f);
dp_psd_frame_linear (p, x, lin);   // unit tone on bin 2, Hann
const int bad = fabsf (lin[4 + 2] - 1.0f) > 1e-4f; // bin 2 at nfft/2 + 2
dp_psd_destroy (p);                // freed on either path
if (bad)
  return 1;
```
 


        

<hr>



### function dp\_psd\_frame\_power 

_One frame's DC-centred two-sided power: window, zero-pad, FFT, \|X\|^2._ 
```C++
void dp_psd_frame_power (
    dp_psd_state_t * state,
    const float _Complex * x,
    float * pwr
) 
```



The per-frame kernel [**dp\_psd\_accumulate()**](psd__core_8h.md#function-dp_psd_accumulate) folds into its average, exposed without the average. The result is the UN-NORMALISED power the averager folds: no division by the window coherent gain squared, so a tone does not read at its true level ([**dp\_psd\_frame\_db()**](psd__core_8h.md#function-dp_psd_frame_db) applies that division). Do not compare this raw output with [**dp\_psd\_power\_twosided()**](psd__core_8h.md#function-dp_psd_power_twosided), which divides by cg^2 on the double path. It stays the raw \|X\|^2 because that is what the averager folds; a consumer that wants a frame against the dBFS reference  in linear units to average itself, or in dB  takes [**dp\_psd\_frame\_linear()**](psd__core_8h.md#function-dp_psd_frame_linear) or [**dp\_psd\_frame\_db()**](psd__core_8h.md#function-dp_psd_frame_db). Uses the estimator's scratch, so it is not thread-safe on one state and it does not touch the running average.




**Parameters:**


* `state` Must be non-NULL. 
* `x` Exactly `state->n` complex samples (cf32). 
* `pwr` Output, `state->nfft` floats, bin k (centred) at index nfft/2 + k. 




        

<hr>



### function dp\_psd\_get\_state 

```C++
void dp_psd_get_state (
    const dp_psd_state_t * state,
    void * blob
) 
```




<hr>



### function dp\_psd\_noise\_floor 

_Noise-floor estimate: median of the averaged dB spectrum._ 
```C++
double dp_psd_noise_floor (
    dp_psd_state_t * state
) 
```





**Returns:**

Median dB level (0 if empty). 





        

<hr>



### function dp\_psd\_occupied\_bw 

_Occupied bandwidth in Hz holding_ `fraction` _of the total power._
```C++
double dp_psd_occupied_bw (
    dp_psd_state_t * state,
    double fraction
) 
```



Calls obw\_from\_power on the averaged spectrum, so the search and its domain are that function's and the two cannot disagree.




**Parameters:**


* `state` Must be non-NULL. 
* `fraction` Power fraction in the OPEN interval (0, 1), e.g. 0.99. 1 is excluded: all of the power in float is every bin that rounding residue reached, which has no stable answer. 



**Returns:**

Occupied bandwidth in Hz (0 if empty or no power); NaN when `fraction` is outside (0, 1), before any frame too, rather than a silently clamped answer.



```C++
>>> import numpy as np
>>> from doppler.spectral import PSD
>>> w = PSD(n=64, fs=64.0, window="rect")
>>> w.accumulate(np.ones(64, dtype=np.complex64))  # all power at DC
>>> w.occupied_bw(0.99)                            # one bin, fs / 64
1.0
>>> w.occupied_bw(1.0)                             # outside (0, 1)
nan
```
 


        

<hr>



### function dp\_psd\_power\_onesided 

_Averaged linear power, one-sided (length nfft/2 + 1). Folds the DC-centred two-sided estimate onto_ `[0, fs/2]` _: the DC and Nyquist bins are kept as-is, every interior bin is the sum of its +k and -k halves (so a real-input tone reads 2\*avg\|_`X[k]` _\|^2 / cg^2 there). Coherent-gain normalised; full\_scale is NOT applied. Returns 0 before any accumulate._
```C++
size_t dp_psd_power_onesided (
    dp_psd_state_t * state,
    size_t cap,
    float * out,
    size_t max_out
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `cap` Caller buffer capacity (must be &gt;= nfft/2 + 1). 
* `out` Destination, at least nfft/2 + 1 float32 elements. 
* `max_out` Capacity of `out` in elements. Emission stops there, so the return value is the number actually written. 



**Returns:**

min(nfft/2 + 1, max\_out), or 0 if empty. 





        

<hr>



### function dp\_psd\_power\_onesided\_max\_out 

_Output capacity hint for_ [_**dp\_psd\_power\_onesided()**_](psd__core_8h.md#function-dp_psd_power_onesided) _; equals nfft/2+1._
```C++
size_t dp_psd_power_onesided_max_out (
    dp_psd_state_t * state
) 
```




<hr>



### function dp\_psd\_power\_twosided 

_Averaged linear power, DC-centred two-sided (length nfft). Coherent-gain normalised (_ `out[k]` _= avg\|_`X[k]` _\|^2 / cg^2); full\_scale is NOT applied (callers that want a dBFS reference divide by full\_scale^2). This is the raw spectral estimate the measurement kernels integrate over. Returns 0 (and writes nothing) before any frame is accumulated._
```C++
size_t dp_psd_power_twosided (
    dp_psd_state_t * state,
    size_t cap,
    float * out,
    size_t max_out
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `cap` Caller buffer capacity (must be &gt;= nfft). 
* `out` Destination, at least nfft float32 elements. 
* `max_out` Capacity of `out` in elements. Emission stops there, so the return value is the number actually written. 



**Returns:**

min(nfft, max\_out), or 0 if empty. 





        

<hr>



### function dp\_psd\_power\_twosided\_max\_out 

_Output capacity hint for_ [_**dp\_psd\_power\_twosided()**_](psd__core_8h.md#function-dp_psd_power_twosided) _; equals nfft._
```C++
size_t dp_psd_power_twosided_max_out (
    dp_psd_state_t * state
) 
```




<hr>



### function dp\_psd\_psd\_db 

_Averaged power spectrum in dB, DC-centred. Normalised by window coherent gain so a full-scale tone reads its true power (a unit-amplitude tone peaks near 0 dB). Returns 0 (Python None) before any frame is accumulated._ 
```C++
size_t dp_psd_psd_db (
    dp_psd_state_t * state,
    size_t n,
    float * out,
    size_t max_out
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `n` Caller buffer capacity (ignored; buffer is pre-sized to nfft). 
* `out` Destination, at least nfft float32 elements. 
* `max_out` Capacity of `out` in elements. Emission stops there, so the return value is the number actually written. 



**Returns:**

min(nfft, max\_out), or 0 if empty. 





        

<hr>



### function dp\_psd\_psd\_db\_max\_out 

_Output capacity hint for psd\_db(); equals nfft._ 
```C++
size_t dp_psd_psd_db_max_out (
    dp_psd_state_t * state
) 
```




<hr>



### function dp\_psd\_psd\_dbhz 

_Averaged power spectral density in dB/Hz, DC-centred. Normalised by_ `fs * sum(w^2)` _(ENBW-aware), the standard one-sided-free PSD scaling. Differs from psd\_db() by the constant_`10*log10(cg^2/(fs*s2))` _. Returns 0 (Python None) before any frame is accumulated._
```C++
size_t dp_psd_psd_dbhz (
    dp_psd_state_t * state,
    size_t n,
    float * out,
    size_t max_out
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



### function dp\_psd\_psd\_dbhz\_max\_out 

_Output capacity hint for psd\_dbhz(); equals nfft._ 
```C++
size_t dp_psd_psd_dbhz_max_out (
    dp_psd_state_t * state
) 
```




<hr>



### function dp\_psd\_reset 

_Discard the running average; the next accumulate re-seeds it._ 
```C++
void dp_psd_reset (
    dp_psd_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 




        

<hr>



### function dp\_psd\_set\_state 

```C++
int dp_psd_set_state (
    dp_psd_state_t * state,
    const void * blob
) 
```




<hr>



### function dp\_psd\_sfdr 

_Spurious-free dynamic range in dB from the two strongest peaks._ 
```C++
double dp_psd_sfdr (
    dp_psd_state_t * state,
    float min_db
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `min_db` Minimum peak level considered, dB. 



**Returns:**

Carrier-minus-highest-spur level in dB (0 if fewer than two peaks). 





        

<hr>



### function dp\_psd\_snr 

_In-band SNR in dB: peak level in_ `[lo_hz, hi_hz]` _minus the noise floor._
```C++
double dp_psd_snr (
    dp_psd_state_t * state,
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



### function dp\_psd\_state\_bytes 

```C++
size_t dp_psd_state_bytes (
    const dp_psd_state_t * state
) 
```




<hr>



### function dp\_psd\_total\_band\_power 

_Total integrated power across all bands in dB._ 
```C++
double dp_psd_total_band_power (
    dp_psd_state_t * state,
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
The documentation for this class was generated from the following file `native/inc/doppler/psd/psd_core.h`

