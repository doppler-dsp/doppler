

# File loop\_filter\_core.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**loop\_filter**](dir_6fa6397534e50a536c96f665c3cf0441.md) **>** [**loop\_filter\_core.h**](loop__filter__core_8h.md)

[Go to the source code of this file](loop__filter__core_8h_source.md)

_Second-order proportional-integral loop filter — the shared engine of every tracking loop (Costas/PLL, DLL, symbol timing)._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "dp_state.h"`
* `#include "jm_perf.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**loop\_filter\_state\_t**](structloop__filter__state__t.md) <br>_Second-order PI loop filter state (embeddable by value)._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  void | [**loop\_filter\_configure**](#function-loop_filter_configure) ([**loop\_filter\_state\_t**](structloop__filter__state__t.md) \* state, double bn, double zeta, double t) <br>_Retune the loop gains_ `kp` _/_`ki` _for a new (bn, zeta, t) without disturbing the integrator._ |
|  [**loop\_filter\_state\_t**](structloop__filter__state__t.md) \* | [**loop\_filter\_create**](#function-loop_filter_create) (double bn, double zeta, double t) <br>_Create a loop\_filter instance, validating its arguments._  |
|  void | [**loop\_filter\_destroy**](#function-loop_filter_destroy) ([**loop\_filter\_state\_t**](structloop__filter__state__t.md) \* state) <br>_Destroy a loop\_filter instance and release all memory._  |
|  void | [**loop\_filter\_get\_state**](#function-loop_filter_get_state) (const [**loop\_filter\_state\_t**](structloop__filter__state__t.md) \* state, void \* blob) <br>_Serialize the loop state into_ `blob` _._ |
|  void | [**loop\_filter\_init**](#function-loop_filter_init) ([**loop\_filter\_state\_t**](structloop__filter__state__t.md) \* state, double bn, double zeta, double t) <br>_Initialise a loop filter in place (no allocation)._  |
|  void | [**loop\_filter\_reset**](#function-loop_filter_reset) ([**loop\_filter\_state\_t**](structloop__filter__state__t.md) \* state) <br>_Zero the integrator memory while keeping the configured gains._  |
|  int | [**loop\_filter\_set\_state**](#function-loop_filter_set_state) ([**loop\_filter\_state\_t**](structloop__filter__state__t.md) \* state, const void \* blob) <br>_Restore state; DP\_OK, or DP\_ERR\_INVALID if the envelope rejects._  |
|  size\_t | [**loop\_filter\_state\_bytes**](#function-loop_filter_state_bytes) (const [**loop\_filter\_state\_t**](structloop__filter__state__t.md) \* state) <br>_Serialized-state byte size._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) double | [**loop\_filter\_step**](#function-loop_filter_step) ([**loop\_filter\_state\_t**](structloop__filter__state__t.md) \* state, double x) <br>_Advance the loop one update with error_ `x` _and return the control value the tracker should apply._ |
|  void | [**loop\_filter\_steps**](#function-loop_filter_steps) ([**loop\_filter\_state\_t**](structloop__filter__state__t.md) \* state, const double \* x, double \* out, size\_t n) <br>_Filter a whole block of loop errors, returning the control value for each update._  |
|  double | [**loop\_filter\_wn**](#function-loop_filter_wn) (double bn, double zeta) <br>_Natural frequency implied by a loop bandwidth and damping._  |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**LOOP\_FILTER\_STATE\_MAGIC**](loop__filter__core_8h.md#define-loop_filter_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc)('L', 'P', 'F', 'L')`<br> |
| define  | [**LOOP\_FILTER\_STATE\_VERSION**](loop__filter__core_8h.md#define-loop_filter_state_version)  `1u`<br> |

## Detailed Description


An error `e` in, a control value out: `control = integ + kp*e`, with the integrator advancing `integ += ki*e`. The integrator therefore holds the running frequency/rate estimate; `kp*e` is the instantaneous (phase) nudge. Gains `kp` / `ki` come from a loop noise bandwidth, damping, and update period via the standard 2nd-order form ([**loop\_filter\_init()**](loop__filter__core_8h.md#function-loop_filter_init)).


#### Keep &lt;tt&gt;bn \* t &lt;= 0.0112&lt;/tt&gt; and the bandwidth is the one you asked for



`bn` is the noise bandwidth of the CLOSED loop, in cycles per sample, and `t` is the update period in samples — so a loop ticking once per update has a bandwidth of `bn * t` cycles per update, and only that PRODUCT matters. Measured (`native/validation/loop_filter_noise_bw.c`), the delivered bandwidth is always slightly **wide**, never narrow, by a fractional excess with a closed form:  At `zeta` = 0.707 that is `bn*t <= 0.0112` for 1% and `<= 0.0552` for 5%; heavier damping is more forgiving (`0.0450` for 1% at zeta = 2.0). Every configuration shipped in this library sits inside the 1% figure. Being wide rather than narrow is the safe direction — a caller sizing jitter or settling off `bn` is conservative.


The promise assumes the REST of the loop has unit gain (`Kd*K0 = 1`): a discriminator whose slope is 4 delivers a loop four times wider than the `bn` it was handed, and nothing here can detect that. See docs/design/loop-filter.md §3.


Settling follows from the same number: a step settles to +-5% within about 2.3 loop constants (`2.3/bn` updates) at zeta = 0.707, so the `5/bn` rule used throughout this library is comfortable rather than tight.


The state struct is **public** so a tracker can embed it by value (no heap) and drive it with [**loop\_filter\_init()**](loop__filter__core_8h.md#function-loop_filter_init)/loop\_filter\_step() — e.g. a despreader keeps one for the carrier loop and one for the code loop. [**loop\_filter\_create()**](loop__filter__core_8h.md#function-loop_filter_create) is the heap path used by the Python wrapper.


Lifecycle: `create -> (step / steps / configure / reset)* -> destroy`



```C++
loop_filter_state_t *lf = loop_filter_create(0.01, 0.707, 1.0);
double ctl = loop_filter_step(lf, 0.25);   // integ += ki*e; ret integ+kp*e
loop_filter_destroy(lf);
```
 



    
## Public Functions Documentation




### function loop\_filter\_configure 

_Retune the loop gains_ `kp` _/_`ki` _for a new (bn, zeta, t) without disturbing the integrator._
```C++
void loop_filter_configure (
    loop_filter_state_t * state,
    double bn,
    double zeta,
    double t
) 
```



Recomputes the proportional and integral gains from the standard 2nd-order form but leaves `integ` untouched, so a loop can be widened for fast acquisition and then narrowed for steady-state tracking while holding its accumulated frequency/rate estimate — the retune preserves lock.




**Parameters:**


* `state` Must be non-NULL. 
* `bn` Loop noise bandwidth, normalized cycles/sample (&gt;= 0). 
* `zeta` Damping factor (typically 0.707). 
* `t` Update period in samples (&gt; 0).


```C++
>>> from doppler.track import LoopFilter
>>> lf = LoopFilter(bn=0.01, zeta=0.707, t=1.0)
>>> _ = lf.step(1.0)
>>> before = round(lf.integ, 6)
>>> lf.configure(0.05, 0.707, 1.0)   # widen the loop, keep lock
>>> round(lf.integ, 6) == before     # integrator preserved
True
>>> round(lf.kp, 6)                  # proportional gain rose
0.124728
```
 


        

<hr>



### function loop\_filter\_create 

_Create a loop\_filter instance, validating its arguments._ 
```C++
loop_filter_state_t * loop_filter_create (
    double bn,
    double zeta,
    double t
) 
```



This is the untrusted boundary — the Python constructor passes a caller's arbitrary doubles here — so unlike [**loop\_filter\_init()**](loop__filter__core_8h.md#function-loop_filter_init) it **rejects** anything outside the declared domain rather than computing gains from it. `bn = 0` is inside the domain and is accepted: it means a deliberately frozen loop.




**Parameters:**


* `bn` Loop noise bandwidth, normalized cycles/sample; &gt;= 0 and finite (default 0.01). 
* `zeta` Damping factor; &gt; 0 and finite (default 0.707). 
* `t` Update period in samples; &gt; 0 and finite (default 1.0). 



**Returns:**

Heap-allocated state, or NULL if any argument is outside the domain above or on allocation failure. The Python binding turns the former into a `ValueError`. 




**Note:**

Caller must call [**loop\_filter\_destroy()**](loop__filter__core_8h.md#function-loop_filter_destroy) when done. 





        

<hr>



### function loop\_filter\_destroy 

_Destroy a loop\_filter instance and release all memory._ 
```C++
void loop_filter_destroy (
    loop_filter_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function loop\_filter\_get\_state 

_Serialize the loop state into_ `blob` _._
```C++
void loop_filter_get_state (
    const loop_filter_state_t * state,
    void * blob
) 
```




<hr>



### function loop\_filter\_init 

_Initialise a loop filter in place (no allocation)._ 
```C++
void loop_filter_init (
    loop_filter_state_t * state,
    double bn,
    double zeta,
    double t
) 
```



Computes `kp` / `ki` from the loop noise bandwidth `bn` (normalized, cycles/sample), damping `zeta`, and update period `t` (samples), and stores `bn` / `zeta` / `t`. Does **not** touch `integ`, so it doubles as a reconfigure that preserves lock. Use this for a `loop_filter_state_t` embedded by value; [**loop\_filter\_create()**](loop__filter__core_8h.md#function-loop_filter_create) is calloc + [**loop\_filter\_init()**](loop__filter__core_8h.md#function-loop_filter_init).


**Arguments are NOT validated here, on purpose.** This is the by-value path taken by the objects that embed a filter, all of which validate upstream; [**loop\_filter\_create()**](loop__filter__core_8h.md#function-loop_filter_create) is the boundary that faces an untrusted caller and it rejects the same domain this documents. Passing `t = 0` here yields `kp = ki = 0` — a loop that never moves — and a non-finite argument yields NaN gains that never recover.




**Parameters:**


* `state` Must be non-NULL. 
* `bn` Loop noise bandwidth, normalized cycles/sample (&gt;= 0). 
* `zeta` Damping factor (typically 0.707), &gt; 0. 
* `t` Update period in samples (&gt; 0). 




        

<hr>



### function loop\_filter\_reset 

_Zero the integrator memory while keeping the configured gains._ 
```C++
void loop_filter_reset (
    loop_filter_state_t * state
) 
```



Clears the accumulated frequency/rate estimate (`integ`) back to zero but leaves `kp` / `ki` as configured, so the loop reacquires from a clean slate at its current bandwidth — the right thing when a tracker drops lock and must restart, without re-deriving gains.




**Parameters:**


* `state` Must be non-NULL.


```C++
>>> from doppler.track import LoopFilter
>>> lf = LoopFilter(bn=0.02, zeta=0.707, t=1.0)
>>> for _ in range(10):
...     _ = lf.step(1.0)             # ramp the integrator
>>> round(lf.integ, 6)
0.013849
>>> lf.reset()
>>> lf.integ                          # integrator cleared, gains kept
0.0
```
 


        

<hr>



### function loop\_filter\_set\_state 

_Restore state; DP\_OK, or DP\_ERR\_INVALID if the envelope rejects._ 
```C++
int loop_filter_set_state (
    loop_filter_state_t * state,
    const void * blob
) 
```




<hr>



### function loop\_filter\_state\_bytes 

_Serialized-state byte size._ 
```C++
size_t loop_filter_state_bytes (
    const loop_filter_state_t * state
) 
```




<hr>



### function loop\_filter\_step 

_Advance the loop one update with error_ `x` _and return the control value the tracker should apply._
```C++
JM_FORCEINLINE  JM_HOT double loop_filter_step (
    loop_filter_state_t * state,
    double x
) 
```



The PI recurrence is `integ += ki*x; control = integ + kp*x`: the integrator accumulates the running frequency/rate estimate while the proportional term `kp*x` is the instantaneous phase nudge.


Fed a constant error with nothing closing the loop, the integrator — and therefore the control — **ramps without bound**; measured at 1.84x between updates 200 and 400 at `bn = 0.02`. That is the accumulation working, not a defect, and it is what pulls a Costas/DLL/timing loop into lock once the loop IS closed, because a converging loop is one whose error is being driven to zero by the correction. Convergence is a property of the closed loop; this function is one term in it.




**Parameters:**


* `state` Must be non-NULL. 
* `x` Loop error (discriminator output) for this update. 



**Returns:**

Control value `integ+kp*x` to drive the NCO / interpolator.



```C++
>>> from doppler.track import LoopFilter
>>> lf = LoopFilter(bn=0.02, zeta=0.707, t=1.0)
>>> round(lf.step(1.0), 6)   # unit error: control = ki + kp
0.05331
>>> round(lf.integ, 6)       # integrator now holds ki
0.001385
```
 


        

<hr>



### function loop\_filter\_steps 

_Filter a whole block of loop errors, returning the control value for each update._ 
```C++
void loop_filter_steps (
    loop_filter_state_t * state,
    const double * x,
    double * out,
    size_t n
) 
```



Equivalent to calling [**loop\_filter\_step()**](loop__filter__core_8h.md#function-loop_filter_step) once per element of `x` in order, carrying the integrator across the block, so the loop's memory and lock state persist from one call to the next. This is the block path used to run a captured error sequence through the filter in one shot — a plain per-element loop, not a vectorized one: the recurrence is sequential, so each update depends on the one before it.




**Parameters:**


* `state` Component state (mutated across the block). 
* `x` Loop-error array, one discriminator sample per update. 
* `out` Control-value array (length &gt;= n; may alias `x`). 
* `n` Number of updates.


```C++
>>> import numpy as np
>>> from doppler.track import LoopFilter
>>> lf = LoopFilter(bn=0.05, zeta=0.707, t=1.0)
>>> ctl = lf.steps(np.full(50, 0.1))   # constant error into the loop
>>> round(float(ctl[0]), 4)            # first control nudge
0.0133
>>> round(float(ctl[-1]), 4)           # open loop: ramping
0.0541
```
 


        

<hr>



### function loop\_filter\_wn 

_Natural frequency implied by a loop bandwidth and damping._ 
```C++
double loop_filter_wn (
    double bn,
    double zeta
) 
```



`wn = 8*zeta*bn / (4*zeta^2 + 1)`, which at `zeta = 0.707` is `1.8857*bn`. In the same units as `bn:` pass a `bn` normalised to the symbol rate and `wn` is per symbol; pass one normalised to the sample rate and it is per sample.


Public because it is the number every closed form about this loop is written in, and callers were re-deriving it rather than asking. The steady-state phase lag under a frequency RAMP of `r` (cycles per unit time squared) is `2*pi*r / wn^2` — the only one of the two standard disturbances that a type-2 loop does NOT null, and therefore the one a measurement can check a gain against. A frequency STEP is nulled regardless of gain, so it cannot.


The formula had five copies (this file, the loop's own C test, a validation harness, an example and a validation script) and no home; a gain error that moved `wn` would have had to be found five times.


Unguarded, like [**loop\_filter\_init()**](loop__filter__core_8h.md#function-loop_filter_init) and for the same reason: this is the trusting path, and [**loop\_filter\_create()**](loop__filter__core_8h.md#function-loop_filter_create) is the boundary that rejects the domain. `zeta = 0` divides by zero here exactly as it always has.




**Parameters:**


* `bn` Loop noise bandwidth, normalized (&gt;= 0). 
* `zeta` Damping factor (typically 0.707), &gt; 0. 



**Returns:**

The natural frequency, in `bn's` units. 





        

<hr>
## Macro Definition Documentation





### define LOOP\_FILTER\_STATE\_MAGIC 

```C++
#define LOOP_FILTER_STATE_MAGIC `DP_FOURCC ('L', 'P', 'F', 'L')`
```




<hr>



### define LOOP\_FILTER\_STATE\_VERSION 

```C++
#define LOOP_FILTER_STATE_VERSION `1u`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/loop_filter/loop_filter_core.h`

