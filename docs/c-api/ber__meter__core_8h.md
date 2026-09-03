

# File ber\_meter\_core.h



[**FileList**](files.md) **>** [**ber\_meter**](dir_01b99f726e31084c217a09fa5a432d53.md) **>** [**ber\_meter\_core.h**](ber__meter__core_8h.md)

[Go to the source code of this file](ber__meter__core_8h_source.md)

_BerMeter — the error-rate accumulator._ [More...](#detailed-description)

* `#include "ber/ber_core.h"`
* `#include "clib_common.h"`
* `#include "detection/detection_core.h"`
* `#include "dp_state.h"`
* `#include "jm_perf.h"`
* `#include <complex.h>`
* `#include <stdint.h>`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**ber\_meter\_state\_t**](structber__meter__state__t.md) <br>_BerMeter state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**ber\_align\_t**](structber__align__t.md) | [**ber\_align\_detect**](#function-ber_align_detect) (const float \_Complex \* rx, size\_t rx\_len, const uint8\_t \* truth, size\_t truth\_len, int m, size\_t t0, size\_t n\_marker, size\_t period, int lag\_span, double pfa) <br>_Exact confidence interval for a run stopped on an ERROR count._  |
|  [**ber\_interval\_t**](structber__interval__t.md) | [**ber\_confidence**](#function-ber_confidence) (size\_t errors, size\_t symbols, double conf) <br> |
|  int | [**ber\_meter\_align**](#function-ber_meter_align) ([**ber\_meter\_state\_t**](structber__meter__state__t.md) \* state, const float \_Complex \* rx, size\_t rx\_len, size\_t t0, size\_t n\_marker, size\_t period, int lag\_span, double pfa) <br>_Detect where_ `rx` _sits against truth and REMEMBER that alignment._ |
|  [**ber\_interval\_t**](structber__interval__t.md) | [**ber\_meter\_ber**](#function-ber_meter_ber) (const [**ber\_meter\_state\_t**](structber__meter__state__t.md) \* state) <br>_Gray-coded bit error rate over the scored bits, with its interval._  |
|  [**ber\_meter\_state\_t**](structber__meter__state__t.md) \* | [**ber\_meter\_create**](#function-ber_meter_create) (int m, size\_t target\_errors, double conf) <br>_Error counters accumulated across as many bursts as it takes._  |
|  void | [**ber\_meter\_destroy**](#function-ber_meter_destroy) ([**ber\_meter\_state\_t**](structber__meter__state__t.md) \* state) <br> |
|  [**ber\_align\_t**](structber__align__t.md) | [**ber\_meter\_detect**](#function-ber_meter_detect) (const [**ber\_meter\_state\_t**](structber__meter__state__t.md) \* state, const float \_Complex \* rx, size\_t rx\_len, size\_t t0, size\_t n\_marker, size\_t period, int lag\_span, double pfa) <br>_Pure detection: returns the alignment without touching state._  |
|  double | [**ber\_meter\_get\_align\_margin\_db**](#function-ber_meter_get_align_margin_db) (const [**ber\_meter\_state\_t**](structber__meter__state__t.md) \* state) <br> |
|  size\_t | [**ber\_meter\_get\_align\_occurrences**](#function-ber_meter_get_align_occurrences) (const [**ber\_meter\_state\_t**](structber__meter__state__t.md) \* state) <br> |
|  int | [**ber\_meter\_get\_align\_ok**](#function-ber_meter_get_align_ok) (const [**ber\_meter\_state\_t**](structber__meter__state__t.md) \* state) <br> |
|  double | [**ber\_meter\_get\_align\_runner\_db**](#function-ber_meter_get_align_runner_db) (const [**ber\_meter\_state\_t**](structber__meter__state__t.md) \* state) <br> |
|  int | [**ber\_meter\_get\_align\_saturated**](#function-ber_meter_get_align_saturated) (const [**ber\_meter\_state\_t**](structber__meter__state__t.md) \* state) <br> |
|  size\_t | [**ber\_meter\_get\_align\_slips**](#function-ber_meter_get_align_slips) (const [**ber\_meter\_state\_t**](structber__meter__state__t.md) \* state) <br> |
|  double | [**ber\_meter\_get\_align\_stat**](#function-ber_meter_get_align_stat) (const [**ber\_meter\_state\_t**](structber__meter__state__t.md) \* state) <br> |
|  size\_t | [**ber\_meter\_get\_bit\_errors**](#function-ber_meter_get_bit_errors) (const [**ber\_meter\_state\_t**](structber__meter__state__t.md) \* state) <br> |
|  size\_t | [**ber\_meter\_get\_bits**](#function-ber_meter_get_bits) (const [**ber\_meter\_state\_t**](structber__meter__state__t.md) \* state) <br> |
|  double | [**ber\_meter\_get\_conf**](#function-ber_meter_get_conf) (const [**ber\_meter\_state\_t**](structber__meter__state__t.md) \* state) <br> |
|  int | [**ber\_meter\_get\_enough**](#function-ber_meter_get_enough) (const [**ber\_meter\_state\_t**](structber__meter__state__t.md) \* state) <br>_Has the error target been reached? The inverse-binomial stop._  |
|  size\_t | [**ber\_meter\_get\_errors**](#function-ber_meter_get_errors) (const [**ber\_meter\_state\_t**](structber__meter__state__t.md) \* state) <br> |
|  int | [**ber\_meter\_get\_lag**](#function-ber_meter_get_lag) (const [**ber\_meter\_state\_t**](structber__meter__state__t.md) \* state) <br> |
|  int | [**ber\_meter\_get\_m**](#function-ber_meter_get_m) (const [**ber\_meter\_state\_t**](structber__meter__state__t.md) \* state) <br> |
|  double | [**ber\_meter\_get\_phase**](#function-ber_meter_get_phase) (const [**ber\_meter\_state\_t**](structber__meter__state__t.md) \* state) <br> |
|  size\_t | [**ber\_meter\_get\_skipped**](#function-ber_meter_get_skipped) (const [**ber\_meter\_state\_t**](structber__meter__state__t.md) \* state) <br> |
|  void | [**ber\_meter\_get\_state**](#function-ber_meter_get_state) (const [**ber\_meter\_state\_t**](structber__meter__state__t.md) \* state, void \* blob) <br> |
|  size\_t | [**ber\_meter\_get\_symbols**](#function-ber_meter_get_symbols) (const [**ber\_meter\_state\_t**](structber__meter__state__t.md) \* state) <br> |
|  size\_t | [**ber\_meter\_get\_target\_errors**](#function-ber_meter_get_target_errors) (const [**ber\_meter\_state\_t**](structber__meter__state__t.md) \* state) <br> |
|  [**ber\_interval\_t**](structber__interval__t.md) | [**ber\_meter\_interval**](#function-ber_meter_interval) (const [**ber\_meter\_state\_t**](structber__meter__state__t.md) \* state, size\_t errors, size\_t symbols) <br>_Exact confidence interval for error/trial counts from ELSEWHERE._  |
|  void | [**ber\_meter\_reset**](#function-ber_meter_reset) ([**ber\_meter\_state\_t**](structber__meter__state__t.md) \* state) <br>_Zero the running counters; keep the configuration and the truth._  |
|  size\_t | [**ber\_meter\_score**](#function-ber_meter_score) ([**ber\_meter\_state\_t**](structber__meter__state__t.md) \* state, const float \_Complex \* rx, size\_t rx\_len, size\_t lo, size\_t hi) <br>_Score_ `rx[lo .. hi)` _against the truth and accumulate the counters._ |
|  [**ber\_interval\_t**](structber__interval__t.md) | [**ber\_meter\_ser**](#function-ber_meter_ser) (const [**ber\_meter\_state\_t**](structber__meter__state__t.md) \* state) <br>_Symbol error rate over the scored symbols, with its exact interval._  |
|  void | [**ber\_meter\_set\_align**](#function-ber_meter_set_align) ([**ber\_meter\_state\_t**](structber__meter__state__t.md) \* state, [**ber\_align\_t**](structber__align__t.md) align, size\_t t0, size\_t n\_marker, size\_t period) <br>_Install an alignment detected elsewhere (e.g. by_ [_**ber\_align\_detect()**_](ber__meter__core_8h.md#function-ber_align_detect) _on a different buffer), with the marker geometry that produced it, so_[_**ber\_meter\_score()**_](ber__meter__core_8h.md#function-ber_meter_score) _can use it._ |
|  int | [**ber\_meter\_set\_state**](#function-ber_meter_set_state) ([**ber\_meter\_state\_t**](structber__meter__state__t.md) \* state, const void \* blob) <br> |
|  int | [**ber\_meter\_set\_truth**](#function-ber_meter_set_truth) ([**ber\_meter\_state\_t**](structber__meter__state__t.md) \* state, const uint8\_t \* truth, size\_t truth\_len) <br>_Install the transmitted symbol sequence this meter scores against._  |
|  size\_t | [**ber\_meter\_state\_bytes**](#function-ber_meter_state_bytes) (const [**ber\_meter\_state\_t**](structber__meter__state__t.md) \* state) <br> |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**BER\_METER\_STATE\_MAGIC**](ber__meter__core_8h.md#define-ber_meter_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('B', 'E', 'R', 'M')`<br> |
| define  | [**BER\_METER\_STATE\_VERSION**](ber__meter__core_8h.md#define-ber_meter_state_version)  `1u`<br> |

## Detailed Description


Owns the transmitted reference, the running error counters, the marker-based alignment detector and the exact confidence interval. A caller loops bursts until the ERROR target is met and reads a defensible rate off the end:



```C++
ber_meter_state_t *m = ber_meter_create (4, 200, 0.99);
ber_meter_set_truth (m, truth, nsym);
while (!ber_meter_get_enough (m))
  {
    size_t n = run_receiver (rx);
    ber_align_t a = ber_meter_align (m, rx, n, t0, 0, 0, 0, 0.0);
    if (a.ok)
      ber_meter_score (m, rx, n, lo, n, a.lag, a.phase, t0, 0, 0,
                       a.occurrences);
  }
ber_interval_t ser = ber_meter_ser (m);
ber_meter_destroy (m);
```



The three gates a result has to pass, and why each exists, are on [**ber/ber\_core.h**](ber__core_8h.md). The one rule this file enforces by construction: the alignment handed to [**ber\_meter\_score()**](ber__meter__core_8h.md#function-ber_meter_score) is DETECTED by [**ber\_meter\_align()**](ber__meter__core_8h.md#function-ber_meter_align), never searched by minimising the error count. 


    
## Public Functions Documentation




### function ber\_align\_detect 

_Exact confidence interval for a run stopped on an ERROR count._ 
```C++
ber_align_t ber_align_detect (
    const float _Complex * rx,
    size_t rx_len,
    const uint8_t * truth,
    size_t truth_len,
    int m,
    size_t t0,
    size_t n_marker,
    size_t period,
    int lag_span,
    double pfa
) 
```



Not a module free function because jm free functions cannot return a record; reach it from Python as BerMeter.ser() / .ber().


Both quantiles come from doppler's own inverse regularized incomplete gamma rather than a second copy of one: `det_threshold_noncoherent(q, r)` returns the `b` with `marcum_q(r, 0, b) = q`, and `marcum_q(r, 0, b) = Q(r, b^2/2)`, so `chi2_q(2r)/2 = 0.5 * det_threshold_noncoherent(1-q, r)^2`. At `r = 1` that reduces to the closed form and the interval is `[-ln(1-a/2)/N, -ln(a/2)/N]` — no normal approximation anywhere, so it stays honest at the small error counts where a Wald interval is worst. Verified bit-identical to SciPy's `chi2.ppf` at r = 1, 2, 20, 200 and 1000.


Detect the (lag, phase) alignment of `rx` against a known marker.


The free-function form, needing only the truth sequence — BerMeter.align() is the stateful spelling. The marker is `truth[t0 .. t0 + n_marker)`, optionally repeating every `period` symbols; repeats are combined NON-COHERENTLY, which raises the processing gain and exposes cycle slips. The noise floor is estimated from the off-peak lags themselves (a CFAR reference), so nothing needs to know the Es/N0.


The peak's phase is the ABSOLUTE constellation rotation, so there is no residual M-fold ambiguity left to search: the marker resolves it. That is what removes the `min over rotation` bias from an error count. The processing gain is `sqrt(2*K*L)`, so a marker too short to identify an alignment simply cannot clear the threshold and reports `ok = 0` — the intended behaviour, and the opposite of returning a plausible wrong lag.




**Parameters:**


* `rx` Recovered symbols. 
* `rx_len` How many. 
* `truth` Transmitted symbol indices (0..m-1). 
* `truth_len` How many. 
* `m` Constellation order. 
* `t0` Truth index of the marker's first occurrence. 
* `n_marker` Marker length in symbols; 0 selects BER\_SYNC\_SYMS. 
* `period` Repeat period in symbols; 0 for a single occurrence. 
* `lag_span` Search half-width; 0 selects BER\_LAG\_SPAN. 
* `pfa` Whole-search false-alarm probability; 0 selects 1e-6. 



**Returns:**

The alignment, with `ok` saying whether to believe it. 





        

<hr>



### function ber\_confidence 

```C++
ber_interval_t ber_confidence (
    size_t errors,
    size_t symbols,
    double conf
) 
```




<hr>



### function ber\_meter\_align 

_Detect where_ `rx` _sits against truth and REMEMBER that alignment._
```C++
int ber_meter_align (
    ber_meter_state_t * state,
    const float _Complex * rx,
    size_t rx_len,
    size_t t0,
    size_t n_marker,
    size_t period,
    int lag_span,
    double pfa
) 
```



Correlates the known marker `truth[t0 .. t0 + n_marker)` against `rx` over a span of lags, gates the peak with a false-alarm probability, and stores the winning lag, absolute carrier phase and marker geometry on the meter so score() later uses exactly this detection — never a lag searched to minimise the error count. The peak's phase is the ABSOLUTE constellation rotation, so no M-fold ambiguity is left to resolve; a marker too short to clear the gate reports failure rather than a plausible wrong lag. Read the outcome through `align_ok`, `lag`, `phase` and `align_margin_db`.




**Parameters:**


* `state` Must be non-NULL, with truth installed by set\_truth(). 
* `rx` Recovered symbols to align against the truth. 
* `rx_len` How many recovered symbols. 
* `t0` Truth index of the marker's first occurrence. 
* `n_marker` Marker length in symbols; 0 selects BER\_SYNC\_SYMS. 
* `period` Repeat period in symbols; 0 for a single occurrence. 
* `lag_span` Search half-width in symbols; 0 selects BER\_LAG\_SPAN. 
* `pfa` Whole-search false-alarm probability; 0 selects 1e-6. 



**Returns:**

1 when the detection passed its false-alarm gate, else 0. 
```C++
>>> import numpy as np
>>> from doppler.ber import BerMeter
>>> rng = np.random.default_rng(0)
>>> truth = rng.integers(0, 4, size=600).astype(np.uint8)
>>> ang = 2 * np.pi * truth / 4 + np.pi / 4
>>> rx = np.exp(1j * ang).astype(np.complex64)
>>> met = BerMeter(m=4)
>>> met.set_truth(truth)
>>> met.align(rx, n_marker=64)     # correlate a 64-symbol marker
1
>>> met.lag, met.align_ok          # detected, so score() is valid
(0, 1)
```
 





        

<hr>



### function ber\_meter\_ber 

_Gray-coded bit error rate over the scored bits, with its interval._ 
```C++
ber_interval_t ber_meter_ber (
    const ber_meter_state_t * state
) 
```



The same exact statistics as ser(), counted over Gray-coded bits rather than symbols, so a QPSK/8PSK symbol error contributes as many bit errors as its Gray labels differ by. Assert on `lo`, never on `p_hat`.




**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

A BerInterval record — `(p_hat, lo, hi, rel, conf, errors, symbols)` — the bit error rate with its exact two-sided limits (`errors` and `symbols` are bit counts here). 
```C++
>>> import numpy as np
>>> from doppler.ber import BerMeter
>>> rng = np.random.default_rng(1)
>>> truth = rng.integers(0, 4, size=800).astype(np.uint8)
>>> ang = 2 * np.pi * truth / 4 + np.pi / 4
>>> rx = np.exp(1j * ang).astype(np.complex64)
>>> rx[200:260:5] *= -1            # corrupt 12 symbols
>>> met = BerMeter(m=4)
>>> met.set_truth(truth)
>>> met.align(rx, n_marker=64)
1
>>> _ = met.score(rx, hi=truth.size)
>>> r = met.ber()                  # as ser(), but over bits
>>> r.errors, r.symbols
(24, 1472)
>>> round(r.lo, 4)
0.009
```
 





        

<hr>



### function ber\_meter\_create 

_Error counters accumulated across as many bursts as it takes._ 
```C++
ber_meter_state_t * ber_meter_create (
    int m,
    size_t target_errors,
    double conf
) 
```



Create a meter for constellation `m` stopping at `target_errors`.




**Parameters:**


* `m` Constellation order (2, 4, 8). 
* `target_errors` Inverse-binomial stop condition; 0 selects 200. 
* `conf` Two-sided confidence level; 0 selects 0.99. 




        

<hr>



### function ber\_meter\_destroy 

```C++
void ber_meter_destroy (
    ber_meter_state_t * state
) 
```




<hr>



### function ber\_meter\_detect 

_Pure detection: returns the alignment without touching state._ 
```C++
ber_align_t ber_meter_detect (
    const ber_meter_state_t * state,
    const float _Complex * rx,
    size_t rx_len,
    size_t t0,
    size_t n_marker,
    size_t period,
    int lag_span,
    double pfa
) 
```



[**ber\_meter\_align()**](ber__meter__core_8h.md#function-ber_meter_align) is the stateful spelling the Python binding uses. The marker comes from the truth installed by [**ber\_meter\_set\_truth()**](ber__meter__core_8h.md#function-ber_meter_set_truth).




**Parameters:**


* `state` Must be non-NULL, with truth installed. 
* `rx` Recovered symbols. 
* `rx_len` How many. 
* `t0` Truth index of the marker's first occurrence. 
* `n_marker` Marker length in symbols; 0 selects BER\_SYNC\_SYMS. 
* `period` Repeat period in symbols; 0 for a single occurrence. 
* `lag_span` Search half-width; 0 selects BER\_LAG\_SPAN. 
* `pfa` Whole-search false-alarm probability; 0 selects 1e-6. 



**Returns:**

The alignment, with `ok` saying whether to believe it. 





        

<hr>



### function ber\_meter\_get\_align\_margin\_db 

```C++
double ber_meter_get_align_margin_db (
    const ber_meter_state_t * state
) 
```




<hr>



### function ber\_meter\_get\_align\_occurrences 

```C++
size_t ber_meter_get_align_occurrences (
    const ber_meter_state_t * state
) 
```




<hr>



### function ber\_meter\_get\_align\_ok 

```C++
int ber_meter_get_align_ok (
    const ber_meter_state_t * state
) 
```




<hr>



### function ber\_meter\_get\_align\_runner\_db 

```C++
double ber_meter_get_align_runner_db (
    const ber_meter_state_t * state
) 
```




<hr>



### function ber\_meter\_get\_align\_saturated 

```C++
int ber_meter_get_align_saturated (
    const ber_meter_state_t * state
) 
```




<hr>



### function ber\_meter\_get\_align\_slips 

```C++
size_t ber_meter_get_align_slips (
    const ber_meter_state_t * state
) 
```




<hr>



### function ber\_meter\_get\_align\_stat 

```C++
double ber_meter_get_align_stat (
    const ber_meter_state_t * state
) 
```




<hr>



### function ber\_meter\_get\_bit\_errors 

```C++
size_t ber_meter_get_bit_errors (
    const ber_meter_state_t * state
) 
```




<hr>



### function ber\_meter\_get\_bits 

```C++
size_t ber_meter_get_bits (
    const ber_meter_state_t * state
) 
```




<hr>



### function ber\_meter\_get\_conf 

```C++
double ber_meter_get_conf (
    const ber_meter_state_t * state
) 
```




<hr>



### function ber\_meter\_get\_enough 

_Has the error target been reached? The inverse-binomial stop._ 
```C++
int ber_meter_get_enough (
    const ber_meter_state_t * state
) 
```




<hr>



### function ber\_meter\_get\_errors 

```C++
size_t ber_meter_get_errors (
    const ber_meter_state_t * state
) 
```




<hr>



### function ber\_meter\_get\_lag 

```C++
int ber_meter_get_lag (
    const ber_meter_state_t * state
) 
```




<hr>



### function ber\_meter\_get\_m 

```C++
int ber_meter_get_m (
    const ber_meter_state_t * state
) 
```




<hr>



### function ber\_meter\_get\_phase 

```C++
double ber_meter_get_phase (
    const ber_meter_state_t * state
) 
```




<hr>



### function ber\_meter\_get\_skipped 

```C++
size_t ber_meter_get_skipped (
    const ber_meter_state_t * state
) 
```




<hr>



### function ber\_meter\_get\_state 

```C++
void ber_meter_get_state (
    const ber_meter_state_t * state,
    void * blob
) 
```




<hr>



### function ber\_meter\_get\_symbols 

```C++
size_t ber_meter_get_symbols (
    const ber_meter_state_t * state
) 
```




<hr>



### function ber\_meter\_get\_target\_errors 

```C++
size_t ber_meter_get_target_errors (
    const ber_meter_state_t * state
) 
```




<hr>



### function ber\_meter\_interval 

_Exact confidence interval for error/trial counts from ELSEWHERE._ 
```C++
ber_interval_t ber_meter_interval (
    const ber_meter_state_t * state,
    size_t errors,
    size_t symbols
) 
```



The pure-function face of the meter's statistics, at this meter's own confidence level: hand it any error and trial counts and it returns the same exact Gamma/chi-square interval ser() would, with quantiles from doppler's own inverse regularized incomplete gamma rather than a normal approximation, so it stays honest at the small error counts where a Wald interval is worst. Assert on `lo`, never on `p_hat`.




**Parameters:**


* `state` Must be non-NULL. 
* `errors` Errors counted, `r`. 
* `symbols` Trials counted, `N` (symbols, or bits for a BER). 



**Returns:**

A BerInterval record — `(p_hat, lo, hi, rel, conf, errors, symbols)` — the unbiased rate with its exact two-sided limits. 
```C++
>>> from doppler.ber import BerMeter
>>> met = BerMeter(m=4, conf=0.99)
>>> ci = met.interval(errors=8, symbols=20000)   # external counts
>>> round(ci.p_hat, 6), round(ci.lo, 6), round(ci.hi, 6)
(0.00035, 0.000129, 0.000857)
```
 





        

<hr>



### function ber\_meter\_reset 

_Zero the running counters; keep the configuration and the truth._ 
```C++
void ber_meter_reset (
    ber_meter_state_t * state
) 
```



Returns the meter to a fresh count while preserving `m`, the error target, the confidence level and the installed truth sequence, so one meter can measure independent captures back to back without reinstalling truth. The last detected alignment is left untouched; call align() again for the next capture before scoring it.




**Parameters:**


* `state` Must be non-NULL. 
```C++
>>> import numpy as np
>>> from doppler.ber import BerMeter
>>> rng = np.random.default_rng(0)
>>> truth = rng.integers(0, 4, size=400).astype(np.uint8)
>>> ang = 2 * np.pi * truth / 4 + np.pi / 4
>>> rx = np.exp(1j * ang).astype(np.complex64)
>>> met = BerMeter(m=4)
>>> met.set_truth(truth)
>>> met.align(rx, n_marker=64)
1
>>> met.score(rx, hi=truth.size)
336
>>> met.symbols
336
>>> met.reset()               # reuse the meter for the next capture
>>> (met.errors, met.symbols)
(0, 0)
```
 




        

<hr>



### function ber\_meter\_score 

_Score_ `rx[lo .. hi)` _against the truth and accumulate the counters._
```C++
size_t ber_meter_score (
    ber_meter_state_t * state,
    const float _Complex * rx,
    size_t rx_len,
    size_t lo,
    size_t hi
) 
```



Demodulates each symbol in the window under the alignment the last align() detected — its lag and absolute phase — and tallies symbol and Gray-coded bit errors against the installed truth. The alignment is used VERBATIM: no lag search, no rotation search, no minimisation of any kind over the answer. Symbols covered by a marker occurrence are excluded, as are any whose truth index falls outside the installed sequence; both land in `skipped`.




**Parameters:**


* `state` Must be non-NULL, with an alignment set by align(). 
* `rx` Recovered symbols to score. 
* `rx_len` How many recovered symbols. 
* `lo` First symbol index to score (inclusive). 
* `hi` One past the last symbol index to score; clamped to rx\_len. `hi = 0` scores nothing, so pass the window's true end. 



**Returns:**

Symbols actually scored (window length minus skipped symbols). 
```C++
>>> import numpy as np
>>> from doppler.ber import BerMeter
>>> rng = np.random.default_rng(1)
>>> truth = rng.integers(0, 4, size=800).astype(np.uint8)
>>> ang = 2 * np.pi * truth / 4 + np.pi / 4
>>> rx = np.exp(1j * ang).astype(np.complex64)
>>> met = BerMeter(m=4)
>>> met.set_truth(truth)
>>> met.align(rx, n_marker=64)
1
>>> met.score(rx, hi=truth.size)   # the 64 marker symbols are excluded
736
>>> met.errors, met.skipped
(0, 64)
```
 





        

<hr>



### function ber\_meter\_ser 

_Symbol error rate over the scored symbols, with its exact interval._ 
```C++
ber_interval_t ber_meter_ser (
    const ber_meter_state_t * state
) 
```



Divides the accumulated symbol-error count by the symbols scored and wraps it in the exact Gamma/chi-square interval for inverse binomial sampling — no normal approximation anywhere. Assert on `lo`, never on `p_hat:` comparing the lower limit against a spec is the form that cannot flake on counting noise.




**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

A BerInterval record — `(p_hat, lo, hi, rel, conf, errors, symbols)` — the symbol error rate with its exact two-sided limits. 
```C++
>>> import numpy as np
>>> from doppler.ber import BerMeter
>>> rng = np.random.default_rng(1)
>>> truth = rng.integers(0, 4, size=800).astype(np.uint8)
>>> ang = 2 * np.pi * truth / 4 + np.pi / 4
>>> rx = np.exp(1j * ang).astype(np.complex64)
>>> rx[200:260:5] *= -1            # corrupt 12 symbols (pi rotation)
>>> met = BerMeter(m=4)
>>> met.set_truth(truth)
>>> met.align(rx, n_marker=64)
1
>>> _ = met.score(rx, hi=truth.size)
>>> r = met.ser()
>>> r.errors, r.symbols
(12, 736)
>>> round(r.lo, 4)                 # assert on lo, never on p_hat
0.0067
```
 





        

<hr>



### function ber\_meter\_set\_align 

_Install an alignment detected elsewhere (e.g. by_ [_**ber\_align\_detect()**_](ber__meter__core_8h.md#function-ber_align_detect) _on a different buffer), with the marker geometry that produced it, so_[_**ber\_meter\_score()**_](ber__meter__core_8h.md#function-ber_meter_score) _can use it._
```C++
void ber_meter_set_align (
    ber_meter_state_t * state,
    ber_align_t align,
    size_t t0,
    size_t n_marker,
    size_t period
) 
```



The stateful [**ber\_meter\_align()**](ber__meter__core_8h.md#function-ber_meter_align) is the usual path; this exists for the case where detection and scoring run over different buffers. It is deliberately the ONLY way to set an alignment other than detecting one — score() never takes a lag from its caller, because a lag that was passed in is a lag that could have been searched for. 


        

<hr>



### function ber\_meter\_set\_state 

```C++
int ber_meter_set_state (
    ber_meter_state_t * state,
    const void * blob
) 
```




<hr>



### function ber\_meter\_set\_truth 

_Install the transmitted symbol sequence this meter scores against._ 
```C++
int ber_meter_set_truth (
    ber_meter_state_t * state,
    const uint8_t * truth,
    size_t truth_len
) 
```



Copied, so the caller's buffer need not outlive the call, and reused across every burst. Values are symbol INDICES in `0..m-1` (not Gray labels): the meter Gray-encodes each side itself when it counts bit errors, so handing it Gray labels would double-encode and inflate the rate.




**Parameters:**


* `state` Must be non-NULL. 
* `truth` Transmitted symbol indices, each in `0..m-1`. 
* `truth_len` How many symbols the reference holds. 



**Returns:**

DP\_OK, or DP\_ERR\_INVALID if any index is outside `0..m-1`. 
```C++
>>> import numpy as np
>>> from doppler.ber import BerMeter
>>> met = BerMeter(m=4)
>>> truth = np.array(
...     [0, 3, 1, 2, 2, 0], dtype=np.uint8)  # indices, 0..3
>>> met.set_truth(truth)
>>> met.set_truth(np.array([9], dtype=np.uint8))  # 9 is not in 0..3
Traceback (most recent call last):
ValueError: set_truth failed (rc=-4)
```
 





        

<hr>



### function ber\_meter\_state\_bytes 

```C++
size_t ber_meter_state_bytes (
    const ber_meter_state_t * state
) 
```




<hr>
## Macro Definition Documentation





### define BER\_METER\_STATE\_MAGIC 

```C++
#define BER_METER_STATE_MAGIC `DP_FOURCC ('B', 'E', 'R', 'M')`
```




<hr>



### define BER\_METER\_STATE\_VERSION 

```C++
#define BER_METER_STATE_VERSION `1u`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/ber_meter/ber_meter_core.h`

