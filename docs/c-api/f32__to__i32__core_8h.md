

# File f32\_to\_i32\_core.h



[**FileList**](files.md) **>** [**f32\_to\_i32**](dir_9f277c348fdc2d73ff85df72003f099b.md) **>** [**f32\_to\_i32\_core.h**](f32__to__i32__core_8h.md)

[Go to the source code of this file](f32__to__i32__core_8h_source.md)

_Scale-and-saturate float-to-int32 converter._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "dp_state.h"`
* `#include "jm_perf.h"`
* `#include <math.h>`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**f32\_to\_i32\_state\_t**](structf32__to__i32__state__t.md) <br>_F32ToI32 state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**f32\_to\_i32\_state\_t**](structf32__to__i32__state__t.md) \* | [**f32\_to\_i32\_create**](#function-f32_to_i32_create) (float scale) <br>_Create a f32\_to\_i32 instance._  |
|  void | [**f32\_to\_i32\_destroy**](#function-f32_to_i32_destroy) ([**f32\_to\_i32\_state\_t**](structf32__to__i32__state__t.md) \* state) <br>_Destroy a f32\_to\_i32 instance and release all memory._  |
|  void | [**f32\_to\_i32\_get\_state**](#function-f32_to_i32_get_state) (const [**f32\_to\_i32\_state\_t**](structf32__to__i32__state__t.md) \* state, void \* blob) <br> |
|  void | [**f32\_to\_i32\_reset**](#function-f32_to_i32_reset) ([**f32\_to\_i32\_state\_t**](structf32__to__i32__state__t.md) \* state) <br>_Clear the sticky clip flag, starting a fresh saturation history._  |
|  int | [**f32\_to\_i32\_set\_state**](#function-f32_to_i32_set_state) ([**f32\_to\_i32\_state\_t**](structf32__to__i32__state__t.md) \* state, const void \* blob) <br> |
|  size\_t | [**f32\_to\_i32\_state\_bytes**](#function-f32_to_i32_state_bytes) (const [**f32\_to\_i32\_state\_t**](structf32__to__i32__state__t.md) \* state) <br> |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) int32\_t | [**f32\_to\_i32\_step**](#function-f32_to_i32_step) ([**f32\_to\_i32\_state\_t**](structf32__to__i32__state__t.md) \* state, float x) <br>_Scale one float sample by_ `scale` _, round, and saturate to int32._ |
|  void | [**f32\_to\_i32\_steps**](#function-f32_to_i32_steps) ([**f32\_to\_i32\_state\_t**](structf32__to__i32__state__t.md) \* state, const float \* input, int32\_t \* output, size\_t n) <br>_Process a block of float samples to int32._  |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**F32\_TO\_I32\_STATE\_MAGIC**](f32__to__i32__core_8h.md#define-f32_to_i32_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('F','2','3','2')`<br> |
| define  | [**F32\_TO\_I32\_STATE\_VERSION**](f32__to__i32__core_8h.md#define-f32_to_i32_state_version)  `1u`<br> |

## Detailed Description


Multiplies the input by `scale`, rounds to the nearest integer, and saturates (clamps) the result to the int32 range `[-2147483648, 2147483647]`. The default scale of 2147483648.0 (2^31) maps a normalised `[-1, +1]` float to the full 32-bit integer range, making it the exact counterpart of I32ToF32. A sticky `clipped` flag is raised on any sample that saturates and is cleared only by reset().


The full scale is 2^31, not 2^31-1: the code grid a converter actually has is 2^32 equally spaced steps of 1/2^31, so scaling by 2^31 maps the normalised range onto that grid exactly and a dyadic input round-trips with no error. An input of exactly +1.0 lands on 2^31, one past INT32\_MAX by construction, and saturating it is what this mapping means rather than a failure of it.


Note the source is a float32, so only 24 significant bits survive the multiply — the low bits of a full-scale code are not meaningful, and two inputs a float apart can produce codes hundreds apart. That is a property of the input type, not of this conversion; I32ToF32 documents the mirror.


Lifecycle: create -&gt; (step / steps / reset)\* -&gt; destroy



```C++
>>> from doppler.cvt import F32ToI32
>>> import numpy as np
>>> obj = F32ToI32(scale=2147483648.0)
>>> obj.step(0.5)
1073741824
>>> obj.step(-1.0)
-2147483648
>>> obj.clipped
False
>>> obj.step(1.0)
2147483647
>>> obj.clipped
True
>>> obj.reset()
>>> obj.clipped
False
>>> x = np.array([0.0, 0.25, -0.5], dtype=np.float32)
>>> obj.steps(x).tolist()
[0, 536870912, -1073741824]
```
 


    
## Public Functions Documentation




### function f32\_to\_i32\_create 

_Create a f32\_to\_i32 instance._ 
```C++
f32_to_i32_state_t * f32_to_i32_create (
    float scale
) 
```



Allocates state and stores `scale`. The `clipped` flag is initialised to 0. Returns NULL for a non-positive scale, which is the only failure a caller can cause; the allocation itself aborts on exhaustion (dp\_xcalloc) rather than handing back an unwind path no test can reach.




**Parameters:**


* `scale` Multiply factor applied before rounding and saturation (default: 2147483648.0f). Use 2^31 to convert a normalised `[-1, +1]` signal to the full 32-bit range. 



**Returns:**

Heap-allocated state, or NULL if `scale` is not positive. 




**Note:**

Caller must call [**f32\_to\_i32\_destroy()**](f32__to__i32__core_8h.md#function-f32_to_i32_destroy) when done. 





        

<hr>



### function f32\_to\_i32\_destroy 

_Destroy a f32\_to\_i32 instance and release all memory._ 
```C++
void f32_to_i32_destroy (
    f32_to_i32_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function f32\_to\_i32\_get\_state 

```C++
void f32_to_i32_get_state (
    const f32_to_i32_state_t * state,
    void * blob
) 
```




<hr>



### function f32\_to\_i32\_reset 

_Clear the sticky clip flag, starting a fresh saturation history._ 
```C++
void f32_to_i32_reset (
    f32_to_i32_state_t * state
) 
```



Zeroes `clipped` so a subsequent clipped query reflects only samples seen after this call; the immutable `scale` is preserved. Call it at a buffer or segment boundary so a saturation on one block does not leak into the next.




**Parameters:**


* `state` Must be non-NULL.


```C++
>>> from doppler.cvt import F32ToI32
>>> c = F32ToI32()
>>> c.step(9.0)          # out of range -> saturates, latches clipped
2147483647
>>> c.reset()            # forget the clip history
>>> c.clipped
False
```
 


        

<hr>



### function f32\_to\_i32\_set\_state 

```C++
int f32_to_i32_set_state (
    f32_to_i32_state_t * state,
    const void * blob
) 
```




<hr>



### function f32\_to\_i32\_state\_bytes 

```C++
size_t f32_to_i32_state_bytes (
    const f32_to_i32_state_t * state
) 
```




<hr>



### function f32\_to\_i32\_step 

_Scale one float sample by_ `scale` _, round, and saturate to int32._
```C++
JM_FORCEINLINE  JM_HOT int32_t f32_to_i32_step (
    f32_to_i32_state_t * state,
    float x
) 
```



Computes `round(x * scale)`, clamps to the int32 range `[-2147483648, 2147483647]`, and latches the sticky `clipped` flag if the scaled value fell outside that range before clamping. At the default scale of 2^31 a normalised `[-1, +1]` input maps to the full 32-bit code range.


Unlike its int8 and int16 siblings this works in `double` throughout. INT32\_MAX is not representable as a float — 2147483647.0f rounds UP to 2^31 — so a float `fminf(s, 2147483647.0f)` clamps to a value one past the range it is trying to enforce, and the following lround() overflows. The float32 input still bounds the useful precision at 24 bits; the double is there to make the CLAMP exact, not to invent significance.




**Parameters:**


* `state` Must be non-NULL. 
* `x` Input sample, normally a normalised float in `[-1, +1]`. 



**Returns:**

Saturated int32 code in `[-2147483648, 2147483647]`.



```C++
>>> from doppler.cvt import F32ToI32
>>> c = F32ToI32(scale=2147483648.0)  # normalised float -> full-scale
>>> c.step(0.5)                       # 0.5 * 2**31
1073741824
>>> c.step(2.0)                       # beyond +1.0 -> saturates to max
2147483647
>>> c.clipped                         # sticky flag latched by the clip
True
```
 


        

<hr>



### function f32\_to\_i32\_steps 

_Process a block of float samples to int32._ 
```C++
void f32_to_i32_steps (
    f32_to_i32_state_t * state,
    const float * input,
    int32_t * output,
    size_t n
) 
```



Applies step() to every element. The `clipped` flag is updated cumulatively across the block — a single saturating sample raises it for the entire call. Accepts an optional pre-allocated output array; allocates a fresh one when `output` is NULL.




**Parameters:**


* `state` Must be non-NULL. 
* `input` Input float32 array; must contain at least `n` elements. 
* `output` Output int32 array; must contain at least `n` elements. 
* `n` Number of samples to process.


```C++
>>> from doppler.cvt import F32ToI32
>>> import numpy as np
>>> x = np.array([0.0, 0.25, -1.0], dtype=np.float32)
>>> F32ToI32().steps(x).tolist()   # scale=2**31 -> full-scale int32
[0, 536870912, -2147483648]
```
 


        

<hr>
## Macro Definition Documentation





### define F32\_TO\_I32\_STATE\_MAGIC 

```C++
#define F32_TO_I32_STATE_MAGIC `DP_FOURCC ('F','2','3','2')`
```




<hr>



### define F32\_TO\_I32\_STATE\_VERSION 

```C++
#define F32_TO_I32_STATE_VERSION `1u`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/f32_to_i32/f32_to_i32_core.h`

