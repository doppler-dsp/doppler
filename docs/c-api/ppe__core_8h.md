

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
|  [**ppe\_result\_t**](structppe__result__t.md) | [**ppe\_estimate**](#function-ppe_estimate) ([**ppe\_state\_t**](structppe__state__t.md) \* state, const float complex \* in, size\_t n\_in) <br>_Estimate (frequency, chirp rate) of_ `in` _via the coherent surface._ |
|  void | [**ppe\_reset**](#function-ppe_reset) ([**ppe\_state\_t**](structppe__state__t.md) \* state) <br>_No-op (the estimator carries no running state)._  |




























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

_Estimate (frequency, chirp rate) of_ `in` _via the coherent surface._
```C++
ppe_result_t ppe_estimate (
    ppe_state_t * state,
    const float complex * in,
    size_t n_in
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `in` Complex sequence (modulation already stripped by the caller). 
* `n_in` Length, in `[4, max_len]`. 



**Returns:**

The estimate; zeroed if `n_in` is out of range. 





        

<hr>



### function ppe\_reset 

_No-op (the estimator carries no running state)._ 
```C++
void ppe_reset (
    ppe_state_t * state
) 
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/ppe/ppe_core.h`

