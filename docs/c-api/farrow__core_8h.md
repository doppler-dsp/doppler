

# File farrow\_core.h



[**FileList**](files.md) **>** [**farrow**](dir_3474bb67440308cdab2155867b5160e7.md) **>** [**farrow\_core.h**](farrow__core_8h.md)

[Go to the source code of this file](farrow__core_8h_source.md)

_Farrow fractional-delay interpolator — linear / parabolic / cubic._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "jm_perf.h"`
* `#include "dp_state.h"`
* `#include <complex.h>`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**farrow\_state\_t**](structfarrow__state__t.md) <br>_Farrow interpolator state (4-tap delay line + order)._  |


## Public Types

| Type | Name |
| ---: | :--- |
| enum  | [**farrow\_\_core\_8h\_1afc4a4c5c83f7c538008107b97d30abcd**](#enum-farrow__core_8h_1afc4a4c5c83f7c538008107b97d30abcd)  <br> |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**farrow\_state\_t**](structfarrow__state__t.md) \* | [**farrow\_create**](#function-farrow_create) (int order) <br>_Create a Farrow interpolator._  |
|  size\_t | [**farrow\_delay**](#function-farrow_delay) ([**farrow\_state\_t**](structfarrow__state__t.md) \* state, const float complex \* x, size\_t x\_len, double mu, float complex \* out, size\_t max\_out) <br> |
|  size\_t | [**farrow\_delay\_max\_out**](#function-farrow_delay_max_out) ([**farrow\_state\_t**](structfarrow__state__t.md) \* state) <br> |
|  void | [**farrow\_destroy**](#function-farrow_destroy) ([**farrow\_state\_t**](structfarrow__state__t.md) \* state) <br>_Destroy a Farrow interpolator._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) float complex | [**farrow\_eval**](#function-farrow_eval) (const [**farrow\_state\_t**](structfarrow__state__t.md) \* s, float mu) <br>_Interpolate at fractional offset_ `mu` _∈_`[0,1)` _between_`d[1]` _and_`d[2]` _._ |
|  size\_t | [**farrow\_get\_group\_delay**](#function-farrow_get_group_delay) (const [**farrow\_state\_t**](structfarrow__state__t.md) \* state) <br> |
|  void | [**farrow\_get\_state**](#function-farrow_get_state) (const [**farrow\_state\_t**](structfarrow__state__t.md) \* state, void \* blob) <br> |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) void | [**farrow\_init**](#function-farrow_init) ([**farrow\_state\_t**](structfarrow__state__t.md) \* s, int order) <br>_Initialise in place: set order, clear the delay line._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) void | [**farrow\_push**](#function-farrow_push) ([**farrow\_state\_t**](structfarrow__state__t.md) \* s, float complex x) <br>_Push one input sample into the delay line (oldest drops out)._  |
|  void | [**farrow\_reset**](#function-farrow_reset) ([**farrow\_state\_t**](structfarrow__state__t.md) \* state) <br>_Clear the delay line; keep the order._  |
|  int | [**farrow\_set\_state**](#function-farrow_set_state) ([**farrow\_state\_t**](structfarrow__state__t.md) \* state, const void \* blob) <br> |
|  size\_t | [**farrow\_state\_bytes**](#function-farrow_state_bytes) (const [**farrow\_state\_t**](structfarrow__state__t.md) \* state) <br> |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**FARROW\_GROUP\_DELAY**](farrow__core_8h.md#define-farrow_group_delay)  `2u`<br> |
| define  | [**FARROW\_STATE\_MAGIC**](farrow__core_8h.md#define-farrow_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('F', 'R', 'R', 'W')`<br> |
| define  | [**FARROW\_STATE\_VERSION**](farrow__core_8h.md#define-farrow_state_version)  `1u`<br> |

## Detailed Description


A selectable-order Lagrange interpolator in Farrow (Horner-in-µ) form — the lean alternative to a full polyphase resampler when all you need is a fractional-delay tap for a timing loop. All three orders share one 4-tap delay line and interpolate at the SAME point — between the two middle taps — so the **group delay is 2 samples regardless of order**, which keeps a driving symbol-timing loop order-agnostic. Push input samples with [**farrow\_push()**](farrow__core_8h.md#function-farrow_push); evaluate the output at a fractional offset µ ∈ `[0,1)` with [**farrow\_eval()**](farrow__core_8h.md#function-farrow_eval). The fractional offset is meant to come from an integer timing NCO (the post-wrap accumulator value), so the timing stays drift-free while only the interpolation itself is floating point.


order: 0 = linear (2-tap Lagrange), 1 = parabolic (4-tap symmetric piecewise-parabolic Farrow, α = 0.5), 2 = cubic (4-tap cubic Lagrange). All three are symmetric about the interpolation point, so the phase (delay) response is linear — no timing bias. Linear and cubic are exact for degree 1 and 3 polynomials; the piecewise-parabolic trades exactness for a flatter magnitude response than linear at no delay cost.


Lifecycle: farrow\_create -&gt; (push / eval / reset)\* -&gt; farrow\_destroy, or embed by value with [**farrow\_init()**](farrow__core_8h.md#function-farrow_init).



```C++
farrow_state_t f;
farrow_init(&f, FARROW_CUBIC);
for (size_t i = 0; i < n; i++) farrow_push(&f, x[i]);
float complex y = farrow_eval(&f, 0.3f);   // x interpolated 0.3 past tap[1]
```
 


    
## Public Types Documentation




### enum farrow\_\_core\_8h\_1afc4a4c5c83f7c538008107b97d30abcd 

```C++
enum farrow__core_8h_1afc4a4c5c83f7c538008107b97d30abcd {
    FARROW_LINEAR = 0,
    FARROW_PARABOLIC = 1,
    FARROW_CUBIC = 2
};
```




<hr>
## Public Functions Documentation




### function farrow\_create 

_Create a Farrow interpolator._ 
```C++
farrow_state_t * farrow_create (
    int order
) 
```





**Parameters:**


* `order` 0 = linear, 1 = parabolic, 2 = cubic. 



**Returns:**

Heap-allocated state, or NULL on allocation failure. 




**Note:**

Caller must call [**farrow\_destroy()**](farrow__core_8h.md#function-farrow_destroy) when done. 





        

<hr>



### function farrow\_delay 

```C++
size_t farrow_delay (
    farrow_state_t * state,
    const float complex * x,
    size_t x_len,
    double mu,
    float complex * out,
    size_t max_out
) 
```




<hr>



### function farrow\_delay\_max\_out 

```C++
size_t farrow_delay_max_out (
    farrow_state_t * state
) 
```




<hr>



### function farrow\_destroy 

_Destroy a Farrow interpolator._ 
```C++
void farrow_destroy (
    farrow_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function farrow\_eval 

_Interpolate at fractional offset_ `mu` _∈_`[0,1)` _between_`d[1]` _and_`d[2]` _._
```C++
JM_FORCEINLINE  JM_HOT float complex farrow_eval (
    const farrow_state_t * s,
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



### function farrow\_get\_group\_delay 

```C++
size_t farrow_get_group_delay (
    const farrow_state_t * state
) 
```




<hr>



### function farrow\_get\_state 

```C++
void farrow_get_state (
    const farrow_state_t * state,
    void * blob
) 
```




<hr>



### function farrow\_init 

_Initialise in place: set order, clear the delay line._ 
```C++
JM_FORCEINLINE void farrow_init (
    farrow_state_t * s,
    int order
) 
```




<hr>



### function farrow\_push 

_Push one input sample into the delay line (oldest drops out)._ 
```C++
JM_FORCEINLINE  JM_HOT void farrow_push (
    farrow_state_t * s,
    float complex x
) 
```




<hr>



### function farrow\_reset 

_Clear the delay line; keep the order._ 
```C++
void farrow_reset (
    farrow_state_t * state
) 
```




<hr>



### function farrow\_set\_state 

```C++
int farrow_set_state (
    farrow_state_t * state,
    const void * blob
) 
```




<hr>



### function farrow\_state\_bytes 

```C++
size_t farrow_state_bytes (
    const farrow_state_t * state
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
The documentation for this class was generated from the following file `native/inc/farrow/farrow_core.h`

