

# File ber\_meter\_core.h



[**FileList**](files.md) **>** [**ber\_meter**](dir_88fe8aa1742881a2471cfb33f972e762.md) **>** [**ber\_meter\_core.h**](ber__meter__core_8h.md)

[Go to the source code of this file](ber__meter__core_8h_source.md)

_BerMeter — the error-rate accumulator._ [More...](#detailed-description)

* `#include "doppler/ber/ber_core.h"`
* `#include "doppler/clib_common.h"`
* `#include "doppler/detection/detection_core.h"`
* `#include "doppler/dp_state.h"`
* `#include "doppler/jm_perf.h"`
* `#include "doppler/dp_complex.h"`
* `#include <stdint.h>`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**dp\_ber\_meter\_state\_t**](structdp__ber__meter__state__t.md) <br>_BerMeter state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**ber\_align\_t**](structber__align__t.md) | [**dp\_ber\_align\_detect**](#function-dp_ber_align_detect) (const float \_Complex \* rx, size\_t rx\_len, const uint8\_t \* truth, size\_t truth\_len, int m, size\_t t0, size\_t n\_marker, size\_t period, int lag\_span, double pfa) <br>_Detect the (lag, phase) alignment of_ `rx` _against a known marker._ |
|  int | [**dp\_ber\_meter\_align**](#function-dp_ber_meter_align) ([**dp\_ber\_meter\_state\_t**](structdp__ber__meter__state__t.md) \* state, const float \_Complex \* rx, size\_t rx\_len, size\_t t0, size\_t n\_marker, size\_t period, int lag\_span, double pfa) <br>_Detect where_ `rx` _sits against truth and REMEMBER that alignment._ |
|  [**ber\_interval\_t**](structber__interval__t.md) | [**dp\_ber\_meter\_ber**](#function-dp_ber_meter_ber) (const [**dp\_ber\_meter\_state\_t**](structdp__ber__meter__state__t.md) \* state) <br>_Gray-coded bit error rate over the scored bits, with its interval._  |
|  [**dp\_ber\_meter\_state\_t**](structdp__ber__meter__state__t.md) \* | [**dp\_ber\_meter\_create**](#function-dp_ber_meter_create) (int m, size\_t target\_errors, double conf) <br>_Create a meter for constellation_ `m` _stopping at_`target_errors` _._ |
|  void | [**dp\_ber\_meter\_destroy**](#function-dp_ber_meter_destroy) ([**dp\_ber\_meter\_state\_t**](structdp__ber__meter__state__t.md) \* state) <br>_Free a meter and its owned truth copy._  |
|  [**ber\_align\_t**](structber__align__t.md) | [**dp\_ber\_meter\_detect**](#function-dp_ber_meter_detect) (const [**dp\_ber\_meter\_state\_t**](structdp__ber__meter__state__t.md) \* state, const float \_Complex \* rx, size\_t rx\_len, size\_t t0, size\_t n\_marker, size\_t period, int lag\_span, double pfa) <br>_Pure detection: returns the alignment without touching state._  |
|  double | [**dp\_ber\_meter\_get\_align\_margin\_db**](#function-dp_ber_meter_get_align_margin_db) (const [**dp\_ber\_meter\_state\_t**](structdp__ber__meter__state__t.md) \* state) <br>_Headroom of the stored alignment over its threshold._  |
|  size\_t | [**dp\_ber\_meter\_get\_align\_occurrences**](#function-dp_ber_meter_get_align_occurrences) (const [**dp\_ber\_meter\_state\_t**](structdp__ber__meter__state__t.md) \* state) <br>_Marker occurrences combined by the stored alignment._  |
|  int | [**dp\_ber\_meter\_get\_align\_ok**](#function-dp_ber_meter_get_align_ok) (const [**dp\_ber\_meter\_state\_t**](structdp__ber__meter__state__t.md) \* state) <br>_Is the stored alignment trustworthy?_  |
|  double | [**dp\_ber\_meter\_get\_align\_runner\_db**](#function-dp_ber_meter_get_align_runner_db) (const [**dp\_ber\_meter\_state\_t**](structdp__ber__meter__state__t.md) \* state) <br>_Peak over runner-up of the stored alignment, dB._  |
|  int | [**dp\_ber\_meter\_get\_align\_saturated**](#function-dp_ber_meter_get_align_saturated) (const [**dp\_ber\_meter\_state\_t**](structdp__ber__meter__state__t.md) \* state) <br>_Did the stored alignment's peak sit on a search edge?_  |
|  size\_t | [**dp\_ber\_meter\_get\_align\_slips**](#function-dp_ber_meter_get_align_slips) (const [**dp\_ber\_meter\_state\_t**](structdp__ber__meter__state__t.md) \* state) <br>_Cycle slips seen by the stored alignment._  |
|  double | [**dp\_ber\_meter\_get\_align\_stat**](#function-dp_ber_meter_get_align_stat) (const [**dp\_ber\_meter\_state\_t**](structdp__ber__meter__state__t.md) \* state) <br>_Detection statistic at the stored alignment's peak._  |
|  size\_t | [**dp\_ber\_meter\_get\_bit\_errors**](#function-dp_ber_meter_get_bit_errors) (const [**dp\_ber\_meter\_state\_t**](structdp__ber__meter__state__t.md) \* state) <br>_Gray-coded bit errors counted so far._  |
|  size\_t | [**dp\_ber\_meter\_get\_bits**](#function-dp_ber_meter_get_bits) (const [**dp\_ber\_meter\_state\_t**](structdp__ber__meter__state__t.md) \* state) <br>_Bits scored so far._  |
|  double | [**dp\_ber\_meter\_get\_conf**](#function-dp_ber_meter_get_conf) (const [**dp\_ber\_meter\_state\_t**](structdp__ber__meter__state__t.md) \* state) <br>_Two-sided confidence level used by ser() and ber()._  |
|  int | [**dp\_ber\_meter\_get\_enough**](#function-dp_ber_meter_get_enough) (const [**dp\_ber\_meter\_state\_t**](structdp__ber__meter__state__t.md) \* state) <br>_Has the error target been reached? The inverse-binomial stop._  |
|  size\_t | [**dp\_ber\_meter\_get\_errors**](#function-dp_ber_meter_get_errors) (const [**dp\_ber\_meter\_state\_t**](structdp__ber__meter__state__t.md) \* state) <br>_Symbol errors counted so far._  |
|  int | [**dp\_ber\_meter\_get\_lag**](#function-dp_ber_meter_get_lag) (const [**dp\_ber\_meter\_state\_t**](structdp__ber__meter__state__t.md) \* state) <br>_Lag of the stored alignment._  |
|  int | [**dp\_ber\_meter\_get\_m**](#function-dp_ber_meter_get_m) (const [**dp\_ber\_meter\_state\_t**](structdp__ber__meter__state__t.md) \* state) <br>_Constellation order._  |
|  double | [**dp\_ber\_meter\_get\_phase**](#function-dp_ber_meter_get_phase) (const [**dp\_ber\_meter\_state\_t**](structdp__ber__meter__state__t.md) \* state) <br>_Absolute constellation rotation of the stored alignment._  |
|  size\_t | [**dp\_ber\_meter\_get\_skipped**](#function-dp_ber_meter_get_skipped) (const [**dp\_ber\_meter\_state\_t**](structdp__ber__meter__state__t.md) \* state) <br>_Symbols skipped by score()._  |
|  void | [**dp\_ber\_meter\_get\_state**](#function-dp_ber_meter_get_state) (const [**dp\_ber\_meter\_state\_t**](structdp__ber__meter__state__t.md) \* state, void \* blob) <br>_Serialize the six running counters._  |
|  size\_t | [**dp\_ber\_meter\_get\_symbols**](#function-dp_ber_meter_get_symbols) (const [**dp\_ber\_meter\_state\_t**](structdp__ber__meter__state__t.md) \* state) <br>_Symbols scored so far._  |
|  size\_t | [**dp\_ber\_meter\_get\_target\_errors**](#function-dp_ber_meter_get_target_errors) (const [**dp\_ber\_meter\_state\_t**](structdp__ber__meter__state__t.md) \* state) <br>_The inverse-binomial stop condition._  |
|  [**ber\_interval\_t**](structber__interval__t.md) | [**dp\_ber\_meter\_interval**](#function-dp_ber_meter_interval) (const [**dp\_ber\_meter\_state\_t**](structdp__ber__meter__state__t.md) \* state, size\_t errors, size\_t symbols) <br>_Exact confidence interval for error/trial counts from ELSEWHERE._  |
|  void | [**dp\_ber\_meter\_reset**](#function-dp_ber_meter_reset) ([**dp\_ber\_meter\_state\_t**](structdp__ber__meter__state__t.md) \* state) <br>_Zero the running counters; keep the configuration and the truth._  |
|  size\_t | [**dp\_ber\_meter\_score**](#function-dp_ber_meter_score) ([**dp\_ber\_meter\_state\_t**](structdp__ber__meter__state__t.md) \* state, const float \_Complex \* rx, size\_t rx\_len, size\_t lo, size\_t hi) <br>_Score_ `rx[lo .. hi)` _against the truth and accumulate the counters._ |
|  [**ber\_interval\_t**](structber__interval__t.md) | [**dp\_ber\_meter\_ser**](#function-dp_ber_meter_ser) (const [**dp\_ber\_meter\_state\_t**](structdp__ber__meter__state__t.md) \* state) <br>_Symbol error rate over the scored symbols, with its exact interval._  |
|  void | [**dp\_ber\_meter\_set\_align**](#function-dp_ber_meter_set_align) ([**dp\_ber\_meter\_state\_t**](structdp__ber__meter__state__t.md) \* state, [**ber\_align\_t**](structber__align__t.md) align, size\_t t0, size\_t n\_marker, size\_t period) <br>_Install an alignment detected elsewhere (e.g. by_ [_**dp\_ber\_align\_detect()**_](ber__meter__core_8h.md#function-dp_ber_align_detect) _on a different buffer), with the marker geometry that produced it, so_[_**dp\_ber\_meter\_score()**_](ber__meter__core_8h.md#function-dp_ber_meter_score) _can use it._ |
|  int | [**dp\_ber\_meter\_set\_state**](#function-dp_ber_meter_set_state) ([**dp\_ber\_meter\_state\_t**](structdp__ber__meter__state__t.md) \* state, const void \* blob) <br>_Restore the six running counters from a blob._  |
|  int | [**dp\_ber\_meter\_set\_truth**](#function-dp_ber_meter_set_truth) ([**dp\_ber\_meter\_state\_t**](structdp__ber__meter__state__t.md) \* state, const uint8\_t \* truth, size\_t truth\_len) <br>_Install the transmitted symbol sequence this meter scores against._  |
|  size\_t | [**dp\_ber\_meter\_state\_bytes**](#function-dp_ber_meter_state_bytes) (const [**dp\_ber\_meter\_state\_t**](structdp__ber__meter__state__t.md) \* state) <br>_Size in bytes of the meter's serialized state blob._  |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**BER\_METER\_STATE\_MAGIC**](ber__meter__core_8h.md#define-ber_meter_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('B', 'E', 'R', 'M')`<br>_State-blob magic tag for a BerMeter ("BERM")._  |
| define  | [**BER\_METER\_STATE\_VERSION**](ber__meter__core_8h.md#define-ber_meter_state_version)  `1u`<br>_State-blob layout version for a BerMeter._  |

## Detailed Description


Owns the transmitted reference, the running error counters, the marker-based alignment detector and the exact confidence interval. A caller loops bursts until the ERROR target is met and reads a defensible rate off the end:



```C++
dp_ber_meter_state_t *m = dp_ber_meter_create (4, 200, 0.99);
dp_ber_meter_set_truth (m, truth, nsym);
while (!dp_ber_meter_get_enough (m))
  {
    size_t n = run_receiver (rx);
    ber_align_t a = dp_ber_meter_align (m, rx, n, t0, 0, 0, 0, 0.0);
    if (a.ok)
      dp_ber_meter_score (m, rx, n, lo, n, a.lag, a.phase, t0, 0, 0,
                       a.occurrences);
  }
ber_interval_t ser = dp_ber_meter_ser (m);
dp_ber_meter_destroy (m);
```



The three gates a result has to pass, and why each exists, are on [**ber/ber\_core.h**](ber__core_8h.md). The one rule this file enforces by construction: the alignment handed to [**dp\_ber\_meter\_score()**](ber__meter__core_8h.md#function-dp_ber_meter_score) is DETECTED by [**dp\_ber\_meter\_align()**](ber__meter__core_8h.md#function-dp_ber_meter_align), never searched by minimising the error count. 


    
## Public Functions Documentation




### function dp\_ber\_align\_detect 

_Detect the (lag, phase) alignment of_ `rx` _against a known marker._
```C++
ber_align_t dp_ber_align_detect (
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



The free-function form, needing only the truth sequence — BerMeter.align() is the stateful spelling. The marker is `truth[t0 .. t0 + n_marker)`, optionally repeating every `period` symbols; repeats are combined NON-COHERENTLY, which raises the processing gain and exposes cycle slips. The noise floor is estimated from the off-peak lags themselves (a CFAR reference), so nothing needs to know the Es/N0.


The peak's phase is the ABSOLUTE constellation rotation, so there is no residual M-fold ambiguity left to search: the marker resolves it. That is what removes the `min over rotation` bias from an error count. The processing gain is `sqrt(2*K*L)`, so a marker too short to identify an alignment simply cannot clear the threshold and reports `ok = 0` — the intended behaviour, and the opposite of returning a plausible wrong lag.


The threshold is set on the statistic's null, which depends on `m`. For m &gt;= 3 a marker against unrelated symbols is circular, and the null is `marcum_q(K, 0, R)`. At m = 2 both are +/-1 on one axis, the null is rank-one  chi-square with K degrees of freedom on R^2/2, heavier-tailed at the same mean  and the threshold is set on that, exactly for one occurrence or an even count and conservatively for an odd count above one. Set as circular, a BPSK search false-alarmed about a thousand times its stated Pfa (doppler#2496).




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



### function dp\_ber\_meter\_align 

_Detect where_ `rx` _sits against truth and REMEMBER that alignment._
```C++
int dp_ber_meter_align (
    dp_ber_meter_state_t * state,
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



### function dp\_ber\_meter\_ber 

_Gray-coded bit error rate over the scored bits, with its interval._ 
```C++
ber_interval_t dp_ber_meter_ber (
    const dp_ber_meter_state_t * state
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



### function dp\_ber\_meter\_create 

_Create a meter for constellation_ `m` _stopping at_`target_errors` _._
```C++
dp_ber_meter_state_t * dp_ber_meter_create (
    int m,
    size_t target_errors,
    double conf
) 
```





**Parameters:**


* `m` Constellation order (2, 4, 8). 
* `target_errors` Inverse-binomial stop condition; 0 selects 200. 
* `conf` Two-sided confidence level; 0 selects 0.99. 



**Returns:**

Heap-allocated meter with no truth installed and all counters and the stored alignment zeroed (so `align_ok` reads 0 and `lag`/`phase` read 0 before the first align()), or NULL when `m` is not 2, 4 or 8 or `conf` is outside (0, 1) and not 0. Release with [**dp\_ber\_meter\_destroy()**](ber__meter__core_8h.md#function-dp_ber_meter_destroy). 





        

<hr>



### function dp\_ber\_meter\_destroy 

_Free a meter and its owned truth copy._ 
```C++
void dp_ber_meter_destroy (
    dp_ber_meter_state_t * state
) 
```





**Parameters:**


* `state` Meter from [**dp\_ber\_meter\_create()**](ber__meter__core_8h.md#function-dp_ber_meter_create); NULL is a no-op. 




        

<hr>



### function dp\_ber\_meter\_detect 

_Pure detection: returns the alignment without touching state._ 
```C++
ber_align_t dp_ber_meter_detect (
    const dp_ber_meter_state_t * state,
    const float _Complex * rx,
    size_t rx_len,
    size_t t0,
    size_t n_marker,
    size_t period,
    int lag_span,
    double pfa
) 
```



[**dp\_ber\_meter\_align()**](ber__meter__core_8h.md#function-dp_ber_meter_align) is the stateful spelling the Python binding uses. The marker comes from the truth installed by [**dp\_ber\_meter\_set\_truth()**](ber__meter__core_8h.md#function-dp_ber_meter_set_truth).




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



### function dp\_ber\_meter\_get\_align\_margin\_db 

_Headroom of the stored alignment over its threshold._ 
```C++
double dp_ber_meter_get_align_margin_db (
    const dp_ber_meter_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

`20*log10(stat/threshold)` in dB; -inf when a detection found no usable peak, 0 before the first align(). 





        

<hr>



### function dp\_ber\_meter\_get\_align\_occurrences 

_Marker occurrences combined by the stored alignment._ 
```C++
size_t dp_ber_meter_get_align_occurrences (
    const dp_ber_meter_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Occurrence count K; 0 before the first align() and on a failed search. 





        

<hr>



### function dp\_ber\_meter\_get\_align\_ok 

_Is the stored alignment trustworthy?_ 
```C++
int dp_ber_meter_get_align_ok (
    const dp_ber_meter_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

1 when the peak beat its threshold, was not on a search edge and stood at least 3 dB over the runner-up, else 0 (including before the first align()). 





        

<hr>



### function dp\_ber\_meter\_get\_align\_runner\_db 

_Peak over runner-up of the stored alignment, dB._ 
```C++
double dp_ber_meter_get_align_runner_db (
    const dp_ber_meter_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Ambiguity margin in dB (+inf when every off-peak lag is zero); 0 before the first align() and on a failed search. 





        

<hr>



### function dp\_ber\_meter\_get\_align\_saturated 

_Did the stored alignment's peak sit on a search edge?_ 
```C++
int dp_ber_meter_get_align_saturated (
    const dp_ber_meter_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

1 when the peak was at +/-lag\_span (widen the span), else 0. 





        

<hr>



### function dp\_ber\_meter\_get\_align\_slips 

_Cycle slips seen by the stored alignment._ 
```C++
size_t dp_ber_meter_get_align_slips (
    const dp_ber_meter_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Occurrences whose phase differed from the peak's by more than pi/m; 0 before the first align(). 





        

<hr>



### function dp\_ber\_meter\_get\_align\_stat 

_Detection statistic at the stored alignment's peak._ 
```C++
double dp_ber_meter_get_align_stat (
    const dp_ber_meter_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

The normalized statistic R (dimensionless); 0 before the first align() and on a detection that found no peak. 





        

<hr>



### function dp\_ber\_meter\_get\_bit\_errors 

_Gray-coded bit errors counted so far._ 
```C++
size_t dp_ber_meter_get_bit_errors (
    const dp_ber_meter_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Accumulated bit-error count; 0 after create() or reset(). 





        

<hr>



### function dp\_ber\_meter\_get\_bits 

_Bits scored so far._ 
```C++
size_t dp_ber_meter_get_bits (
    const dp_ber_meter_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Scored symbols times log2(m); 0 after create() or reset(). 





        

<hr>



### function dp\_ber\_meter\_get\_conf 

_Two-sided confidence level used by ser() and ber()._ 
```C++
double dp_ber_meter_get_conf (
    const dp_ber_meter_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

create()'s level, or 0.99 when it was 0. 





        

<hr>



### function dp\_ber\_meter\_get\_enough 

_Has the error target been reached? The inverse-binomial stop._ 
```C++
int dp_ber_meter_get_enough (
    const dp_ber_meter_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

1 once the symbol-error count has reached target\_errors, else 0. Bit errors do not count toward it. 





        

<hr>



### function dp\_ber\_meter\_get\_errors 

_Symbol errors counted so far._ 
```C++
size_t dp_ber_meter_get_errors (
    const dp_ber_meter_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Accumulated symbol-error count; 0 after create() or reset(). 





        

<hr>



### function dp\_ber\_meter\_get\_lag 

_Lag of the stored alignment._ 
```C++
int dp_ber_meter_get_lag (
    const dp_ber_meter_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

`rx[i]` carries `truth[i + lag]`, in symbols; 0 before the first align() or set\_align(). 





        

<hr>



### function dp\_ber\_meter\_get\_m 

_Constellation order._ 
```C++
int dp_ber_meter_get_m (
    const dp_ber_meter_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

The order passed to create(): 2, 4 or 8. 





        

<hr>



### function dp\_ber\_meter\_get\_phase 

_Absolute constellation rotation of the stored alignment._ 
```C++
double dp_ber_meter_get_phase (
    const dp_ber_meter_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Residual rotation in radians, between -pi and pi; 0 before the first align() or set\_align(). 





        

<hr>



### function dp\_ber\_meter\_get\_skipped 

_Symbols skipped by score()._ 
```C++
size_t dp_ber_meter_get_skipped (
    const dp_ber_meter_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Symbols excluded because a marker occurrence covered them or their truth index fell outside the installed sequence. 





        

<hr>



### function dp\_ber\_meter\_get\_state 

_Serialize the six running counters._ 
```C++
void dp_ber_meter_get_state (
    const dp_ber_meter_state_t * state,
    void * blob
) 
```



Configuration (m, target\_errors, conf), the truth sequence and the stored alignment are not included.




**Parameters:**


* `state` Must be non-NULL. 
* `blob` Destination, at least [**dp\_ber\_meter\_state\_bytes()**](ber__meter__core_8h.md#function-dp_ber_meter_state_bytes) bytes. 




        

<hr>



### function dp\_ber\_meter\_get\_symbols 

_Symbols scored so far._ 
```C++
size_t dp_ber_meter_get_symbols (
    const dp_ber_meter_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Accumulated scored-symbol count; 0 after create() or reset(). 





        

<hr>



### function dp\_ber\_meter\_get\_target\_errors 

_The inverse-binomial stop condition._ 
```C++
size_t dp_ber_meter_get_target_errors (
    const dp_ber_meter_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Symbol-error target: create()'s value, or 200 when it was 0. 





        

<hr>



### function dp\_ber\_meter\_interval 

_Exact confidence interval for error/trial counts from ELSEWHERE._ 
```C++
ber_interval_t dp_ber_meter_interval (
    const dp_ber_meter_state_t * state,
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



### function dp\_ber\_meter\_reset 

_Zero the running counters; keep the configuration and the truth._ 
```C++
void dp_ber_meter_reset (
    dp_ber_meter_state_t * state
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



### function dp\_ber\_meter\_score 

_Score_ `rx[lo .. hi)` _against the truth and accumulate the counters._
```C++
size_t dp_ber_meter_score (
    dp_ber_meter_state_t * state,
    const float _Complex * rx,
    size_t rx_len,
    size_t lo,
    size_t hi
) 
```



Demodulates each symbol in the window under the alignment the last align() detected — its lag and absolute phase — and tallies symbol and Gray-coded bit errors against the installed truth. The alignment is used VERBATIM: no lag search, no rotation search, no minimisation of any kind over the answer. Symbols covered by a marker occurrence are excluded, as are any whose truth index falls outside the installed sequence; both land in `skipped`.




**Parameters:**


* `state` Must be non-NULL. Without a usable alignment  before any align(), or after one that failed (`ok == 0`), unless set\_align() installed one since  nothing is scored, the counters are untouched and 0 is returned, rather than scoring at lag 0, phase 0. With no truth installed (or a NULL `rx`) nothing is scored and 0 is returned either. 
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



### function dp\_ber\_meter\_ser 

_Symbol error rate over the scored symbols, with its exact interval._ 
```C++
ber_interval_t dp_ber_meter_ser (
    const dp_ber_meter_state_t * state
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



### function dp\_ber\_meter\_set\_align 

_Install an alignment detected elsewhere (e.g. by_ [_**dp\_ber\_align\_detect()**_](ber__meter__core_8h.md#function-dp_ber_align_detect) _on a different buffer), with the marker geometry that produced it, so_[_**dp\_ber\_meter\_score()**_](ber__meter__core_8h.md#function-dp_ber_meter_score) _can use it._
```C++
void dp_ber_meter_set_align (
    dp_ber_meter_state_t * state,
    ber_align_t align,
    size_t t0,
    size_t n_marker,
    size_t period
) 
```



The stateful [**dp\_ber\_meter\_align()**](ber__meter__core_8h.md#function-dp_ber_meter_align) is the usual path; this exists for the case where detection and scoring run over different buffers. It is deliberately the ONLY way to set an alignment other than detecting one — score() never takes a lag from its caller, because a lag that was passed in is a lag that could have been searched for.




**Parameters:**


* `state` Must be non-NULL. 
* `align` The alignment record to install verbatim. Its `ok` flag is stored but not checked: installing it is the caller's statement that it is known, so score() uses it. 
* `t0` Truth index of the marker's first occurrence. 
* `n_marker` Marker length in symbols; 0 selects BER\_SYNC\_SYMS. 
* `period` Repeat period in symbols; 0 for a single occurrence. 




        

<hr>



### function dp\_ber\_meter\_set\_state 

_Restore the six running counters from a blob._ 
```C++
int dp_ber_meter_set_state (
    dp_ber_meter_state_t * state,
    const void * blob
) 
```



Only the counters change: configuration, truth and the stored alignment stay as they are on the target, so reinstall truth and re-align before scoring more.




**Parameters:**


* `state` Must be non-NULL. 
* `blob` Blob written by [**dp\_ber\_meter\_get\_state()**](ber__meter__core_8h.md#function-dp_ber_meter_get_state). 



**Returns:**

DP\_OK on success, or DP\_ERR\_INVALID when the envelope (magic, version, endianness, size) does not match; the state is then left untouched. 





        

<hr>



### function dp\_ber\_meter\_set\_truth 

_Install the transmitted symbol sequence this meter scores against._ 
```C++
int dp_ber_meter_set_truth (
    dp_ber_meter_state_t * state,
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



### function dp\_ber\_meter\_state\_bytes 

_Size in bytes of the meter's serialized state blob._ 
```C++
size_t dp_ber_meter_state_bytes (
    const dp_ber_meter_state_t * state
) 
```



The blob holds the envelope header and the six running counters (errors, symbols, bit\_errors, bits, skipped, bursts) as uint64.




**Parameters:**


* `state` Meter (unused: the size is the same for every meter). 



**Returns:**

Blob size in bytes. 





        

<hr>
## Macro Definition Documentation





### define BER\_METER\_STATE\_MAGIC 

_State-blob magic tag for a BerMeter ("BERM")._ 
```C++
#define BER_METER_STATE_MAGIC `DP_FOURCC ('B', 'E', 'R', 'M')`
```




<hr>



### define BER\_METER\_STATE\_VERSION 

_State-blob layout version for a BerMeter._ 
```C++
#define BER_METER_STATE_VERSION `1u`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/ber_meter/ber_meter_core.h`

