

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
|  void | [**loop\_filter\_configure**](#function-loop_filter_configure) ([**loop\_filter\_state\_t**](structloop__filter__state__t.md) \* state, double bn, double zeta, double t) <br>_Recompute_ `kp` _/_`ki` _for a new (bn, zeta, t); preserve_`integ` _._ |
|  [**loop\_filter\_state\_t**](structloop__filter__state__t.md) \* | [**loop\_filter\_create**](#function-loop_filter_create) (double bn, double zeta, double t) <br>_Create a loop\_filter instance._  |
|  void | [**loop\_filter\_destroy**](#function-loop_filter_destroy) ([**loop\_filter\_state\_t**](structloop__filter__state__t.md) \* state) <br>_Destroy a loop\_filter instance and release all memory._  |
|  void | [**loop\_filter\_get\_state**](#function-loop_filter_get_state) (const [**loop\_filter\_state\_t**](structloop__filter__state__t.md) \* state, void \* blob) <br>_Serialize the loop state into_ `blob` _._ |
|  void | [**loop\_filter\_init**](#function-loop_filter_init) ([**loop\_filter\_state\_t**](structloop__filter__state__t.md) \* state, double bn, double zeta, double t) <br>_Initialise a loop filter in place (no allocation)._  |
|  void | [**loop\_filter\_reset**](#function-loop_filter_reset) ([**loop\_filter\_state\_t**](structloop__filter__state__t.md) \* state) <br>_Zero the integrator; keep the configured gains._  |
|  int | [**loop\_filter\_set\_state**](#function-loop_filter_set_state) ([**loop\_filter\_state\_t**](structloop__filter__state__t.md) \* state, const void \* blob) <br>_Restore state; DP\_OK, or DP\_ERR\_INVALID if the envelope rejects._  |
|  size\_t | [**loop\_filter\_state\_bytes**](#function-loop_filter_state_bytes) (const [**loop\_filter\_state\_t**](structloop__filter__state__t.md) \* state) <br>_Serialized-state byte size._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) double | [**loop\_filter\_step**](#function-loop_filter_step) ([**loop\_filter\_state\_t**](structloop__filter__state__t.md) \* state, double x) <br>_Advance the loop one update with error_ `x` _; return the control._ |
|  void | [**loop\_filter\_steps**](#function-loop_filter_steps) ([**loop\_filter\_state\_t**](structloop__filter__state__t.md) \* state, const double \* input, double \* output, size\_t n) <br>_Run a block of errors through the loop._  |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**LOOP\_FILTER\_STATE\_MAGIC**](loop__filter__core_8h.md#define-loop_filter_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc)('L', 'P', 'F', 'L')`<br> |
| define  | [**LOOP\_FILTER\_STATE\_VERSION**](loop__filter__core_8h.md#define-loop_filter_state_version)  `1u`<br> |

## Detailed Description


An error `e` in, a control value out: `control = integ + kp*e`, with the integrator advancing `integ += ki*e`. The integrator therefore holds the running frequency/rate estimate; `kp*e` is the instantaneous (phase) nudge. Gains `kp` / `ki` come from a loop noise bandwidth, damping, and update period via the standard 2nd-order form ([**loop\_filter\_init()**](loop__filter__core_8h.md#function-loop_filter_init)).


The state struct is **public** so a tracker can embed it by value (no heap) and drive it with [**loop\_filter\_init()**](loop__filter__core_8h.md#function-loop_filter_init)/loop\_filter\_step() — e.g. a despreader keeps one for the carrier loop and one for the code loop. [**loop\_filter\_create()**](loop__filter__core_8h.md#function-loop_filter_create) is the heap path used by the Python wrapper.


Lifecycle: create -&gt; [step / steps / configure / reset]\* -&gt; destroy



```C++
loop_filter_state_t *lf = loop_filter_create(0.01, 0.707, 1.0);
double ctl = loop_filter_step(lf, 0.25);   // integ += ki*e; ret integ+kp*e
loop_filter_destroy(lf);
```
 


    
## Public Functions Documentation




### function loop\_filter\_configure 

_Recompute_ `kp` _/_`ki` _for a new (bn, zeta, t); preserve_`integ` _._
```C++
void loop_filter_configure (
    loop_filter_state_t * state,
    double bn,
    double zeta,
    double t
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `bn` Loop noise bandwidth, normalized cycles/sample (&gt;= 0). 
* `zeta` Damping factor (typically 0.707). 
* `t` Update period in samples (&gt; 0). 




        

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

_Zero the integrator; keep the configured gains._ 
```C++
void loop_filter_reset (
    loop_filter_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 




        

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

_Advance the loop one update with error_ `x` _; return the control._
```C++
JM_FORCEINLINE  JM_HOT double loop_filter_step (
    loop_filter_state_t * state,
    double x
) 
```



`integ += ki*x; return integ + kp*x`.




**Parameters:**


* `state` Must be non-NULL. 
* `x` Loop error. 



**Returns:**

Control value (integ + kp\*x). 





        

<hr>



### function loop\_filter\_steps 

_Run a block of errors through the loop._ 
```C++
void loop_filter_steps (
    loop_filter_state_t * state,
    const double * input,
    double * output,
    size_t n
) 
```





**Parameters:**


* `state` Component state (mutated). 
* `input` Error array (length &gt;= n). 
* `output` Control array (length &gt;= n; may alias input). 
* `n` Number of updates. 




        

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

