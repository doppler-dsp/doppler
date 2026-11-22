

# File state\_filter\_kalman\_core.h



[**FileList**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**state\_filter\_kalman**](dir_cec9ba98649e30568bd09d31ea5df0bb.md) **>** [**state\_filter\_kalman\_core.h**](state__filter__kalman__core_8h.md)

[Go to the source code of this file](state__filter__kalman__core_8h_source.md)

_The Kalman flavor of the third-order tracking loop: the gains come from a covariance recursion instead of being held._ [More...](#detailed-description)

* `#include "doppler/clib_common.h"`
* `#include "doppler/jm_perf.h"`
* `#include "doppler/state_filter/state_filter_core.h"`
* `#include <stdbool.h>`
* `#include <stddef.h>`
* `#include <stdint.h>`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**dp\_state\_filter\_kalman\_state\_t**](structdp__state__filter__kalman__state__t.md) <br>_A third-order loop whose gains come from a covariance recursion._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  const char \* | [**dp\_state\_filter\_kalman\_args\_why**](#function-dp_state_filter_kalman_args_why) (double sigma\_e, double sigma\_a, double t, double sigma\_f0, double bn) <br>_The reason the arguments of_ [_**dp\_state\_filter\_kalman\_create\_args()**_](state__filter__kalman__core_8h.md#function-dp_state_filter_kalman_create_args) _are refused, naming the argument, or NULL when they are accepted._ |
|  double | [**dp\_state\_filter\_kalman\_bandwidth**](#function-dp_state_filter_kalman_bandwidth) (const [**dp\_state\_filter\_kalman\_state\_t**](structdp__state__filter__kalman__state__t.md) \* k, double delay, double gain) <br>_The exact bandwidth of the loop at the gains in force now: the base's_ [_**dp\_state\_filter\_bandwidth()**_](state__filter__core_8h.md#function-dp_state_filter_bandwidth) _on_`&k->base` _._ |
|  int | [**dp\_state\_filter\_kalman\_configure**](#function-dp_state_filter_kalman_configure) ([**dp\_state\_filter\_kalman\_state\_t**](structdp__state__filter__kalman__state__t.md) \* k, double sigma\_e, double sigma\_a, double t, double sigma\_f0, double bn) <br>_Change the figures, or to a bandwidth alone, keeping the estimates._  |
|  [**dp\_state\_filter\_kalman\_state\_t**](structdp__state__filter__kalman__state__t.md) \* | [**dp\_state\_filter\_kalman\_create**](#function-dp_state_filter_kalman_create) (double sigma\_e, double sigma\_a, double t, double sigma\_f0) <br>_Create from the caller's figures._  |
|  [**dp\_state\_filter\_kalman\_state\_t**](structdp__state__filter__kalman__state__t.md) \* | [**dp\_state\_filter\_kalman\_create\_args**](#function-dp_state_filter_kalman_create_args) (double sigma\_e, double sigma\_a, double t, double sigma\_f0, double bn) <br>_Create from the figures or from_ `bn` _alone. The binding calls_[_**dp\_state\_filter\_kalman\_create\_args\_why()**_](state__filter__kalman__core_8h.md#function-dp_state_filter_kalman_create_args_why) _, which also says why._ |
|  [**dp\_state\_filter\_kalman\_state\_t**](structdp__state__filter__kalman__state__t.md) \* | [**dp\_state\_filter\_kalman\_create\_args\_why**](#function-dp_state_filter_kalman_create_args_why) (double sigma\_e, double sigma\_a, double t, double sigma\_f0, double bn, const char \*\* why) <br>_Create from the figures or from_ `bn` _alone: the one constructor the Python class_`KalmanStateFilter` _calls._ |
|  [**dp\_state\_filter\_kalman\_state\_t**](structdp__state__filter__kalman__state__t.md) \* | [**dp\_state\_filter\_kalman\_create\_bn**](#function-dp_state_filter_kalman_create_bn) (double bn, double t) <br>_Create the automatic case._  |
|  [**dp\_state\_filter\_kalman\_state\_t**](structdp__state__filter__kalman__state__t.md) \* | [**dp\_state\_filter\_kalman\_create\_qr**](#function-dp_state_filter_kalman_create_qr) (double q, double r, double t, double start) <br>_Create a Kalman filter, validating its arguments._  |
|  void | [**dp\_state\_filter\_kalman\_destroy**](#function-dp_state_filter_kalman_destroy) ([**dp\_state\_filter\_kalman\_state\_t**](structdp__state__filter__kalman__state__t.md) \* k) <br>_Destroy a Kalman filter._  |
|  void | [**dp\_state\_filter\_kalman\_get\_state**](#function-dp_state_filter_kalman_get_state) (const [**dp\_state\_filter\_kalman\_state\_t**](structdp__state__filter__kalman__state__t.md) \* k, void \* blob) <br>_Serialize into a buffer of_ [_**dp\_state\_filter\_kalman\_state\_bytes()**_](state__filter__kalman__core_8h.md#function-dp_state_filter_kalman_state_bytes) _._ |
|  void | [**dp\_state\_filter\_kalman\_init**](#function-dp_state_filter_kalman_init) ([**dp\_state\_filter\_kalman\_state\_t**](structdp__state__filter__kalman__state__t.md) \* k, double sigma\_e, double sigma\_a, double t, double sigma\_f0) <br>_Initialise in place from the caller's figures. Unguarded, as_ [_**dp\_state\_filter\_init()**_](state__filter__core_8h.md#function-dp_state_filter_init) _is: outside the domain the filter holds a zero covariance and zero gains, defined and inert._ |
|  void | [**dp\_state\_filter\_kalman\_init\_bn**](#function-dp_state_filter_kalman_init_bn) ([**dp\_state\_filter\_kalman\_state\_t**](structdp__state__filter__kalman__state__t.md) \* k, double bn, double t) <br>_Initialise in place from_ `bn` _and_`t` _: the steady-state gains of the optimal filter whose exact bandwidth is_`bn` _, from the first update. Unguarded; see_[_**dp\_state\_filter\_kalman\_init()**_](state__filter__kalman__core_8h.md#function-dp_state_filter_kalman_init) _._ |
|  void | [**dp\_state\_filter\_kalman\_init\_qr**](#function-dp_state_filter_kalman_init_qr) ([**dp\_state\_filter\_kalman\_state\_t**](structdp__state__filter__kalman__state__t.md) \* k, double q, double r, double t, double start) <br>_Initialise in place (no allocation): the covariance at its steady state with the frequency entry scaled by_ `start` _squared, the estimates zero, the gains the first update's._ |
|  int | [**dp\_state\_filter\_kalman\_params\_ok**](#function-dp_state_filter_kalman_params_ok) (double sigma\_e, double sigma\_a, double t, double sigma\_f0) <br>_The domain, in the caller's figures._  |
|  int | [**dp\_state\_filter\_kalman\_params\_ok\_bn**](#function-dp_state_filter_kalman_params_ok_bn) (double bn, double t) <br>_The domain of the automatic case: a bandwidth and a period alone,_ `bn * t` _from STATE\_FILTER\_KALMAN\_BNT\_MIN to STATE\_FILTER\_KALMAN\_BNT\_MAX._ |
|  int | [**dp\_state\_filter\_kalman\_params\_ok\_qr**](#function-dp_state_filter_kalman_params_ok_qr) (double q, double r, double t, double start) <br>_The domain:_ `q > 0` _,_`r > 0` _,_`t > 0` _, all finite; the steady-state bandwidth from STATE\_FILTER\_KALMAN\_BNT\_MIN up to STATE\_FILTER\_KALMAN\_BNT\_MAX per update; and a start from 1 up to STATE\_FILTER\_KALMAN\_START\_MAX whose whole covariance trajectory keeps the gains inside the stability rectangle. A start above 1 costs a walk of 8/(bn\*t) updates and about 1200 stability tests at bn\*t 1e-2 (3000 at 1e-4): measured 1.2 ms and 5.7 ms by the bench rows create\_start\_1e2 and create\_start\_1e4, growing as 1/(bn\*t)._ |
|  double | [**dp\_state\_filter\_kalman\_ratio\_for\_bandwidth**](#function-dp_state_filter_kalman_ratio_for_bandwidth) (double bnt) <br>_The rate-walk variance whose steady-state loop has the exact noise bandwidth_ `bnt` _per update, for a unit measurement variance: the noise ratio_`bn` _selects._ |
|  void | [**dp\_state\_filter\_kalman\_reset**](#function-dp_state_filter_kalman_reset) ([**dp\_state\_filter\_kalman\_state\_t**](structdp__state__filter__kalman__state__t.md) \* k) <br>_Return to the start: the estimates zero and the covariance back to the one the filter began with. The noise figures are kept._  |
|  int | [**dp\_state\_filter\_kalman\_set\_state**](#function-dp_state_filter_kalman_set_state) ([**dp\_state\_filter\_kalman\_state\_t**](structdp__state__filter__kalman__state__t.md) \* k, const void \* blob) <br>_Restore from a blob; nothing changes unless it is wholly accepted._  |
|  double | [**dp\_state\_filter\_kalman\_sigma\_f**](#function-dp_state_filter_kalman_sigma_f) (const [**dp\_state\_filter\_kalman\_state\_t**](structdp__state__filter__kalman__state__t.md) \* k) <br>_The steady state's frequency deviation for this filter's figures, in the loop's units: the lower edge of a_ `sigma_f0` _that_[_**dp\_state\_filter\_kalman\_params\_ok()**_](state__filter__kalman__core_8h.md#function-dp_state_filter_kalman_params_ok) _accepts (its upper edge is STATE\_FILTER\_KALMAN\_START\_MAX times this)._ |
|  bool | [**dp\_state\_filter\_kalman\_stable**](#function-dp_state_filter_kalman_stable) (const [**dp\_state\_filter\_kalman\_state\_t**](structdp__state__filter__kalman__state__t.md) \* k, double delay, double gain) <br>_The exact stability of the loop at the gains in force now: the base's_ [_**dp\_state\_filter\_stable()**_](state__filter__core_8h.md#function-dp_state_filter_stable) _on_`&k->base` _._ |
|  double | [**dp\_state\_filter\_kalman\_start\_for**](#function-dp_state_filter_kalman_start_for) (double sigma\_e, double sigma\_a, double sigma\_f0) <br>_The start multiplier a frequency uncertainty means:_ `sigma_f0` _over the steady state's frequency deviation for these figures._ |
|  size\_t | [**dp\_state\_filter\_kalman\_state\_bytes**](#function-dp_state_filter_kalman_state_bytes) (const [**dp\_state\_filter\_kalman\_state\_t**](structdp__state__filter__kalman__state__t.md) \* k) <br>_Size of the Kalman blob: envelope, gains, estimates, covariance, its start, and the two figures._  |
|  int | [**dp\_state\_filter\_kalman\_steady**](#function-dp_state_filter_kalman_steady) (double q, double r, double p, double gains) <br>_The steady-state covariance and gains of the recursion, by structure-preserving doubling (quadratically convergent, no iteration to a tolerance in_ `1/(bn*t)` _updates)._ |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) double | [**dp\_state\_filter\_kalman\_step**](#function-dp_state_filter_kalman_step) ([**dp\_state\_filter\_kalman\_state\_t**](structdp__state__filter__kalman__state__t.md) \* k, double e) <br>_One update, the call to use: the loop's three lines on the gains in force, then the covariance advanced and the gains refreshed. Inline, over_ [_**dp\_state\_filter\_kalman\_update()**_](state__filter__kalman__core_8h.md#function-dp_state_filter_kalman_update) _._ |
|  void | [**dp\_state\_filter\_kalman\_steps**](#function-dp_state_filter_kalman_steps) ([**dp\_state\_filter\_kalman\_state\_t**](structdp__state__filter__kalman__state__t.md) \* k, const double \* x, double \* out, size\_t n) <br>_A block of updates, equivalent to_ [_**dp\_state\_filter\_kalman\_step()**_](state__filter__kalman__core_8h.md#function-dp_state_filter_kalman_step) _once per element._ |
|  double | [**dp\_state\_filter\_kalman\_update**](#function-dp_state_filter_kalman_update) ([**dp\_state\_filter\_kalman\_state\_t**](structdp__state__filter__kalman__state__t.md) \* k, double e) <br>_The implementation of_ [_**dp\_state\_filter\_kalman\_step()**_](state__filter__kalman__core_8h.md#function-dp_state_filter_kalman_step) _, out of line. Call the step; this is its body: the loop's three lines on the gains in force, then the covariance advanced and the gains refreshed. The step is a header-inline wrapper over it because the binding generator derives dp\_state\_filter\_kalman\_step as the built-in step's name and looks for it in the header._ |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**STATE\_FILTER\_KALMAN\_BNT\_MAX**](state__filter__kalman__core_8h.md#define-state_filter_kalman_bnt_max)  `0.12`<br>_The largest steady-state_ `bn*t` _the Kalman flavor accepts. Its own constant: the Riccati gains are not the fixed-gain flavor's, so the rectangle is derived again from the exact function at them, by native/validation/state\_filter\_kalman\_loop.c, which finds the largest_`bn*t` _stable at the upper corner (K 2, delay 2) to be 0.1302. This constant is the largest value on its 0.01 grid at which the upper gain edge at the worst delay keeps 5% headroom over STATE\_FILTER\_K\_MAX, so a 5% error in loop gain or detector slope stays stable: 2.18 against 2 at 0.12 and 2.004 at 0.13. The harness asserts exactly that, so a different value fails it. Both bounds are compared in double: a bandwidth within rounding of one (a figure set whose implied_`bn*t` _lands within an ulp of it) is outside the contract, and may be accepted on one platform and refused on another._ |
| define  | [**STATE\_FILTER\_KALMAN\_BNT\_MIN**](state__filter__kalman__core_8h.md#define-state_filter_kalman_bnt_min)  `1e-6`<br>_The smallest_ `bn*t` _the Kalman flavor accepts: the smallest the validation harness measures it at, so nothing below is claimed. At 1e-6 the delivered bandwidth is_`bn*t` _to under 1e-8 and the plain covariance update stays positive definite with gains within 3e-11 of a long double reference over 10/(bn\*t) updates (the harness's gains and plain-update blocks, which assert this constant is not below 1e-6). Not the fixed-gain flavor's STATE\_FILTER\_BNT\_MIN._ |
| define  | [**STATE\_FILTER\_KALMAN\_START\_MAX**](state__filter__kalman__core_8h.md#define-state_filter_kalman_start_max)  `5.0`<br>_The largest start any figure set can be given: the initial frequency deviation as a multiple of the steady-state one._  |
| define  | [**STATE\_FILTER\_KALMAN\_STATE\_MAGIC**](state__filter__kalman__core_8h.md#define-state_filter_kalman_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('S', 'F', 'K', 'A')`<br>_Magic of the Kalman blob (the fixed-gain filter's is 'SFLT')._  |
| define  | [**STATE\_FILTER\_KALMAN\_STATE\_VERSION**](state__filter__kalman__core_8h.md#define-state_filter_kalman_state_version)  `1u`<br>_Version of the Kalman blob layout._  |

## Detailed Description


See docs/design/loop-filter.md Part III. The filter is the base [**dp\_state\_filter\_state\_t**](structdp__state__filter__state__t.md) (the loop's three lines, called not copied) followed by the covariance of the predicted state, its start, and the two figures `q` (the rate's random-walk variance per update) and `r` (the detector-error variance per update). A caller states the two rms figures `sigma_e` and `sigma_a`, or a bandwidth alone.


Lifecycle: `create -> (step / steps / configure / reset)* -> destroy`; [**dp\_state\_filter\_kalman\_init()**](state__filter__kalman__core_8h.md#function-dp_state_filter_kalman_init) does the same into caller memory with no heap, as the base's [**dp\_state\_filter\_init()**](state__filter__core_8h.md#function-dp_state_filter_init) does.



```C++
dp_state_filter_kalman_state_t k;
dp_state_filter_kalman_init_bn (&k, 0.01, 1.0);  // the automatic case
double control = dp_state_filter_kalman_step (&k, 0.1);
```
 


    
## Public Functions Documentation




### function dp\_state\_filter\_kalman\_args\_why 

_The reason the arguments of_ [_**dp\_state\_filter\_kalman\_create\_args()**_](state__filter__kalman__core_8h.md#function-dp_state_filter_kalman_create_args) _are refused, naming the argument, or NULL when they are accepted._
```C++
const char * dp_state_filter_kalman_args_why (
    double sigma_e,
    double sigma_a,
    double t,
    double sigma_f0,
    double bn
) 
```



The conditions are those of the two constructors it dispatches to, in the order they are checked: the form (figures or `bn`, not both, not neither), `t`, then the figures' own conditions (each positive, a steady state, a bandwidth inside the domain, a `sigma_f0` that is a legal start) or `bn`'s. The strings are static and carry no numbers, so they do not drift with the domain's limits.




**Parameters:**


* `sigma_e` RMS detector error when locked (the figures form). 
* `sigma_a` RMS change of the rate per update (the figures form). 
* `t` Update period in samples. 
* `sigma_f0` Frequency uncertainty (the figures form), 0 for none. 
* `bn` Bandwidth alone (the automatic form). 



**Returns:**

A static sentence, or NULL if the arguments are accepted. 





        

<hr>



### function dp\_state\_filter\_kalman\_bandwidth 

_The exact bandwidth of the loop at the gains in force now: the base's_ [_**dp\_state\_filter\_bandwidth()**_](state__filter__core_8h.md#function-dp_state_filter_bandwidth) _on_`&k->base` _._
```C++
double dp_state_filter_kalman_bandwidth (
    const dp_state_filter_kalman_state_t * k,
    double delay,
    double gain
) 
```





**Parameters:**


* `k` Must be non-NULL. 
* `delay` Detector delay in updates. 
* `gain` Loop gain as a multiple of nominal. 



**Returns:**

Cycles per update, NaN where the frozen loop is not stable.



```C++
>>> from doppler.track import KalmanStateFilter
>>> k = KalmanStateFilter(bn=0.01)
>>> round(k.bandwidth(delay=1.0), 4)   # a detector delay of one update
0.0103
```
 


        

<hr>



### function dp\_state\_filter\_kalman\_configure 

_Change the figures, or to a bandwidth alone, keeping the estimates._ 
```C++
int dp_state_filter_kalman_configure (
    dp_state_filter_kalman_state_t * k,
    double sigma_e,
    double sigma_a,
    double t,
    double sigma_f0,
    double bn
) 
```



A retune restarts the gain schedule; the estimates carry: the covariance restarts from the new steady state (scaled by the start `sigma_f0` means), the gains follow it, and `integ` and `rate` are untouched. An argument of 0 keeps its current value (`sigma_f0` 0 is the steady state), so a call names only what changes. `bn` alone is the automatic case and may not be mixed with the figures. A call outside the domain ([**dp\_state\_filter\_kalman\_params\_ok()**](state__filter__kalman__core_8h.md#function-dp_state_filter_kalman_params_ok)) changes nothing.


Three behaviors, all in the blob (q, r and t are reject keys, the start is P0):
* The automatic case holds the figures its bandwidth implies, with `sigma_e` 1 (the detector's rms as the unit) and `sigma_a` the square root of the noise ratio that bandwidth selects; a figures-form call that names one figure keeps the other at that implied value.
* A call with `bn` replaces the stored figures by those the bandwidth implies: a figures-form call that follows keeps the implied `r` and `q`, not the ones the filter was built with.
* A call with every argument 0 puts the gains at the steady state at once and keeps the estimates: it ends a high-gain start early, once the loop has pulled in, without changing the design. `configure (k, 0, 0, 0, f, 0)` with a frequency uncertainty `f` alone does the opposite and re-opens the gains with the same figures, as after a loss of lock. 

**Parameters:**


  * `k` Must be non-NULL. 
  * `sigma_e` RMS detector error when locked, 0 to keep. 
  * `sigma_a` RMS change of the rate per update, 0 to keep. 
  * `t` Update period in samples, 0 to keep. 
  * `sigma_f0` Frequency uncertainty, 0 for the steady state. 
  * `bn` Noise bandwidth in cycles per sample (automatic form), 0 for the figures form. 



**Returns:**

DP\_OK, or DP\_ERR\_INVALID (nothing changed).



```C++
>>> import numpy as np
>>> from doppler.track import KalmanStateFilter
>>> k = KalmanStateFilter(bn=0.01)
>>> _ = k.steps(np.full(50, 0.1))
>>> k.configure(bn=0.02)               # widen the loop, keep lock
>>> round(k.integ, 4), round(k.bn, 4)
(0.0016, 0.02)
```
 




        

<hr>



### function dp\_state\_filter\_kalman\_create 

_Create from the caller's figures._ 
```C++
dp_state_filter_kalman_state_t * dp_state_filter_kalman_create (
    double sigma_e,
    double sigma_a,
    double t,
    double sigma_f0
) 
```





**Parameters:**


* `sigma_e` RMS detector error when locked. 
* `sigma_a` RMS change of the rate per update. 
* `t` Update period in samples. 
* `sigma_f0` Frequency uncertainty, 0 for the steady state. 



**Returns:**

Heap-allocated filter, or NULL outside [**dp\_state\_filter\_kalman\_params\_ok()**](state__filter__kalman__core_8h.md#function-dp_state_filter_kalman_params_ok). 





        

<hr>



### function dp\_state\_filter\_kalman\_create\_args 

_Create from the figures or from_ `bn` _alone. The binding calls_[_**dp\_state\_filter\_kalman\_create\_args\_why()**_](state__filter__kalman__core_8h.md#function-dp_state_filter_kalman_create_args_why) _, which also says why._
```C++
dp_state_filter_kalman_state_t * dp_state_filter_kalman_create_args (
    double sigma_e,
    double sigma_a,
    double t,
    double sigma_f0,
    double bn
) 
```



The figures form is `sigma_e` and `sigma_a` both &gt; 0 with `bn` 0; the automatic form is `bn` &gt; 0 with `sigma_e`, `sigma_a` and `sigma_f0` all 0. Both or neither is refused. 

**Parameters:**


* `sigma_e` RMS detector error when locked (figures form). 
* `sigma_a` RMS change of the rate per update (figures form). 
* `t` Update period in samples. 
* `sigma_f0` Initial frequency uncertainty (figures form), 0 for the steady state. 
* `bn` Noise bandwidth in cycles per sample (automatic form). 



**Returns:**

Heap-allocated filter, or NULL outside either form's domain. 





        

<hr>



### function dp\_state\_filter\_kalman\_create\_args\_why 

_Create from the figures or from_ `bn` _alone: the one constructor the Python class_`KalmanStateFilter` _calls._
```C++
dp_state_filter_kalman_state_t * dp_state_filter_kalman_create_args_why (
    double sigma_e,
    double sigma_a,
    double t,
    double sigma_f0,
    double bn,
    const char ** why
) 
```



The figures form is `sigma_e` and `sigma_a` both &gt; 0 with `bn` 0; the automatic form is `bn` &gt; 0 with `sigma_e`, `sigma_a` and `sigma_f0` all 0. Both or neither is refused. 

**Parameters:**


* `sigma_e` RMS detector error when locked (figures form). 
* `sigma_a` RMS change of the rate per update (figures form). 
* `t` Update period in samples. 
* `sigma_f0` Initial frequency uncertainty (figures form), 0 for the steady state. 
* `bn` Noise bandwidth in cycles per sample (automatic form). 
* `why` Where the refusal's sentence goes, naming the argument it refused ([**dp\_state\_filter\_kalman\_args\_why()**](state__filter__kalman__core_8h.md#function-dp_state_filter_kalman_args_why)); set to NULL when the filter is built. May be NULL. 



**Returns:**

Heap-allocated filter, or NULL (with `why` set) outside either form's domain.



```C++
>>> from doppler.track import KalmanStateFilter
>>> k = KalmanStateFilter(bn=0.01)             # the automatic case
>>> round(k.bandwidth(), 6)
0.01
>>> k.stable()
True
>>> f = KalmanStateFilter(sigma_e=1.0, sigma_a=1e-3)   # the two figures
>>> round(f.bandwidth(), 4)
0.0904
>>> KalmanStateFilter()                        # neither form
Traceback (most recent call last):
    ...
ValueError: give sigma_e and sigma_a, or bn alone
```
 


        

<hr>



### function dp\_state\_filter\_kalman\_create\_bn 

_Create the automatic case._ 
```C++
dp_state_filter_kalman_state_t * dp_state_filter_kalman_create_bn (
    double bn,
    double t
) 
```





**Parameters:**


* `bn` Noise bandwidth in cycles per sample. 
* `t` Update period in samples. 



**Returns:**

Heap-allocated filter, or NULL outside [**dp\_state\_filter\_kalman\_params\_ok\_bn()**](state__filter__kalman__core_8h.md#function-dp_state_filter_kalman_params_ok_bn). 





        

<hr>



### function dp\_state\_filter\_kalman\_create\_qr 

_Create a Kalman filter, validating its arguments._ 
```C++
dp_state_filter_kalman_state_t * dp_state_filter_kalman_create_qr (
    double q,
    double r,
    double t,
    double start
) 
```





**Parameters:**


* `q` Rate-walk variance per update, &gt; 0. 
* `r` Detector-error variance per update, &gt; 0. 
* `t` Update period in samples, &gt; 0. 
* `start` Initial frequency deviation over the steady-state one. 



**Returns:**

Heap-allocated filter, or NULL outside [**dp\_state\_filter\_kalman\_params\_ok\_qr()**](state__filter__kalman__core_8h.md#function-dp_state_filter_kalman_params_ok_qr). 





        

<hr>



### function dp\_state\_filter\_kalman\_destroy 

_Destroy a Kalman filter._ 
```C++
void dp_state_filter_kalman_destroy (
    dp_state_filter_kalman_state_t * k
) 
```





**Parameters:**


* `k` May be NULL. 




        

<hr>



### function dp\_state\_filter\_kalman\_get\_state 

_Serialize into a buffer of_ [_**dp\_state\_filter\_kalman\_state\_bytes()**_](state__filter__kalman__core_8h.md#function-dp_state_filter_kalman_state_bytes) _._
```C++
void dp_state_filter_kalman_get_state (
    const dp_state_filter_kalman_state_t * k,
    void * blob
) 
```





**Parameters:**


* `k` The filter to serialize. 
* `blob` Destination, at least [**dp\_state\_filter\_kalman\_state\_bytes()**](state__filter__kalman__core_8h.md#function-dp_state_filter_kalman_state_bytes). 




        

<hr>



### function dp\_state\_filter\_kalman\_init 

_Initialise in place from the caller's figures. Unguarded, as_ [_**dp\_state\_filter\_init()**_](state__filter__core_8h.md#function-dp_state_filter_init) _is: outside the domain the filter holds a zero covariance and zero gains, defined and inert._
```C++
void dp_state_filter_kalman_init (
    dp_state_filter_kalman_state_t * k,
    double sigma_e,
    double sigma_a,
    double t,
    double sigma_f0
) 
```





**Parameters:**


* `k` Must be non-NULL. 
* `sigma_e` RMS detector error when locked. 
* `sigma_a` RMS change of the rate per update. 
* `t` Update period in samples. 
* `sigma_f0` Frequency uncertainty, 0 for the steady state. 




        

<hr>



### function dp\_state\_filter\_kalman\_init\_bn 

_Initialise in place from_ `bn` _and_`t` _: the steady-state gains of the optimal filter whose exact bandwidth is_`bn` _, from the first update. Unguarded; see_[_**dp\_state\_filter\_kalman\_init()**_](state__filter__kalman__core_8h.md#function-dp_state_filter_kalman_init) _._
```C++
void dp_state_filter_kalman_init_bn (
    dp_state_filter_kalman_state_t * k,
    double bn,
    double t
) 
```





**Parameters:**


* `k` Must be non-NULL. 
* `bn` Noise bandwidth in cycles per sample. 
* `t` Update period in samples. 




        

<hr>



### function dp\_state\_filter\_kalman\_init\_qr 

_Initialise in place (no allocation): the covariance at its steady state with the frequency entry scaled by_ `start` _squared, the estimates zero, the gains the first update's._
```C++
void dp_state_filter_kalman_init_qr (
    dp_state_filter_kalman_state_t * k,
    double q,
    double r,
    double t,
    double start
) 
```



Arguments are not validated here, as in [**dp\_state\_filter\_init()**](state__filter__core_8h.md#function-dp_state_filter_init), and the result is defined for every input: a non-positive or non-finite q or r makes the steady solve refuse, which leaves the inert filter (zero covariance, zero gains, zero estimates; the figures and the period are still stored); any other input, including a bandwidth outside the flavor's range or a start past the ceiling, builds the live filter those figures describe. [**dp\_state\_filter\_kalman\_params\_ok\_qr()**](state__filter__kalman__core_8h.md#function-dp_state_filter_kalman_params_ok_qr) is the check.




**Parameters:**


* `k` Must be non-NULL. 
* `q` Rate-walk variance per update. 
* `r` Detector-error variance per update. 
* `t` Update period in samples. 
* `start` Initial frequency deviation over the steady-state one, &gt;= 1 (1 is the automatic case: the steady-state gains from the first update). 




        

<hr>



### function dp\_state\_filter\_kalman\_params\_ok 

_The domain, in the caller's figures._ 
```C++
int dp_state_filter_kalman_params_ok (
    double sigma_e,
    double sigma_a,
    double t,
    double sigma_f0
) 
```





**Parameters:**


* `sigma_e` RMS detector error when locked, &gt; 0. 
* `sigma_a` RMS change of the rate per update, &gt; 0. 
* `t` Update period in samples, &gt; 0. 
* `sigma_f0` Frequency uncertainty, 0 or within [**dp\_state\_filter\_kalman\_start\_for()**](state__filter__kalman__core_8h.md#function-dp_state_filter_kalman_start_for)'s range. 



**Returns:**

1 inside, 0 outside (including a steady-state bandwidth outside STATE\_FILTER\_KALMAN\_BNT\_MIN to STATE\_FILTER\_KALMAN\_BNT\_MAX). 





        

<hr>



### function dp\_state\_filter\_kalman\_params\_ok\_bn 

_The domain of the automatic case: a bandwidth and a period alone,_ `bn * t` _from STATE\_FILTER\_KALMAN\_BNT\_MIN to STATE\_FILTER\_KALMAN\_BNT\_MAX._
```C++
int dp_state_filter_kalman_params_ok_bn (
    double bn,
    double t
) 
```





**Parameters:**


* `bn` Noise bandwidth in cycles per sample. 
* `t` Update period in samples. 



**Returns:**

1 inside the domain, 0 outside it. 





        

<hr>



### function dp\_state\_filter\_kalman\_params\_ok\_qr 

_The domain:_ `q > 0` _,_`r > 0` _,_`t > 0` _, all finite; the steady-state bandwidth from STATE\_FILTER\_KALMAN\_BNT\_MIN up to STATE\_FILTER\_KALMAN\_BNT\_MAX per update; and a start from 1 up to STATE\_FILTER\_KALMAN\_START\_MAX whose whole covariance trajectory keeps the gains inside the stability rectangle. A start above 1 costs a walk of 8/(bn\*t) updates and about 1200 stability tests at bn\*t 1e-2 (3000 at 1e-4): measured 1.2 ms and 5.7 ms by the bench rows create\_start\_1e2 and create\_start\_1e4, growing as 1/(bn\*t)._
```C++
int dp_state_filter_kalman_params_ok_qr (
    double q,
    double r,
    double t,
    double start
) 
```





**Parameters:**


* `q` Rate-walk variance per update. 
* `r` Detector-error variance per update. 
* `t` Update period in samples. 
* `start` Initial frequency deviation over the steady-state one. 



**Returns:**

1 inside the domain, 0 outside it. 





        

<hr>



### function dp\_state\_filter\_kalman\_ratio\_for\_bandwidth 

_The rate-walk variance whose steady-state loop has the exact noise bandwidth_ `bnt` _per update, for a unit measurement variance: the noise ratio_`bn` _selects._
```C++
double dp_state_filter_kalman_ratio_for_bandwidth (
    double bnt
) 
```





**Parameters:**


* `bnt` The bandwidth in cycles per update, between STATE\_FILTER\_KALMAN\_BNT\_MIN and STATE\_FILTER\_KALMAN\_BNT\_MAX. 



**Returns:**

The ratio `q/r`, or 0 outside that range. 





        

<hr>



### function dp\_state\_filter\_kalman\_reset 

_Return to the start: the estimates zero and the covariance back to the one the filter began with. The noise figures are kept._ 
```C++
void dp_state_filter_kalman_reset (
    dp_state_filter_kalman_state_t * k
) 
```





**Parameters:**


* `k` Must be non-NULL.


```C++
>>> import numpy as np
>>> from doppler.track import KalmanStateFilter
>>> k = KalmanStateFilter(bn=0.01)
>>> _ = k.steps(np.full(50, 0.1))
>>> round(k.integ, 4)
0.0016
>>> k.reset()
>>> k.integ
0.0
```
 


        

<hr>



### function dp\_state\_filter\_kalman\_set\_state 

_Restore from a blob; nothing changes unless it is wholly accepted._ 
```C++
int dp_state_filter_kalman_set_state (
    dp_state_filter_kalman_state_t * k,
    const void * blob
) 
```



Refused (DP\_ERR\_INVALID): a bad envelope (the fixed-gain filter's blob has another magic); a covariance P or start P0 that is not finite and positive definite; figures outside [**dp\_state\_filter\_kalman\_params\_ok\_qr()**](state__filter__kalman__core_8h.md#function-dp_state_filter_kalman_params_ok_qr); gains that are not the ones P and r give; and a P0 that is not a start a live object holds: the steady state for (q, r) with only the frequency entry scaled, by a start from 1 up to STATE\_FILTER\_KALMAN\_START\_MAX. A start past the ceiling (an embedder that used the unguarded init) is refused here, and that is the embedder's to know.


What is not checked, and why. The current covariance P is checked for positive definiteness and for giving the gains in force, and no further: mid-transient it cannot be recomputed, as the base's integ and rate are not checked. The trajectory rule is not rerun on restore (it walks 8/(bn\*t) updates and 1200 to 3000 stability tests: 1.2 ms at bn\*t 1e-2, 5.7 ms at 1e-4, measured): a restored start is checked for the ceiling and for being a start at all, so a start between the rule's cap and the ceiling that an embedder built with the unguarded init is restored as it was. Whether to rerun the rule on restore is tracked in [https://github.com/doppler-dsp/doppler/issues/3136](https://github.com/doppler-dsp/doppler/issues/3136).




**Parameters:**


* `k` The filter to restore into. 
* `blob` A blob from [**dp\_state\_filter\_kalman\_get\_state()**](state__filter__kalman__core_8h.md#function-dp_state_filter_kalman_get_state). 



**Returns:**

DP\_OK or DP\_ERR\_INVALID. 





        

<hr>



### function dp\_state\_filter\_kalman\_sigma\_f 

_The steady state's frequency deviation for this filter's figures, in the loop's units: the lower edge of a_ `sigma_f0` _that_[_**dp\_state\_filter\_kalman\_params\_ok()**_](state__filter__kalman__core_8h.md#function-dp_state_filter_kalman_params_ok) _accepts (its upper edge is STATE\_FILTER\_KALMAN\_START\_MAX times this)._
```C++
double dp_state_filter_kalman_sigma_f (
    const dp_state_filter_kalman_state_t * k
) 
```





**Parameters:**


* `k` Must be non-NULL. 



**Returns:**

The deviation, or 0 for an inert (out-of-domain) filter.



```C++
>>> from doppler.track import KalmanStateFilter
>>> k = KalmanStateFilter(sigma_e=1.0, sigma_a=2e-6)
>>> sf = k.sigma_f
>>> KalmanStateFilter(sigma_e=1.0, sigma_a=2e-6, sigma_f0=3 * sf).bn > 0
True
```
 


        

<hr>



### function dp\_state\_filter\_kalman\_stable 

_The exact stability of the loop at the gains in force now: the base's_ [_**dp\_state\_filter\_stable()**_](state__filter__core_8h.md#function-dp_state_filter_stable) _on_`&k->base` _._
```C++
bool dp_state_filter_kalman_stable (
    const dp_state_filter_kalman_state_t * k,
    double delay,
    double gain
) 
```





**Parameters:**


* `k` Must be non-NULL. 
* `delay` Detector delay in updates. 
* `gain` Loop gain as a multiple of nominal. 



**Returns:**

true where the frozen loop is stable.



```C++
>>> from doppler.track import KalmanStateFilter
>>> KalmanStateFilter(bn=0.01).stable(delay=2.0)
True
```
 


        

<hr>



### function dp\_state\_filter\_kalman\_start\_for 

_The start multiplier a frequency uncertainty means:_ `sigma_f0` _over the steady state's frequency deviation for these figures._
```C++
double dp_state_filter_kalman_start_for (
    double sigma_e,
    double sigma_a,
    double sigma_f0
) 
```





**Parameters:**


* `sigma_e` RMS detector error when locked, &gt; 0. 
* `sigma_a` RMS change of the rate per update, &gt; 0. 
* `sigma_f0` Frequency uncertainty; 0 for the steady state. 



**Returns:**

The multiplier (1 for 0), or 0 for a non-positive figure or a `sigma_f0` below the steady state's deviation or above STATE\_FILTER\_KALMAN\_START\_MAX times it. 





        

<hr>



### function dp\_state\_filter\_kalman\_state\_bytes 

_Size of the Kalman blob: envelope, gains, estimates, covariance, its start, and the two figures._ 
```C++
size_t dp_state_filter_kalman_state_bytes (
    const dp_state_filter_kalman_state_t * k
) 
```





**Parameters:**


* `k` The filter (its size is the same for every filter). 



**Returns:**

Bytes [**dp\_state\_filter\_kalman\_get\_state()**](state__filter__kalman__core_8h.md#function-dp_state_filter_kalman_get_state) writes. 





        

<hr>



### function dp\_state\_filter\_kalman\_steady 

_The steady-state covariance and gains of the recursion, by structure-preserving doubling (quadratically convergent, no iteration to a tolerance in_ `1/(bn*t)` _updates)._
```C++
int dp_state_filter_kalman_steady (
    double q,
    double r,
    double p,
    double gains
) 
```





**Parameters:**


* `q` Rate-walk variance per update, &gt; 0. 
* `r` Detector-error variance per update, &gt; 0. 
* `p` The steady-state covariance, `p00 p01 p02 p11 p12 p22`. 
* `gains` The loop gains `kp`, `ki`, `kr` it gives. 



**Returns:**

1 on success, 0 for a non-positive or non-finite figure. 





        

<hr>



### function dp\_state\_filter\_kalman\_step 

_One update, the call to use: the loop's three lines on the gains in force, then the covariance advanced and the gains refreshed. Inline, over_ [_**dp\_state\_filter\_kalman\_update()**_](state__filter__kalman__core_8h.md#function-dp_state_filter_kalman_update) _._
```C++
JM_FORCEINLINE  JM_HOT double dp_state_filter_kalman_step (
    dp_state_filter_kalman_state_t * k,
    double e
) 
```





**Parameters:**


* `k` Must be non-NULL. 
* `e` The loop error for this update. 



**Returns:**

The control value `integ + kp*e`.



```C++
>>> from doppler.track import KalmanStateFilter
>>> k = KalmanStateFilter(bn=0.01)
>>> round(k.step(0.1), 4)             # the first nudge
0.0024
```
 


        

<hr>



### function dp\_state\_filter\_kalman\_steps 

_A block of updates, equivalent to_ [_**dp\_state\_filter\_kalman\_step()**_](state__filter__kalman__core_8h.md#function-dp_state_filter_kalman_step) _once per element._
```C++
void dp_state_filter_kalman_steps (
    dp_state_filter_kalman_state_t * k,
    const double * x,
    double * out,
    size_t n
) 
```





**Parameters:**


* `k` Must be non-NULL. 
* `x` Loop errors. 
* `out` Control values (length &gt;= n; may alias `x`). 
* `n` Number of updates.


```C++
>>> import numpy as np
>>> from doppler.track import KalmanStateFilter
>>> k = KalmanStateFilter(bn=0.01)
>>> y = k.steps(np.full(50, 0.1))     # constant error: open loop, ramping
>>> round(float(y[0]), 4), round(float(y[-1]), 4)
(0.0024, 0.004)
```
 


        

<hr>



### function dp\_state\_filter\_kalman\_update 

_The implementation of_ [_**dp\_state\_filter\_kalman\_step()**_](state__filter__kalman__core_8h.md#function-dp_state_filter_kalman_step) _, out of line. Call the step; this is its body: the loop's three lines on the gains in force, then the covariance advanced and the gains refreshed. The step is a header-inline wrapper over it because the binding generator derives dp\_state\_filter\_kalman\_step as the built-in step's name and looks for it in the header._
```C++
double dp_state_filter_kalman_update (
    dp_state_filter_kalman_state_t * k,
    double e
) 
```





**Parameters:**


* `k` Must be non-NULL. 
* `e` The loop error for this update. 



**Returns:**

The control value `integ + kp*e`. 





        

<hr>
## Macro Definition Documentation





### define STATE\_FILTER\_KALMAN\_BNT\_MAX 

_The largest steady-state_ `bn*t` _the Kalman flavor accepts. Its own constant: the Riccati gains are not the fixed-gain flavor's, so the rectangle is derived again from the exact function at them, by native/validation/state\_filter\_kalman\_loop.c, which finds the largest_`bn*t` _stable at the upper corner (K 2, delay 2) to be 0.1302. This constant is the largest value on its 0.01 grid at which the upper gain edge at the worst delay keeps 5% headroom over STATE\_FILTER\_K\_MAX, so a 5% error in loop gain or detector slope stays stable: 2.18 against 2 at 0.12 and 2.004 at 0.13. The harness asserts exactly that, so a different value fails it. Both bounds are compared in double: a bandwidth within rounding of one (a figure set whose implied_`bn*t` _lands within an ulp of it) is outside the contract, and may be accepted on one platform and refused on another._
```C++
#define STATE_FILTER_KALMAN_BNT_MAX `0.12`
```




<hr>



### define STATE\_FILTER\_KALMAN\_BNT\_MIN 

_The smallest_ `bn*t` _the Kalman flavor accepts: the smallest the validation harness measures it at, so nothing below is claimed. At 1e-6 the delivered bandwidth is_`bn*t` _to under 1e-8 and the plain covariance update stays positive definite with gains within 3e-11 of a long double reference over 10/(bn\*t) updates (the harness's gains and plain-update blocks, which assert this constant is not below 1e-6). Not the fixed-gain flavor's STATE\_FILTER\_BNT\_MIN._
```C++
#define STATE_FILTER_KALMAN_BNT_MIN `1e-6`
```




<hr>



### define STATE\_FILTER\_KALMAN\_START\_MAX 

_The largest start any figure set can be given: the initial frequency deviation as a multiple of the steady-state one._ 
```C++
#define STATE_FILTER_KALMAN_START_MAX `5.0`
```



It is a ceiling, not the rule. The rule is that the gains along the covariance's whole trajectory stay inside the stability rectangle (K from 0.5 to 2, detector delays 0 to 2), and [**dp\_state\_filter\_kalman\_params\_ok\_qr()**](state__filter__kalman__core_8h.md#function-dp_state_filter_kalman_params_ok_qr) checks it for the figures given. The rule's own cap is above 5.9 at every bn\*t up to 0.04, so the ceiling binds there: 5 is the integer part of the smallest of those caps (5.90 at 0.04), which the harness asserts, so the ceiling is the rule's own number rounded down and not a free choice. Past 0.04 the rule binds: 3.77 at 0.06, 2.42 at 0.08, 1.67 at 0.10 and 1.19 at the bound 0.12 (the harness's caps table, which the validation report renders in section 2.5). 


        

<hr>



### define STATE\_FILTER\_KALMAN\_STATE\_MAGIC 

_Magic of the Kalman blob (the fixed-gain filter's is 'SFLT')._ 
```C++
#define STATE_FILTER_KALMAN_STATE_MAGIC `DP_FOURCC ('S', 'F', 'K', 'A')`
```




<hr>



### define STATE\_FILTER\_KALMAN\_STATE\_VERSION 

_Version of the Kalman blob layout._ 
```C++
#define STATE_FILTER_KALMAN_STATE_VERSION `1u`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/state_filter_kalman/state_filter_kalman_core.h`

