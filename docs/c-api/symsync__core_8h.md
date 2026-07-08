

# File symsync\_core.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**symsync**](dir_bee143323fe2e99a30a6d3a881f82f29.md) **>** [**symsync\_core.h**](symsync__core_8h.md)

[Go to the source code of this file](symsync__core_8h_source.md)

_SymbolSync component API._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "dp_state.h"`
* `#include "farrow/farrow_core.h"`
* `#include "jm_perf.h"`
* `#include "loop_filter/loop_filter_core.h"`
* `#include "nco/nco_core.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**symsync\_state\_t**](structsymsync__state__t.md) <br>_SymbolSync state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  void | [**symsync\_configure**](#function-symsync_configure) ([**symsync\_state\_t**](structsymsync__state__t.md) \* state, double bn, double zeta) <br> |
|  [**symsync\_state\_t**](structsymsync__state__t.md) \* | [**symsync\_create**](#function-symsync_create) (size\_t sps, double bn, double zeta, int order) <br>_Create a symsync instance._  |
|  void | [**symsync\_destroy**](#function-symsync_destroy) ([**symsync\_state\_t**](structsymsync__state__t.md) \* state) <br>_Destroy a symsync instance and release all memory._  |
|  double | [**symsync\_get\_bn**](#function-symsync_get_bn) (const [**symsync\_state\_t**](structsymsync__state__t.md) \* state) <br> |
|  double | [**symsync\_get\_rate**](#function-symsync_get_rate) (const [**symsync\_state\_t**](structsymsync__state__t.md) \* state) <br> |
|  void | [**symsync\_get\_state**](#function-symsync_get_state) (const [**symsync\_state\_t**](structsymsync__state__t.md) \* state, void \* blob) <br> |
|  double | [**symsync\_get\_timing\_error**](#function-symsync_get_timing_error) (const [**symsync\_state\_t**](structsymsync__state__t.md) \* state) <br> |
|  void | [**symsync\_init**](#function-symsync_init) ([**symsync\_state\_t**](structsymsync__state__t.md) \* s, size\_t sps, double bn, double zeta, int order) <br>_Initialise a SymbolSync in place (no allocation)._  |
|  void | [**symsync\_reset**](#function-symsync_reset) ([**symsync\_state\_t**](structsymsync__state__t.md) \* state) <br>_Reset SymbolSync to its post-create state._  |
|  void | [**symsync\_set\_bn**](#function-symsync_set_bn) ([**symsync\_state\_t**](structsymsync__state__t.md) \* state, double val) <br> |
|  int | [**symsync\_set\_state**](#function-symsync_set_state) ([**symsync\_state\_t**](structsymsync__state__t.md) \* state, const void \* blob) <br> |
|  size\_t | [**symsync\_state\_bytes**](#function-symsync_state_bytes) (const [**symsync\_state\_t**](structsymsync__state__t.md) \* state) <br> |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) int | [**symsync\_step**](#function-symsync_step) ([**symsync\_state\_t**](structsymsync__state__t.md) \* s, float complex x, float complex \* y\_out) <br>_Per-sample symbol-timing step (the inline composition API)._  |
|  size\_t | [**symsync\_steps**](#function-symsync_steps) ([**symsync\_state\_t**](structsymsync__state__t.md) \* state, const float complex \* x, size\_t x\_len, float complex \* out, size\_t max\_out) <br> |
|  size\_t | [**symsync\_steps\_max\_out**](#function-symsync_steps_max_out) ([**symsync\_state\_t**](structsymsync__state__t.md) \* state) <br> |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**SYMSYNC\_STATE\_MAGIC**](symsync__core_8h.md#define-symsync_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('S','Y','N','C')`<br> |
| define  | [**SYMSYNC\_STATE\_VERSION**](symsync__core_8h.md#define-symsync_state_version)  `1u`<br> |

## Detailed Description


Lifecycle: create -&gt; [step / steps / reset]\* -&gt; destroy


Example: 
```C++
symsync_state_t *obj = symsync_create(4, 0.01, 0.707, 0);
float complex y = symsync_step(obj, 0.0f + 0.0f * I);
symsync_destroy(obj);
```
 


    
## Public Functions Documentation




### function symsync\_configure 

```C++
void symsync_configure (
    symsync_state_t * state,
    double bn,
    double zeta
) 
```




<hr>



### function symsync\_create 

_Create a symsync instance._ 
```C++
symsync_state_t * symsync_create (
    size_t sps,
    double bn,
    double zeta,
    int order
) 
```





**Parameters:**


* `sps` sps (default: 4). 
* `bn` bn (default: 0.01). 
* `zeta` zeta (default: 0.707). 
* `order` Enum index; 0=linear…2=cubic. 



**Returns:**

Heap-allocated state, or NULL on allocation failure. 




**Note:**

Caller must call [**symsync\_destroy()**](symsync__core_8h.md#function-symsync_destroy) when done. 





        

<hr>



### function symsync\_destroy 

_Destroy a symsync instance and release all memory._ 
```C++
void symsync_destroy (
    symsync_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function symsync\_get\_bn 

```C++
double symsync_get_bn (
    const symsync_state_t * state
) 
```




<hr>



### function symsync\_get\_rate 

```C++
double symsync_get_rate (
    const symsync_state_t * state
) 
```




<hr>



### function symsync\_get\_state 

```C++
void symsync_get_state (
    const symsync_state_t * state,
    void * blob
) 
```




<hr>



### function symsync\_get\_timing\_error 

```C++
double symsync_get_timing_error (
    const symsync_state_t * state
) 
```




<hr>



### function symsync\_init 

_Initialise a SymbolSync in place (no allocation)._ 
```C++
void symsync_init (
    symsync_state_t * s,
    size_t sps,
    double bn,
    double zeta,
    int order
) 
```



The by-value counterpart to [**symsync\_create()**](symsync__core_8h.md#function-symsync_create): lets a composing object embed a [**symsync\_state\_t**](structsymsync__state__t.md) by value and initialise it without a heap allocation ([**symsync\_state\_t**](structsymsync__state__t.md) holds no heap members — the NCO, Farrow and loop filter are all by value). Mirrors [**loop\_filter\_init()**](loop__filter__core_8h.md#function-loop_filter_init)/costas\_init().




**Parameters:**


* `s` State to initialise. Must be non-NULL. 
* `sps` Nominal samples per symbol. 
* `bn` Loop noise bandwidth (normalised to the symbol rate). 
* `zeta` Damping factor (0.707 = critically damped). 
* `order` Farrow interpolator order (0=linear, 1=parabolic, 2=cubic). 




        

<hr>



### function symsync\_reset 

_Reset SymbolSync to its post-create state._ 
```C++
void symsync_reset (
    symsync_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 




        

<hr>



### function symsync\_set\_bn 

```C++
void symsync_set_bn (
    symsync_state_t * state,
    double val
) 
```




<hr>



### function symsync\_set\_state 

```C++
int symsync_set_state (
    symsync_state_t * state,
    const void * blob
) 
```




<hr>



### function symsync\_state\_bytes 

```C++
size_t symsync_state_bytes (
    const symsync_state_t * state
) 
```




<hr>



### function symsync\_step 

_Per-sample symbol-timing step (the inline composition API)._ 
```C++
JM_FORCEINLINE  JM_HOT int symsync_step (
    symsync_state_t * s,
    float complex x,
    float complex * y_out
) 
```



Pushes one input sample into the Farrow history and advances the integer timing NCO. When the NCO crosses its half-scale (mid-symbol) it stores the Gardner mid interpolant; when it wraps (on-time) it forms the on-time interpolant, runs the Gardner TED, steers the NCO frequency, and emits the timing-corrected symbol. Returns 1 and writes `y_out` on an on-time symbol, else 0. [**symsync\_steps()**](symsync__core_8h.md#function-symsync_steps) is exactly this in a loop; a tracking channel inlines it to drive a downstream carrier loop on the recovered symbols.




**Parameters:**


* `s` State. Must be non-NULL. 
* `x` One input sample. 
* `y_out` Receives the symbol when the return is 1. 



**Returns:**

1 if a symbol was emitted (into `y_out`), 0 otherwise. 





        

<hr>



### function symsync\_steps 

```C++
size_t symsync_steps (
    symsync_state_t * state,
    const float complex * x,
    size_t x_len,
    float complex * out,
    size_t max_out
) 
```




<hr>



### function symsync\_steps\_max\_out 

```C++
size_t symsync_steps_max_out (
    symsync_state_t * state
) 
```




<hr>
## Macro Definition Documentation





### define SYMSYNC\_STATE\_MAGIC 

```C++
#define SYMSYNC_STATE_MAGIC `DP_FOURCC ('S','Y','N','C')`
```




<hr>



### define SYMSYNC\_STATE\_VERSION 

```C++
#define SYMSYNC_STATE_VERSION `1u`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/symsync/symsync_core.h`

