

# File farrow\_core.h



[**FileList**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**farrow**](dir_f5d5da611c5546f094b053d8a6116219.md) **>** [**farrow\_core.h**](farrow__core_8h.md)

[Go to the source code of this file](farrow__core_8h_source.md)

_Farrow fractional-delay interpolator — linear / parabolic / cubic._ [More...](#detailed-description)

* `#include "doppler/clib_common.h"`
* `#include "doppler/jm_perf.h"`
* `#include "doppler/dp_state.h"`
* `#include "doppler/dp_complex.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**dp\_farrow\_state\_t**](structdp__farrow__state__t.md) <br>_Farrow interpolator state (4-tap delay line + order)._  |


## Public Types

| Type | Name |
| ---: | :--- |
| enum  | [**farrow\_\_core\_8h\_1a06fc87d81c62e9abb8790b6e5713c55b**](#enum-farrow__core_8h_1a06fc87d81c62e9abb8790b6e5713c55b)  <br> |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**dp\_farrow\_state\_t**](structdp__farrow__state__t.md) \* | [**dp\_farrow\_create**](#function-dp_farrow_create) (int order) <br>_Create a Farrow interpolator._  |
|  size\_t | [**dp\_farrow\_delay**](#function-dp_farrow_delay) ([**dp\_farrow\_state\_t**](structdp__farrow__state__t.md) \* state, const float \_Complex \* x, size\_t x\_len, double mu, float \_Complex \* out, size\_t max\_out) <br>_Apply a constant fractional delay of_ `mu` _samples to a CF32 block._ |
|  size\_t | [**dp\_farrow\_delay\_max\_out**](#function-dp_farrow_delay_max_out) ([**dp\_farrow\_state\_t**](structdp__farrow__state__t.md) \* state) <br> |
|  void | [**dp\_farrow\_destroy**](#function-dp_farrow_destroy) ([**dp\_farrow\_state\_t**](structdp__farrow__state__t.md) \* state) <br>_Destroy a Farrow interpolator._  |
|  size\_t | [**dp\_farrow\_get\_group\_delay**](#function-dp_farrow_get_group_delay) (const [**dp\_farrow\_state\_t**](structdp__farrow__state__t.md) \* state) <br> |
|  void | [**dp\_farrow\_get\_state**](#function-dp_farrow_get_state) (const [**dp\_farrow\_state\_t**](structdp__farrow__state__t.md) \* state, void \* blob) <br> |
|  void | [**dp\_farrow\_reset**](#function-dp_farrow_reset) ([**dp\_farrow\_state\_t**](structdp__farrow__state__t.md) \* state) <br>_Clear the interpolator delay line; keep the order._  |
|  int | [**dp\_farrow\_set\_state**](#function-dp_farrow_set_state) ([**dp\_farrow\_state\_t**](structdp__farrow__state__t.md) \* state, const void \* blob) <br> |
|  size\_t | [**dp\_farrow\_state\_bytes**](#function-dp_farrow_state_bytes) (const [**dp\_farrow\_state\_t**](structdp__farrow__state__t.md) \* state) <br> |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) float \_Complex | [**farrow\_eval**](#function-farrow_eval) (const [**dp\_farrow\_state\_t**](structdp__farrow__state__t.md) \* s, float mu) <br>_Interpolate at fractional offset_ `mu` _∈_`[0,1)` _between_`d[1]` _and_`d[2]` _._ |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) void | [**farrow\_init**](#function-farrow_init) ([**dp\_farrow\_state\_t**](structdp__farrow__state__t.md) \* s, int order) <br>_Initialise in place: set order, clear the delay line._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) void | [**farrow\_push**](#function-farrow_push) ([**dp\_farrow\_state\_t**](structdp__farrow__state__t.md) \* s, float \_Complex x) <br>_Push one input sample into the delay line (oldest drops out)._  |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**FARROW\_GROUP\_DELAY**](farrow__core_8h.md#define-farrow_group_delay)  `2u`<br> |
| define  | [**FARROW\_STATE\_MAGIC**](farrow__core_8h.md#define-farrow_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('F', 'R', 'R', 'W')`<br> |
| define  | [**FARROW\_STATE\_VERSION**](farrow__core_8h.md#define-farrow_state_version)  `1u`<br> |

## Detailed Description


A selectable-order Lagrange interpolator in Farrow (Horner-in-µ) form — the lean alternative to a full polyphase resampler when all you need is a fractional-delay tap for a timing loop. All three orders share one 4-tap delay line and interpolate at the SAME point — between the two middle taps — so the **group delay is 2 samples regardless of order**, which keeps a driving symbol-timing loop order-agnostic. Push input samples with [**farrow\_push()**](farrow__core_8h.md#function-farrow_push); evaluate the output at a fractional offset µ ∈ `[0,1)` with [**farrow\_eval()**](farrow__core_8h.md#function-farrow_eval). The fractional offset is meant to come from an integer timing NCO (the post-wrap accumulator value), so the timing stays drift-free while only the interpolation itself is floating point.


order: 0 = linear (2-tap Lagrange), 1 = parabolic (4-tap symmetric piecewise-parabolic Farrow, α = 0.5), 2 = cubic (4-tap cubic Lagrange). All three are symmetric about the interpolation point, so the phase (delay) response is linear — no timing bias. Linear and cubic are exact for degree 1 and 3 polynomials; the piecewise-parabolic trades exactness for a flatter magnitude response than linear at no delay cost.


Lifecycle: dp\_farrow\_create -&gt; (push / eval / reset)\* -&gt; dp\_farrow\_destroy, or embed by value with [**farrow\_init()**](farrow__core_8h.md#function-farrow_init).



```C++
dp_farrow_state_t f;
farrow_init(&f, FARROW_CUBIC);
for (size_t i = 0; i < n; i++) farrow_push(&f, x[i]);
float _Complex y = farrow_eval(&f, 0.3f);   // x interpolated 0.3 past tap[1]
```
 


    
## Public Types Documentation




### enum farrow\_\_core\_8h\_1a06fc87d81c62e9abb8790b6e5713c55b 

```C++
enum farrow__core_8h_1a06fc87d81c62e9abb8790b6e5713c55b {
    FARROW_LINEAR = 0,
    FARROW_PARABOLIC = 1,
    FARROW_CUBIC = 2
};
```




<hr>
## Public Functions Documentation




### function dp\_farrow\_create 

_Create a Farrow interpolator._ 
```C++
dp_farrow_state_t * dp_farrow_create (
    int order
) 
```





**Parameters:**


* `order` 0 = linear, 1 = parabolic, 2 = cubic. 



**Returns:**

Heap-allocated state, or NULL on allocation failure. 




**Note:**

Caller must call [**dp\_farrow\_destroy()**](farrow__core_8h.md#function-dp_farrow_destroy) when done. 





        

<hr>



### function dp\_farrow\_delay 

_Apply a constant fractional delay of_ `mu` _samples to a CF32 block._
```C++
size_t dp_farrow_delay (
    dp_farrow_state_t * state,
    const float _Complex * x,
    size_t x_len,
    double mu,
    float _Complex * out,
    size_t max_out
) 
```



Pushes each input sample through the delay line and evaluates the interpolator at the same fixed offset, so the whole block is delayed by a constant, non-integer amount. Output sample i is the input interpolated at `i - group_delay + mu`, i.e. the stream shifted later by `group_delay - mu` samples; the first `group_delay` outputs are the delay-line filling transient and should be discarded. Because the offset is held constant this is the open-loop use of the interpolator — a timing loop instead steers `mu` per sample via [**farrow\_push()**](farrow__core_8h.md#function-farrow_push)/farrow\_eval().




**Parameters:**


* `state` Pointer to a valid [**dp\_farrow\_state\_t**](structdp__farrow__state__t.md). 
* `x` CF32 input samples. 
* `x_len` Number of input samples. 
* `mu` Fractional delay in samples; the offset in `[0,1)` into the interpolation interval (values outside extrapolate). 
* `out` Output buffer; one output per input sample. 
* `max_out` Capacity of `out` in samples. 



**Returns:**

CF32 output array, same length as `x`, each sample delayed by `group_delay - mu`.



```C++
>>> from doppler.resample import Farrow
>>> import numpy as np
>>> f = Farrow(order="cubic")
>>> x = np.arange(8, dtype=np.complex64)   # a ramp: exact interp
>>> y = f.delay(x, 0.5)                  # delay group_delay - 0.5
>>> [round(float(v.real), 4) for v in y]  # first 2 are transient
[0.0, -0.0625, 0.4375, 1.5, 2.5, 3.5, 4.5, 5.5]
```
 


        

<hr>



### function dp\_farrow\_delay\_max\_out 

```C++
size_t dp_farrow_delay_max_out (
    dp_farrow_state_t * state
) 
```




<hr>



### function dp\_farrow\_destroy 

_Destroy a Farrow interpolator._ 
```C++
void dp_farrow_destroy (
    dp_farrow_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function dp\_farrow\_get\_group\_delay 

```C++
size_t dp_farrow_get_group_delay (
    const dp_farrow_state_t * state
) 
```




<hr>



### function dp\_farrow\_get\_state 

```C++
void dp_farrow_get_state (
    const dp_farrow_state_t * state,
    void * blob
) 
```




<hr>



### function dp\_farrow\_reset 

_Clear the interpolator delay line; keep the order._ 
```C++
void dp_farrow_reset (
    dp_farrow_state_t * state
) 
```



Zeroes the 4-tap delay line so the next block starts from a filling transient again, exactly as a freshly created interpolator would. The order (linear / parabolic / cubic) is preserved, so the same object can be reused across independent bursts without rebuilding the polynomial. Call it between unrelated signal segments to stop the tail of one leaking into the head of the next.




**Parameters:**


* `state` Must be non-NULL.


```C++
>>> from doppler.resample import Farrow
>>> import numpy as np
>>> f = Farrow(order="cubic")
>>> _ = f.delay(np.ones(8, dtype=np.complex64), 0.25)  # leaves state
>>> f.reset()                                 # back to pristine
>>> x = np.arange(8, dtype=np.complex64)
>>> f.delay(x, 0.5)[3:].real.tolist()   # steady part: ramp - 1.5
[1.5, 2.5, 3.5, 4.5, 5.5]
```
 


        

<hr>



### function dp\_farrow\_set\_state 

```C++
int dp_farrow_set_state (
    dp_farrow_state_t * state,
    const void * blob
) 
```




<hr>



### function dp\_farrow\_state\_bytes 

```C++
size_t dp_farrow_state_bytes (
    const dp_farrow_state_t * state
) 
```




<hr>



### function farrow\_eval 

_Interpolate at fractional offset_ `mu` _∈_`[0,1)` _between_`d[1]` _and_`d[2]` _._
```C++
JM_FORCEINLINE  JM_HOT float _Complex farrow_eval (
    const dp_farrow_state_t * s,
    float mu
) 
```



Horner-in-µ evaluation of the order's Lagrange polynomial. µ = 0 returns `d[1]` (= input at i - 2); µ → 1 returns `d[2]`.




**Parameters:**


* `s` State. Must be non-NULL. 
* `mu` Fractional offset in `[0,1)`. 



**Returns:**

The interpolated sample. 





        

<hr>



### function farrow\_init 

_Initialise in place: set order, clear the delay line._ 
```C++
JM_FORCEINLINE void farrow_init (
    dp_farrow_state_t * s,
    int order
) 
```




<hr>



### function farrow\_push 

_Push one input sample into the delay line (oldest drops out)._ 
```C++
JM_FORCEINLINE  JM_HOT void farrow_push (
    dp_farrow_state_t * s,
    float _Complex x
) 
```




<hr>
## Macro Definition Documentation





### define FARROW\_GROUP\_DELAY 

```C++
#define FARROW_GROUP_DELAY `2u`
```




<hr>



### define FARROW\_STATE\_MAGIC 

```C++
#define FARROW_STATE_MAGIC `DP_FOURCC ('F', 'R', 'R', 'W')`
```




<hr>



### define FARROW\_STATE\_VERSION 

```C++
#define FARROW_STATE_VERSION `1u`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/farrow/farrow_core.h`

