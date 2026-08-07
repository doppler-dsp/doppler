

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
|  [**loop\_filter\_state\_t**](structloop__filter__state__t.md) \* | [**loop\_filter\_create**](#function-loop_filter_create) (double bn, double zeta, double t) <br>_Create a loop\_filter instance._  |
|  void | [**loop\_filter\_destroy**](#function-loop_filter_destroy) ([**loop\_filter\_state\_t**](structloop__filter__state__t.md) \* state) <br>_Destroy a loop\_filter instance and release all memory._  |
|  void | [**loop\_filter\_get\_state**](#function-loop_filter_get_state) (const [**loop\_filter\_state\_t**](structloop__filter__state__t.md) \* state, void \* blob) <br>_Serialize the loop state into_ `blob` _._ |
|  void | [**loop\_filter\_init**](#function-loop_filter_init) ([**loop\_filter\_state\_t**](structloop__filter__state__t.md) \* state, double bn, double zeta, double t) <br>_Initialise a loop filter in place (no allocation)._  |
|  void | [**loop\_filter\_reset**](#function-loop_filter_reset) ([**loop\_filter\_state\_t**](structloop__filter__state__t.md) \* state) <br>_Zero the integrator memory while keeping the configured gains._  |
|  int | [**loop\_filter\_set\_state**](#function-loop_filter_set_state) ([**loop\_filter\_state\_t**](structloop__filter__state__t.md) \* state, const void \* blob) <br>_Restore state; DP\_OK, or DP\_ERR\_INVALID if the envelope rejects._  |
|  size\_t | [**loop\_filter\_state\_bytes**](#function-loop_filter_state_bytes) (const [**loop\_filter\_state\_t**](structloop__filter__state__t.md) \* state) <br>_Serialized-state byte size._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) double | [**loop\_filter\_step**](#function-loop_filter_step) ([**loop\_filter\_state\_t**](structloop__filter__state__t.md) \* state, double x) <br>_Advance the loop one update with error_ `x` _and return the control value the tracker should apply._ |
|  void | [**loop\_filter\_steps**](#function-loop_filter_steps) ([**loop\_filter\_state\_t**](structloop__filter__state__t.md) \* state, const double \* x, double \* out, size\_t n) <br>_Filter a whole block of loop errors, returning the control value for each update._  |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**LOOP\_FILTER\_STATE\_MAGIC**](loop__filter__core_8h.md#define-loop_filter_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc)('L', 'P', 'F', 'L')`<br> |
| define  | [**LOOP\_FILTER\_STATE\_VERSION**](loop__filter__core_8h.md#define-loop_filter_state_version)  `1u`<br> |

## Detailed Description


An error `e` in, a control value out: `control = integ + kp*e`, with the integrator advancing `integ += ki*e`. The integrator therefore holds the running frequency/rate estimate; `kp*e` is the instantaneous (phase) nudge. Gains `kp` / `ki` come from a loop noise bandwidth, damping, and update period via the standard 2nd-order form ([**loop\_filter\_init()**](loop__filter__core_8h.md#function-loop_filter_init)).


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

_Create a loop\_filter instance._ 
```C++
loop_filter_state_t * loop_filter_create (
    double bn,
    double zeta,
    double t
) 
```





**Parameters:**


* `bn` Loop noise bandwidth, normalized cycles/sample (default 0.01). 
* `zeta` Damping factor (default 0.707). 
* `t` Update period in samples (default 1.0). 



**Returns:**

Heap-allocated state, or NULL on allocation failure. 




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




**Parameters:**


* `state` Must be non-NULL. 
* `bn` Loop noise bandwidth, normalized cycles/sample (&gt;= 0). 
* `zeta` Damping factor (typically 0.707). 
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



The PI recurrence is `integ += ki*x; control = integ + kp*x`: the integrator accumulates the running frequency/rate estimate while the proportional term `kp*x` is the instantaneous phase nudge. Fed a constant error the integrator ramps linearly and the control converges to the steady-state estimate — the behaviour that pulls a Costas/DLL/timing loop into lock.




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



Equivalent to calling [**loop\_filter\_step()**](loop__filter__core_8h.md#function-loop_filter_step) once per element of `x` in order, carrying the integrator across the block, so the loop's memory and lock state persist from one call to the next. This is the vectorized path used to run a captured error sequence through the filter in one shot.




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
>>> round(float(ctl[-1]), 4)           # converging toward the estimate
0.0541
```
 


        

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

