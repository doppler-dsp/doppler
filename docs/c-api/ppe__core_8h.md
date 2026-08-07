

# File ppe\_core.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**ppe**](dir_d640b2c624b0e530b2e913b3aa05ce26.md) **>** [**ppe\_core.h**](ppe__core_8h.md)

[Go to the source code of this file](ppe__core_8h_source.md)

_Feedforward polynomial-phase estimator (frequency + chirp rate)._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "jm_perf.h"`
* `#include "fft/fft_core.h"`
* `#include "spectral/spectral_core.h"`
* `#include <complex.h>`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**ppe\_result\_t**](structppe__result__t.md) <br>_Polynomial-phase estimate (one search)._  |
| struct | [**ppe\_state\_t**](structppe__state__t.md) <br>_PolynomialPhaseEstimator state (FFT plan + rate grid + scratch)._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**ppe\_state\_t**](structppe__state__t.md) \* | [**ppe\_create**](#function-ppe_create) (size\_t max\_len, double max\_rate) <br>_Create a polynomial-phase estimator._  |
|  void | [**ppe\_destroy**](#function-ppe_destroy) ([**ppe\_state\_t**](structppe__state__t.md) \* state) <br>_Destroy an estimator._  |
|  [**ppe\_result\_t**](structppe__result__t.md) | [**ppe\_estimate**](#function-ppe_estimate) ([**ppe\_state\_t**](structppe__state__t.md) \* state, const float complex \* x, size\_t n\_in) <br>_Estimate the normalized frequency and chirp rate of a complex segment via the coherent (chirp-rate x frequency) surface._  |
|  void | [**ppe\_reset**](#function-ppe_reset) ([**ppe\_state\_t**](structppe__state__t.md) \* state) <br>_Do nothing — the estimator keeps no running state between calls._  |




























## Detailed Description


Estimates the normalized frequency `f` (cycles/sample) and chirp rate `r` (cycles/sample^2) of a complex sequence by a **coherent 2-D matched-filter search**. For each chirp-rate hypothesis `r_i` in `[-max_rate, +max_rate]` the sequence is dechirped (multiplied by `exp`(-j\*pi\*r\_i\*m^2)) and FFT-ed; the resulting (chirp-rate x frequency) surface peaks at the true (r, f), refined sub-bin in both axes by parabolic interpolation. Being fully coherent it is the matched-filter-optimal estimator (holds at low SNR), and it collapses to a single FFT — pure Doppler — when `max_rate` = 0. One knob therefore spans near-static Doppler through severe LEO chirp.


The caller strips modulation first: data-aided (multiply by conj of the known symbols) keeps full SNR; non-data-aided raises an M-PSK stream to the M-th power (BPSK: square) — which doubles `f` and `r`, so the caller halves them.


Stateless by-value analyzer (the measure-suite pattern). Composes fft\_core + the spectral\_core window / find\_peaks free functions.



```C++
ppe_state_t *p = ppe_create(4096, 0.0);     // Doppler only (single FFT)
ppe_result_t e = ppe_estimate(p, y, n);     // e.freq_norm, e.rate_norm
ppe_destroy(p);
```
 


    
## Public Functions Documentation




### function ppe\_create 

_Create a polynomial-phase estimator._ 
```C++
ppe_state_t * ppe_create (
    size_t max_len,
    double max_rate
) 
```





**Parameters:**


* `max_len` Maximum input sequence length (&gt;= 4). 
* `max_rate` Chirp-rate search half-span (cycles/sample^2); 0 searches frequency only (a single FFT — near-static Doppler). 



**Returns:**

Heap state, or NULL on bad args / allocation failure. 





        

<hr>



### function ppe\_destroy 

_Destroy an estimator._ 
```C++
void ppe_destroy (
    ppe_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function ppe\_estimate 

_Estimate the normalized frequency and chirp rate of a complex segment via the coherent (chirp-rate x frequency) surface._ 
```C++
ppe_result_t ppe_estimate (
    ppe_state_t * state,
    const float complex * x,
    size_t n_in
) 
```



Runs the full 2-D matched-filter search in one shot: for each chirp-rate hypothesis the segment is dechirped and FFT-ed, and the peak of the resulting surface — refined sub-bin by parabolic interpolation on both axes — gives the estimate. With `max_rate` = 0 the rate axis collapses to a single FFT (pure Doppler) and the returned rate is forced to exactly 0.


Feed a segment whose modulation has already been stripped (data-aided by the known symbols, or non-data-aided by the M-th-power trick — remembering that raising to the M-th power scales both returned values by M). The result carries `freq_norm` (cycles/sample), `rate_norm` (cycles/sample^2), and `snr_db` (a rough peak-to-mean confidence).




**Parameters:**


* `state` Estimator handle; must be non-NULL. 
* `x` Complex segment (modulation already stripped by the caller). 
* `n_in` Segment length, in `[4, max_len]`. 



**Returns:**

The estimate; all fields are zeroed if `n_in` is out of range. 
```C++
>>> import numpy as np
>>> from doppler.dsss import PolynomialPhaseEstimator
>>> m = np.arange(512)
>>> f, r = 0.05, 1e-5               # true Doppler + chirp rate
>>> x = np.exp(2j*np.pi*(f*m + 0.5*r*m*m)).astype(np.complex64)
>>> p = PolynomialPhaseEstimator(max_len=512, max_rate=5e-5)
>>> e = p.estimate(x)                        # one-shot coherent search
>>> round(e.freq_norm, 4), round(e.rate_norm, 7)
(0.0501, 1e-05)
```
 





        

<hr>



### function ppe\_reset 

_Do nothing — the estimator keeps no running state between calls._ 
```C++
void ppe_reset (
    ppe_state_t * state
) 
```



A feedforward analyzer computes each estimate purely from the segment it is handed, so there is nothing to clear. The method exists only to satisfy the common object protocol; calling it is always safe and has no effect.




**Parameters:**


* `state` Estimator handle (unused). 
```C++
>>> from doppler.dsss import PolynomialPhaseEstimator
>>> p = PolynomialPhaseEstimator(max_len=512, max_rate=0.0)
>>> p.reset()   # no-op: an estimate depends only on the next
>>> #           segment
```
 




        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/ppe/ppe_core.h`

