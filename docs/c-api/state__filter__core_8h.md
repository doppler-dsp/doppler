

# File state\_filter\_core.h



[**FileList**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**state\_filter**](dir_f99952e39f9637fbb70decda0be42187.md) **>** [**state\_filter\_core.h**](state__filter__core_8h.md)

[Go to the source code of this file](state__filter__core_8h_source.md)

_A filter over an explicit state vector (the frequency estimate and its rate): the fixed-gain third-order tracking loop._ [More...](#detailed-description)

* `#include "doppler/clib_common.h"`
* `#include "doppler/dp_state.h"`
* `#include <stdbool.h>`
* `#include "doppler/jm_perf.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**dp\_state\_filter\_state\_t**](structdp__state__filter__state__t.md) <br>_Third-order tracking-loop filter state (embeddable by value)._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  double | [**dp\_state\_filter\_bandwidth**](#function-dp_state_filter_bandwidth) (const [**dp\_state\_filter\_state\_t**](structdp__state__filter__state__t.md) \* state, double delay, double gain) <br>_This filter's exact delivered noise bandwidth at a detector delay and loop gain._  |
|  int | [**dp\_state\_filter\_configure**](#function-dp_state_filter_configure) ([**dp\_state\_filter\_state\_t**](structdp__state__filter__state__t.md) \* state, double bn, double t) <br>_Retune the gains for a new_ `(bn, t)` _without disturbing the frequency and rate estimates._ |
|  [**dp\_state\_filter\_state\_t**](structdp__state__filter__state__t.md) \* | [**dp\_state\_filter\_create**](#function-dp_state_filter_create) (double bn, double t) <br>_Create a state filter, validating its arguments. The binding calls_ [_**dp\_state\_filter\_create\_why()**_](state__filter__core_8h.md#function-dp_state_filter_create_why) _, which also says why it refused._ |
|  [**dp\_state\_filter\_state\_t**](structdp__state__filter__state__t.md) \* | [**dp\_state\_filter\_create\_why**](#function-dp_state_filter_create_why) (double bn, double t, const char \*\* why) <br>_Create a third-order state filter whose closed loop has noise bandwidth_ `bn` _, for a loop whose rest has unit gain: the one constructor the Python class_`StateFilter` _calls._ |
|  void | [**dp\_state\_filter\_destroy**](#function-dp_state_filter_destroy) ([**dp\_state\_filter\_state\_t**](structdp__state__filter__state__t.md) \* state) <br>_Destroy a state filter and release all memory._  |
|  void | [**dp\_state\_filter\_get\_state**](#function-dp_state_filter_get_state) (const [**dp\_state\_filter\_state\_t**](structdp__state__filter__state__t.md) \* state, void \* blob) <br>_Serialize the filter into_ `blob:` _gains, estimates, bn and t._ |
|  void | [**dp\_state\_filter\_init**](#function-dp_state_filter_init) ([**dp\_state\_filter\_state\_t**](structdp__state__filter__state__t.md) \* state, double bn, double t) <br>_Initialise a filter in place (no allocation)._  |
|  double | [**dp\_state\_filter\_loop\_bandwidth**](#function-dp_state_filter_loop_bandwidth) (int order, double kp, double ki, double kr, double delay, double gain) <br>_The exact delivered noise bandwidth of that loop, in cycles per update:_ `Bn = (1/2) * sum h[n]^2` _of its impulse response. The phase is the oscillator's at the detector instant, the phase the error is formed from; the phase just after the proportional step of the same update has a smaller sum of squares (design page section 11.1, which gives the ratio)._ |
|  int | [**dp\_state\_filter\_loop\_stable**](#function-dp_state_filter_loop_stable) (int order, double kp, double ki, double kr, double delay, double gain) <br>_Whether a loop of this order with these gains is stable._  |
|  int | [**dp\_state\_filter\_params\_ok**](#function-dp_state_filter_params_ok) (double bn, double t) <br>_The domain the loop is defined on:_ `bn >= 0` _,_`t > 0` _, both finite, and_`bn*t` _zero or between STATE\_FILTER\_BNT\_MIN and STATE\_FILTER\_BNT\_MAX._ |
|  const char \* | [**dp\_state\_filter\_params\_why**](#function-dp_state_filter_params_why) (double bn, double t) <br>_The reason (_ `bn` _,_`t` _) is outside the domain, naming the argument, or NULL when it is inside._ |
|  void | [**dp\_state\_filter\_reset**](#function-dp_state_filter_reset) ([**dp\_state\_filter\_state\_t**](structdp__state__filter__state__t.md) \* state) <br>_Zero the frequency and rate estimates, keeping the gains._  |
|  int | [**dp\_state\_filter\_set\_state**](#function-dp_state_filter_set_state) ([**dp\_state\_filter\_state\_t**](structdp__state__filter__state__t.md) \* state, const void \* blob) <br>_Restore state; DP\_OK, or DP\_ERR\_INVALID if the blob is refused. The envelope is checked, the flavor word must be 0, and the payload is decoded into a temporary and accepted only when_ [_**dp\_state\_filter\_state\_ok()**_](state__filter__core_8h.md#function-dp_state_filter_state_ok) _accepts it. The estimates are not checked: a NaN in them is garbage in. A refused blob leaves_`state` _unchanged._ |
|  bool | [**dp\_state\_filter\_stable**](#function-dp_state_filter_stable) (const [**dp\_state\_filter\_state\_t**](structdp__state__filter__state__t.md) \* state, double delay, double gain) <br>_Whether this filter is stable at a detector delay and loop gain._  |
|  size\_t | [**dp\_state\_filter\_state\_bytes**](#function-dp_state_filter_state_bytes) (const [**dp\_state\_filter\_state\_t**](structdp__state__filter__state__t.md) \* state) <br>_Serialized-state byte size._  |
|  int | [**dp\_state\_filter\_state\_ok**](#function-dp_state_filter_state_ok) (const [**dp\_state\_filter\_state\_t**](structdp__state__filter__state__t.md) \* s) <br>_Whether a stored filter is one a live filter could hold._  |
|  double | [**dp\_state\_filter\_steady\_error**](#function-dp_state_filter_steady_error) (int order, double ki, double kr, int input\_order, double x) <br>_The steady phase error of a loop of this order to a polynomial input, from the final-value theorem on the discrete loop._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) double | [**dp\_state\_filter\_step**](#function-dp_state_filter_step) ([**dp\_state\_filter\_state\_t**](structdp__state__filter__state__t.md) \* state, double x) <br>_Advance the loop one update with error_ `x` _and return the control value the tracker should apply._ |
|  void | [**dp\_state\_filter\_steps**](#function-dp_state_filter_steps) ([**dp\_state\_filter\_state\_t**](structdp__state__filter__state__t.md) \* state, const double \* x, double \* out, size\_t n) <br>_Filter a block of loop errors, one control value per update._  |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**STATE\_FILTER\_BNT\_MAX**](state__filter__core_8h.md#define-state_filter_bnt_max)  `0.12`<br>_See STATE\_FILTER\_K\_MIN._  |
| define  | [**STATE\_FILTER\_BNT\_MIN**](state__filter__core_8h.md#define-state_filter_bnt_min)  `1e-30`<br>_The smallest nonzero_ `bn*t` _the filter accepts._ |
| define  | [**STATE\_FILTER\_DELAY\_LIMIT**](state__filter__core_8h.md#define-state_filter_delay_limit)  `4.0`<br>_The largest detector delay, in updates, the exact functions take._  |
| define  | [**STATE\_FILTER\_DELAY\_MAX**](state__filter__core_8h.md#define-state_filter_delay_max)  `2.0`<br>_See STATE\_FILTER\_K\_MIN._  |
| define  | [**STATE\_FILTER\_K\_MAX**](state__filter__core_8h.md#define-state_filter_k_max)  `2.0`<br>_See STATE\_FILTER\_K\_MIN._  |
| define  | [**STATE\_FILTER\_K\_MIN**](state__filter__core_8h.md#define-state_filter_k_min)  `0.5`<br>_The stability rectangle the automatic case is good for._  |
| define  | [**STATE\_FILTER\_SHAPE\_A**](state__filter__core_8h.md#define-state_filter_shape_a)  `2.0`<br>_The shape of the loop: the analog prototype is_ `s^3 + B*w0*s^2 + A*w0^2*s + w0^3` _._ |
| define  | [**STATE\_FILTER\_SHAPE\_B**](state__filter__core_8h.md#define-state_filter_shape_b)  `2.0`<br>_See STATE\_FILTER\_SHAPE\_A._  |
| define  | [**STATE\_FILTER\_STATE\_MAGIC**](state__filter__core_8h.md#define-state_filter_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('S', 'F', 'L', 'T')`<br> |
| define  | [**STATE\_FILTER\_STATE\_VERSION**](state__filter__core_8h.md#define-state_filter_state_version)  `1u`<br> |

## Detailed Description


An error `e` in, a control value out, with two accumulators in cascade:  so `F(z) = kp + ki/(1 - z^-1) + kr/(1 - z^-1)^2`. `integ` keeps the meaning it has in [**dp\_loop\_filter\_state\_t**](structdp__loop__filter__state__t.md) (the running frequency estimate, in the caller's control units per update); `rate` is its slope, in the same units per update. A loop with this filter has no steady phase error on a frequency ramp, where the second-order loop's error is `r/ki`.


#### The caller gives &lt;tt&gt;bn&lt;/tt&gt; and &lt;tt&gt;t&lt;/tt&gt;, and gets a stable loop



The three gains come from a loop noise bandwidth `bn` (cycles per sample) and an update period `t` (samples), with no other coefficient to pick. The loop's shape is fixed: the analog prototype `s^3 + b*w0*s^2 + a*w0^2*s + w0^3` with `STATE_FILTER_SHAPE_A` and `STATE_FILTER_SHAPE_B` (a Butterworth placement) has the gains `kp = b*th`, `ki = a*th^2`, `kr = th^3`. The scale `th` is SOLVED so that the exact noise bandwidth of the discrete loop equals `bn*t`: the delivered bandwidth is the one asked for at a detector delay of zero, with no excess to correct for. The derivation is docs/design/loop-filter.md section 11.1.



#### What it needs of the rest of the loop



Like the second-order filter, the promise assumes a unit-gain loop: the contract is stated on [**dp\_state\_filter\_create\_why()**](state__filter__core_8h.md#function-dp_state_filter_create_why) (and so on the Python class's docstring) and in docs/design/loop-filter.md section 3. A third-order loop is CONDITIONALLY stable in the loop gain `K` (the discriminator slope times the oscillator gain): it needs `K` above a lower edge near `1/(a*b)` of nominal, and the sampled loop has an upper edge that falls with the detector's delay. [**dp\_state\_filter\_create()**](state__filter__core_8h.md#function-dp_state_filter_create) accepts a `bn*t` up to STATE\_FILTER\_BNT\_MAX, the largest for which the loop is stable at every gain from STATE\_FILTER\_K\_MIN to STATE\_FILTER\_K\_MAX and every detector delay up to STATE\_FILTER\_DELAY\_MAX updates. The object cannot see the gain or the delay: a discriminator whose slope is below the lower edge breaks the loop with no error reported. [**dp\_state\_filter\_stable()**](state__filter__core_8h.md#function-dp_state_filter_stable) and [**dp\_state\_filter\_bandwidth()**](state__filter__core_8h.md#function-dp_state_filter_bandwidth) answer for a gain and delay you know.


The state struct is public so a tracker can embed it by value, as with [**dp\_loop\_filter\_state\_t**](structdp__loop__filter__state__t.md), and drive it with [**dp\_state\_filter\_init()**](state__filter__core_8h.md#function-dp_state_filter_init) and the inline [**dp\_state\_filter\_step()**](state__filter__core_8h.md#function-dp_state_filter_step).


Lifecycle: `create -> (step / steps / configure / reset)* -> destroy`



```C++
dp_state_filter_state_t *sf = dp_state_filter_create(0.01, 1.0);
double ctl = dp_state_filter_step(sf, 0.25);
dp_state_filter_destroy(sf);
```
 



    
## Public Functions Documentation




### function dp\_state\_filter\_bandwidth 

_This filter's exact delivered noise bandwidth at a detector delay and loop gain._ 
```C++
double dp_state_filter_bandwidth (
    const dp_state_filter_state_t * state,
    double delay,
    double gain
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `delay` Detector delay in updates. 
* `gain` Loop gain as a multiple of nominal. 



**Returns:**

Cycles per update, or NaN if not stable there. 




**See also:** [**dp\_state\_filter\_loop\_bandwidth()**](state__filter__core_8h.md#function-dp_state_filter_loop_bandwidth)

```C++
>>> from doppler.track import StateFilter
>>> sf = StateFilter(bn=0.03, t=1.0)
>>> round(sf.bandwidth(), 6)           # delay 0: the bandwidth asked for
0.03
>>> round(sf.bandwidth(delay=1.0), 6)  # one update of detector delay
0.032646
>>> sf.bandwidth(gain=0.05)            # unstable: no bandwidth
nan
```
 


        

<hr>



### function dp\_state\_filter\_configure 

_Retune the gains for a new_ `(bn, t)` _without disturbing the frequency and rate estimates._
```C++
int dp_state_filter_configure (
    dp_state_filter_state_t * state,
    double bn,
    double t
) 
```



As [**dp\_loop\_filter\_configure()**](loop__filter__core_8h.md#function-dp_loop_filter_configure) does for the second-order filter: widen to acquire, narrow to track, holding the estimates. A `(bn, t)` outside [**dp\_state\_filter\_params\_ok()**](state__filter__core_8h.md#function-dp_state_filter_params_ok) is refused and changes nothing.




**Parameters:**


* `state` Must be non-NULL. 
* `bn` Loop noise bandwidth, normalized cycles/sample. 
* `t` Update period in samples. 



**Returns:**

`DP_OK`, or `DP_ERR_INVALID` outside the domain.



```C++
>>> from doppler.track import StateFilter
>>> sf = StateFilter(bn=0.01, t=1.0)
>>> _ = sf.step(1.0)
>>> before = sf.rate
>>> sf.configure(0.05, 1.0)            # widen the loop, keep lock
>>> sf.rate == before                  # the estimates are preserved
True
>>> round(sf.kp, 6)                    # the gains rose
0.111058
```
 


        

<hr>



### function dp\_state\_filter\_create 

_Create a state filter, validating its arguments. The binding calls_ [_**dp\_state\_filter\_create\_why()**_](state__filter__core_8h.md#function-dp_state_filter_create_why) _, which also says why it refused._
```C++
dp_state_filter_state_t * dp_state_filter_create (
    double bn,
    double t
) 
```





**Parameters:**


* `bn` Loop noise bandwidth, normalized cycles/sample; &gt;= 0 (default 0.01). `bn = 0` is a deliberately frozen loop. 
* `t` Update period in samples; &gt; 0 (default 1.0). 



**Returns:**

Heap-allocated state, or NULL if `(bn, t)` is outside [**dp\_state\_filter\_params\_ok()**](state__filter__core_8h.md#function-dp_state_filter_params_ok) or on allocation failure. 




**Note:**

Caller must call [**dp\_state\_filter\_destroy()**](state__filter__core_8h.md#function-dp_state_filter_destroy) when done. 





        

<hr>



### function dp\_state\_filter\_create\_why 

_Create a third-order state filter whose closed loop has noise bandwidth_ `bn` _, for a loop whose rest has unit gain: the one constructor the Python class_`StateFilter` _calls._
```C++
dp_state_filter_state_t * dp_state_filter_create_why (
    double bn,
    double t,
    const char ** why
) 
```



#### The contract



The closed loop's noise bandwidth is `bn` cycles per sample, a fixed function of `kp`, `ki` and `kr`, provided the rest of the loop has unit gain. The filter cannot see the rest of the loop, so each of these is the caller's to deliver, and each is checkable:



* The error detector's slope at lock is 1 per unit of the filter's error input, open loop and noise free, in the unit the control is applied in per update; the radians-to-cycles factor, where the oscillator takes cycles, is applied once after the filter, never inside the loop gain.
* The actuator applies the control with unit gain, once per update, `t` samples apart.
* The loop is tracking, inside the detector's linear region.
* The error reaches the filter and the control the actuator within the same update: the delivered bandwidth is the one asked for at a delay of zero.




A loop gain K other than 1 changes the delivered bandwidth, and this third-order loop is also stable only for K above a lower edge near `1/(a*b)` of nominal and below an upper edge that falls with the delay (docs/design/loop-filter.md section 12), so a detector slope below that edge breaks the loop with no error reported. The second-order closed form `(4*K*zeta^2 + 1) / (4*zeta^2 + 1)` does not apply to it. A detector whose slope is not 1, or rides on signal level or SNR, is scaled to unit slope before the filter; a slope that rides on level makes the bandwidth ride on level.


Held by the state filter's own tests for the promise, `test_loop_filter_contract.py` for these sentences on every face, and the detector matrix of doppler#3229 for the callers' side.



#### Arguments





**Parameters:**


* `bn` Loop noise bandwidth, normalized cycles/sample, of the closed loop given a unit-slope detector (see the contract); &gt;= 0 (default 0.01). `bn = 0` is a deliberately frozen loop. 
* `t` Update period in samples; &gt; 0 (default 1.0). 
* `why` Where the refusal's sentence goes, naming the argument it refused ([**dp\_state\_filter\_params\_why()**](state__filter__core_8h.md#function-dp_state_filter_params_why)); set to NULL when the filter is built. May be NULL. 



**Returns:**

Heap-allocated state, or NULL (with `why` set) outside the domain or on allocation failure. 




**Note:**

Caller must call [**dp\_state\_filter\_destroy()**](state__filter__core_8h.md#function-dp_state_filter_destroy) when done.



```C++
>>> from doppler.track import StateFilter
>>> sf = StateFilter(bn=0.01, t=1.0)
>>> round(sf.kp, 6), round(sf.ki, 6)   # the gains follow from bn and t
(0.023622, 0.000279)
>>> sf.integ, sf.rate                  # both estimates start at zero
(0.0, 0.0)
>>> StateFilter(bn=0.5)                # bn*t past the stability bound
Traceback (most recent call last):
    ...
ValueError: bn*t is above the largest the filter accepts, the bound the third-order loop is stable to
```
 



        

<hr>



### function dp\_state\_filter\_destroy 

_Destroy a state filter and release all memory._ 
```C++
void dp_state_filter_destroy (
    dp_state_filter_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function dp\_state\_filter\_get\_state 

_Serialize the filter into_ `blob:` _gains, estimates, bn and t._
```C++
void dp_state_filter_get_state (
    const dp_state_filter_state_t * state,
    void * blob
) 
```





**Parameters:**


* `state` Filter state. Must be non-NULL. 
* `blob` At least [**dp\_state\_filter\_state\_bytes()**](state__filter__core_8h.md#function-dp_state_filter_state_bytes) bytes. 




        

<hr>



### function dp\_state\_filter\_init 

_Initialise a filter in place (no allocation)._ 
```C++
void dp_state_filter_init (
    dp_state_filter_state_t * state,
    double bn,
    double t
) 
```



Computes the gains for `bn` and `t` and stores both. Does **not** touch `integ` or `rate`, so it doubles as a reconfigure that preserves lock. Arguments are NOT validated here, on purpose, as in [**dp\_loop\_filter\_init()**](loop__filter__core_8h.md#function-dp_loop_filter_init): the embedder checks [**dp\_state\_filter\_params\_ok()**](state__filter__core_8h.md#function-dp_state_filter_params_ok) first. An argument outside it gives gains no stability claim is made for (past STATE\_FILTER\_BNT\_MAX the corner of the rectangle goes unstable, but only well past it: the exact bound is about 0.1248).




**Parameters:**


* `state` Must be non-NULL. 
* `bn` Loop noise bandwidth, normalized cycles/sample. 
* `t` Update period in samples. 




        

<hr>



### function dp\_state\_filter\_loop\_bandwidth 

_The exact delivered noise bandwidth of that loop, in cycles per update:_ `Bn = (1/2) * sum h[n]^2` _of its impulse response. The phase is the oscillator's at the detector instant, the phase the error is formed from; the phase just after the proportional step of the same update has a smaller sum of squares (design page section 11.1, which gives the ratio)._
```C++
double dp_state_filter_loop_bandwidth (
    int order,
    double kp,
    double ki,
    double kr,
    double delay,
    double gain
) 
```



Solved exactly (a discrete Lyapunov equation), not summed. For order 2 at `delay = 0` and `gain = 1` it equals `(2 kp^2 + (2 + kp) ki) / (2 kp (4 - 2 kp - ki))`. The delay and gain model are those of [**dp\_state\_filter\_loop\_stable()**](state__filter__core_8h.md#function-dp_state_filter_loop_stable). The solve is on the loop's own state space, where a slow loop is a matrix near the identity, so it holds a relative accuracy near 1e-10 for gains of a loop of `bn*t` down to 1e-6; it is not claimed below that.




**Parameters:**


* `order` 2 or 3. 
* `kp` Proportional gain. 
* `ki` Frequency-path gain. 
* `kr` Rate-path gain (order 3). 
* `delay` Detector delay in updates, from 0 to STATE\_FILTER\_DELAY\_LIMIT. 
* `gain` Loop gain as a multiple of nominal, &gt; 0. 



**Returns:**

The bandwidth, or NaN if the loop is not stable there or an argument is outside its domain. 





        

<hr>



### function dp\_state\_filter\_loop\_stable 

_Whether a loop of this order with these gains is stable._ 
```C++
int dp_state_filter_loop_stable (
    int order,
    double kp,
    double ki,
    double kr,
    double delay,
    double gain
) 
```



The exact test on the characteristic polynomial of the discrete loop of docs/design/loop-filter.md section 11.1: a unit detector, the filter, an oscillator that accumulates the control, a detector delay of `delay` updates, and a loop gain of `gain` times nominal. The one function for both orders: `order` 2 is [**dp\_loop\_filter\_state\_t**](structdp__loop__filter__state__t.md)'s recurrence (`kr` is ignored), `order` 3 this object's.


At an integer `delay` the delay is a real transport delay and the answer is exact. Between integers the model is linear interpolation of the detector's error between the two nearest updates, which low-passes the detector, a different model from a pure fractional delay, whose stability this function does not state; the rectangle's claim is exact at integer delays and holds under this model between them. A detector that averages over a window is a third model the caller owns.


The test is whether the sum `P = sum_k A^k b b^T (A^T)^k` converges (below), which is stability only when the loop's state is driven (the pair (A, b) is controllable): `P` is then the solution of the loop's Lyapunov equation, and it exists exactly when every pole is inside the unit circle. Where the state is not driven, the answer is 0: a gain at zero leaves a state undriven or marginal, so the frozen loop (`bn = 0`, every gain zero), an order-3 loop with `kr = 0` and an order-2 loop with `ki = 0` (a pole-zero cancellation at `z = 1`) all read as not stable, and [**dp\_state\_filter\_loop\_bandwidth()**](state__filter__core_8h.md#function-dp_state_filter_loop_bandwidth) is NaN for them.


Both answers come from one computation in `double` on every platform: the loop's state matrix `A` is balanced by a power-of-two diagonal similarity and then squared repeatedly while `P = sum_k A^k b b^T (A^T)^k` accumulates by `P <- P + A P A^T`, `A <- A A`. Each term of the sum is positive semidefinite, so the sum cannot lose definiteness by cancellation (an individual entry may be negative); the bandwidth is `(d^2 + c P c^T) / 2` of that `P`.


The decision rule is exact: after each pass the largest absolute entry of the balanced `A^(2^j)` is read. Below 1e-12 the loop is stable and the sum is complete (every later term is under 1e-24 of the last). Non-finite or above 1e150 the loop is not stable, and the function returns 0 there, before overflow can make any entry NaN. After 64 passes without either, the loop is not stable (a pole at radius `1 - delta` is decided in about `log2 (40/delta)` passes, so the cap reaches `delta` near 1e-17). A pole within about 1e-12 of the unit circle is within rounding of it, and the answer there is not guaranteed either way; a marginal loop (a gain at zero, a pole at `z = 1`) never goes to zero and reads as not stable.


What the tests run, and what they do not: the exact bandwidth at delay 0 and loop gain 1 against the bandwidth asked for, from `bn*t` 1e-6 to STATE\_FILTER\_BNT\_MAX (1203 cells, within 1e-9; not claimed below 1e-6, which is above STATE\_FILTER\_BNT\_MIN); the verdict and the bandwidth against a simulation of the real step at one design loop (`bn*t` 0.03) for delays 0, 0.5, 1, 1.25, 2 and 3 and loop gains 1, 0.7 and 1.6; loops built at a known radius either side of 1 (1e-6 and 1e-9); and gains off the design manifold (a Kalman filter's gains while its covariance converges, `bn*t` 1e-3 to 0.08) against a simulation of the loop's own state recursion. The full sweep (every `bn*t` from the floor to the bound, delays 0 to 4, loop gains 0.5 to 2) is not run.




**Parameters:**


* `order` 2 or 3. 
* `kp` Proportional gain. 
* `ki` Frequency-path gain. 
* `kr` Rate-path gain (order 3). 
* `delay` Detector delay in updates, from 0 to STATE\_FILTER\_DELAY\_LIMIT. 
* `gain` Loop gain as a multiple of nominal, &gt; 0. 



**Returns:**

1 if every pole is strictly inside the unit circle, 0 if not, if the loop is not controllable, or if an argument is outside its domain. 





        

<hr>



### function dp\_state\_filter\_params\_ok 

_The domain the loop is defined on:_ `bn >= 0` _,_`t > 0` _, both finite, and_`bn*t` _zero or between STATE\_FILTER\_BNT\_MIN and STATE\_FILTER\_BNT\_MAX._
```C++
int dp_state_filter_params_ok (
    double bn,
    double t
) 
```



The ONE predicate; [**dp\_state\_filter\_create()**](state__filter__core_8h.md#function-dp_state_filter_create) and [**dp\_state\_filter\_configure()**](state__filter__core_8h.md#function-dp_state_filter_configure) refuse outside it, and an embedder that takes these numbers from a caller calls it before the unguarded [**dp\_state\_filter\_init()**](state__filter__core_8h.md#function-dp_state_filter_init).




**Parameters:**


* `bn` Loop noise bandwidth, normalized cycles/sample. 
* `t` Update period, in samples. 



**Returns:**

1 inside the domain, 0 outside it (a NaN is outside). 





        

<hr>



### function dp\_state\_filter\_params\_why 

_The reason (_ `bn` _,_`t` _) is outside the domain, naming the argument, or NULL when it is inside._
```C++
const char * dp_state_filter_params_why (
    double bn,
    double t
) 
```



The same predicate as [**dp\_state\_filter\_params\_ok()**](state__filter__core_8h.md#function-dp_state_filter_params_ok), with a sentence for the first condition that fails: `t` not positive and finite, `bn` NaN, `bn` negative, `bn` infinite, or `bn*t` under the floor or over the bound. The strings are static.




**Parameters:**


* `bn` Loop noise bandwidth. 
* `t` Update period in samples. 



**Returns:**

A static sentence, or NULL if the pair is inside the domain. 





        

<hr>



### function dp\_state\_filter\_reset 

_Zero the frequency and rate estimates, keeping the gains._ 
```C++
void dp_state_filter_reset (
    dp_state_filter_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL.


```C++
>>> from doppler.track import StateFilter
>>> sf = StateFilter(bn=0.02, t=1.0)
>>> for _ in range(10):
...     _ = sf.step(1.0)               # open loop: both estimates ramp
>>> round(sf.integ, 6), round(sf.rate, 6)
(0.011506, 0.000126)
>>> sf.reset()
>>> sf.integ, sf.rate                  # cleared, gains kept
(0.0, 0.0)
```
 


        

<hr>



### function dp\_state\_filter\_set\_state 

_Restore state; DP\_OK, or DP\_ERR\_INVALID if the blob is refused. The envelope is checked, the flavor word must be 0, and the payload is decoded into a temporary and accepted only when_ [_**dp\_state\_filter\_state\_ok()**_](state__filter__core_8h.md#function-dp_state_filter_state_ok) _accepts it. The estimates are not checked: a NaN in them is garbage in. A refused blob leaves_`state` _unchanged._
```C++
int dp_state_filter_set_state (
    dp_state_filter_state_t * state,
    const void * blob
) 
```





**Parameters:**


* `state` Filter state to overwrite. Must be non-NULL. 
* `blob` Blob from [**dp\_state\_filter\_get\_state()**](state__filter__core_8h.md#function-dp_state_filter_get_state). 



**Returns:**

DP\_OK, or DP\_ERR\_INVALID. 





        

<hr>



### function dp\_state\_filter\_stable 

_Whether this filter is stable at a detector delay and loop gain._ 
```C++
bool dp_state_filter_stable (
    const dp_state_filter_state_t * state,
    double delay,
    double gain
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `delay` Detector delay in updates. 
* `gain` Loop gain as a multiple of nominal. 



**Returns:**

true if stable, false if not. 




**See also:** [**dp\_state\_filter\_loop\_stable()**](state__filter__core_8h.md#function-dp_state_filter_loop_stable)

```C++
>>> from doppler.track import StateFilter
>>> sf = StateFilter(bn=0.12, t=1.0)   # the bound itself
>>> sf.stable(delay=2.0, gain=2.0)     # worst corner of the rectangle
True
>>> sf.stable(delay=2.0, gain=400.0)   # a detector slope far too high
False
>>> sf.stable(gain=0.05)               # ...and far too low
False
```
 


        

<hr>



### function dp\_state\_filter\_state\_bytes 

_Serialized-state byte size._ 
```C++
size_t dp_state_filter_state_bytes (
    const dp_state_filter_state_t * state
) 
```





**Parameters:**


* `state` Filter state (unused: the size is fixed). 



**Returns:**

Envelope, the flavor word and the seven doubles. 





        

<hr>



### function dp\_state\_filter\_state\_ok 

_Whether a stored filter is one a live filter could hold._ 
```C++
int dp_state_filter_state_ok (
    const dp_state_filter_state_t * s
) 
```



For restoring a serialized filter, whose gains are taken verbatim: the filter must be inside [**dp\_state\_filter\_params\_ok()**](state__filter__core_8h.md#function-dp_state_filter_params_ok) at its own `bn` and `t`, and its gains must match the ones [**dp\_state\_filter\_init()**](state__filter__core_8h.md#function-dp_state_filter_init) writes there to a relative tolerance of 1e-9. `integ` and `rate` are not checked.


The 1e-9 is an allowance between platforms, and what is known of the spread is this: the test holds the gains within 1e-12 of a long double solve of the same closed form on every platform whose long double is wider than double (x86-64 Linux, 80-bit; arm64 Linux, 128-bit), so two such platforms agree within 2e-12 and the tolerance is 500 times that. macOS and Windows have no wider reference and are not covered by the number; the tolerance is to be tightened to a stated multiple once they have one.




**Parameters:**


* `s` The stored filter. 



**Returns:**

1 if it is one a live filter could hold, 0 otherwise. 





        

<hr>



### function dp\_state\_filter\_steady\_error 

_The steady phase error of a loop of this order to a polynomial input, from the final-value theorem on the discrete loop._ 
```C++
double dp_state_filter_steady_error (
    int order,
    double ki,
    double kr,
    int input_order,
    double x
) 
```



`input_order` 1 is a frequency step, 2 a frequency ramp, 3 a frequency acceleration; `x` is the input's size in cycles per update raised to its order (a ramp of `R` Hz/s at an update period of `T` seconds is `x = R*T^2`). The error is 0 where the loop nulls the input (a step in either order, a ramp in order 3), `x/ki` for a ramp in order 2, `x/kr` for an acceleration in order 3, and +infinity where it does not settle (an acceleration in order 2, whose error grows by `x/ki` every update).




**Parameters:**


* `order` 2 or 3. 
* `ki` Frequency-path gain. 
* `kr` Rate-path gain (order 3). 
* `input_order` 1, 2 or 3. 
* `x` Size of the input, cycles per update to the power of its order. 



**Returns:**

The steady phase error in cycles, or NaN for an unknown order. 





        

<hr>



### function dp\_state\_filter\_step 

_Advance the loop one update with error_ `x` _and return the control value the tracker should apply._
```C++
JM_FORCEINLINE  JM_HOT double dp_state_filter_step (
    dp_state_filter_state_t * state,
    double x
) 
```



`rate += kr*x; integ += rate + ki*x; control = integ + kp*x`. Inline, with no test of any kind: this is the hot step of a tracking loop.




**Parameters:**


* `state` Must be non-NULL. 
* `x` Loop error (discriminator output) for this update. 



**Returns:**

Control value `integ + kp*x`.



```C++
>>> from doppler.track import StateFilter
>>> sf = StateFilter(bn=0.02, t=1.0)
>>> round(sf.step(1.0), 6)             # unit error: kr + ki + kp
0.047602
>>> round(sf.rate, 8), round(sf.integ, 8)
(1.257e-05, 0.00109406)
```
 


        

<hr>



### function dp\_state\_filter\_steps 

_Filter a block of loop errors, one control value per update._ 
```C++
void dp_state_filter_steps (
    dp_state_filter_state_t * state,
    const double * x,
    double * out,
    size_t n
) 
```



Equivalent to [**dp\_state\_filter\_step()**](state__filter__core_8h.md#function-dp_state_filter_step) once per element in order, carrying both accumulators across the block.




**Parameters:**


* `state` Component state (mutated across the block). 
* `x` Loop-error array, one discriminator sample per update. 
* `out` Control-value array (length &gt;= n; may alias `x`). 
* `n` Number of updates.


```C++
>>> import numpy as np
>>> from doppler.track import StateFilter
>>> sf = StateFilter(bn=0.05, t=1.0)
>>> ctl = sf.steps(np.full(50, 0.1))   # constant error into the loop
>>> round(float(ctl[0]), 4)            # the first nudge
0.0117
>>> round(float(ctl[-1]), 4)           # open loop: ramping
0.0638
```
 


        

<hr>
## Macro Definition Documentation





### define STATE\_FILTER\_BNT\_MAX 

_See STATE\_FILTER\_K\_MIN._ 
```C++
#define STATE_FILTER_BNT_MAX `0.12`
```




<hr>



### define STATE\_FILTER\_BNT\_MIN 

_The smallest nonzero_ `bn*t` _the filter accepts._
```C++
#define STATE_FILTER_BNT_MIN `1e-30`
```



Below it the gains `th`, `a*th^2` and `th^3` approach the underflow of a double (`th^3` reaches zero near `th = 1e-108`), where the closed form the gains are solved from is 0/0. `bn = 0` is not below it: it is the frozen loop, with all three gains zero. 


        

<hr>



### define STATE\_FILTER\_DELAY\_LIMIT 

_The largest detector delay, in updates, the exact functions take._ 
```C++
#define STATE_FILTER_DELAY_LIMIT `4.0`
```




<hr>



### define STATE\_FILTER\_DELAY\_MAX 

_See STATE\_FILTER\_K\_MIN._ 
```C++
#define STATE_FILTER_DELAY_MAX `2.0`
```




<hr>



### define STATE\_FILTER\_K\_MAX 

_See STATE\_FILTER\_K\_MIN._ 
```C++
#define STATE_FILTER_K_MAX `2.0`
```




<hr>



### define STATE\_FILTER\_K\_MIN 

_The stability rectangle the automatic case is good for._ 
```C++
#define STATE_FILTER_K_MIN `0.5`
```



The loop is stable at every loop gain from STATE\_FILTER\_K\_MIN to STATE\_FILTER\_K\_MAX times nominal and every detector delay from 0 to STATE\_FILTER\_DELAY\_MAX updates (exactly at an integer delay; between integers under the interpolation model of [**dp\_state\_filter\_loop\_stable()**](state__filter__core_8h.md#function-dp_state_filter_loop_stable)), for `bn*t` up to STATE\_FILTER\_BNT\_MAX. `K_MIN` is the floor the automatic case covers for a detector whose slope falls with signal-to-noise ratio: a requirement, with no measured source of that slope on main yet (the async receiver's pull-in validator will print one, #3135); `DELAY_MAX` covers the delay a detector that updates on a sample grid sweeps through. The bound is the largest `bn*t` for which the exact stability test passes at the upper corner (`K_MAX`, `DELAY_MAX`), rounded down; the C test derives it from [**dp\_state\_filter\_loop\_stable()**](state__filter__core_8h.md#function-dp_state_filter_loop_stable) rather than restating it.


What `K_MIN` constrains is the claim, not the predicate: the lower gain edge is set by the shape (about `1/(A*B)` of nominal, a little higher with delay), so no `bn*t` bound can move it, and the predicate bounds `bn*t` from the upper corner alone. The C test asserts that the measured lower edge at the bound, at every delay up to `DELAY_MAX`, is below `K_MIN`, and prints the margin. At the bound that edge is 0.236 of nominal at a delay of zero and 0.310 at `DELAY_MAX`, 4.1 dB under `K_MIN`. 


        

<hr>



### define STATE\_FILTER\_SHAPE\_A 

_The shape of the loop: the analog prototype is_ `s^3 + B*w0*s^2 + A*w0^2*s + w0^3` _._
```C++
#define STATE_FILTER_SHAPE_A `2.0`
```



Both constants live here and nowhere else; a different placement is a change of these two numbers and a rerun of the validator. `A = B = 2` is the third-order Butterworth polynomial `(s+1)(s^2+s+1)`, whose lower gain edge is `1/(A*B) = 0.25` of nominal. The noise bandwidth of the prototype is `w0 * (A*B^2 + A^2 - B) / (4*(A*B - 1))`. 


        

<hr>



### define STATE\_FILTER\_SHAPE\_B 

_See STATE\_FILTER\_SHAPE\_A._ 
```C++
#define STATE_FILTER_SHAPE_B `2.0`
```




<hr>



### define STATE\_FILTER\_STATE\_MAGIC 

```C++
#define STATE_FILTER_STATE_MAGIC `DP_FOURCC ('S', 'F', 'L', 'T')`
```



State-blob magic ("SFLT") checked by [**dp\_state\_filter\_set\_state()**](state__filter__core_8h.md#function-dp_state_filter_set_state). 


        

<hr>



### define STATE\_FILTER\_STATE\_VERSION 

```C++
#define STATE_FILTER_STATE_VERSION `1u`
```



State-blob layout version. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/state_filter/state_filter_core.h`

