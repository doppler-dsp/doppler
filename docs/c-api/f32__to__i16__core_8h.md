

# File f32\_to\_i16\_core.h



[**FileList**](files.md) **>** [**f32\_to\_i16**](dir_e25c96329f88166d8f87eefdc2ba64fa.md) **>** [**f32\_to\_i16\_core.h**](f32__to__i16__core_8h.md)

[Go to the source code of this file](f32__to__i16__core_8h_source.md)

_Scale-and-saturate float-to-int16 converter._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "dp_state.h"`
* `#include "jm_perf.h"`
* `#include <math.h>`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**f32\_to\_i16\_state\_t**](structf32__to__i16__state__t.md) <br>_F32ToI16 state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**f32\_to\_i16\_state\_t**](structf32__to__i16__state__t.md) \* | [**f32\_to\_i16\_create**](#function-f32_to_i16_create) (float scale) <br>_Create a f32\_to\_i16 instance._  |
|  void | [**f32\_to\_i16\_destroy**](#function-f32_to_i16_destroy) ([**f32\_to\_i16\_state\_t**](structf32__to__i16__state__t.md) \* state) <br>_Destroy a f32\_to\_i16 instance and release all memory._  |
|  void | [**f32\_to\_i16\_get\_state**](#function-f32_to_i16_get_state) (const [**f32\_to\_i16\_state\_t**](structf32__to__i16__state__t.md) \* state, void \* blob) <br> |
|  void | [**f32\_to\_i16\_reset**](#function-f32_to_i16_reset) ([**f32\_to\_i16\_state\_t**](structf32__to__i16__state__t.md) \* state) <br>_Clear the sticky clip flag, starting a fresh saturation history._  |
|  int | [**f32\_to\_i16\_set\_state**](#function-f32_to_i16_set_state) ([**f32\_to\_i16\_state\_t**](structf32__to__i16__state__t.md) \* state, const void \* blob) <br> |
|  size\_t | [**f32\_to\_i16\_state\_bytes**](#function-f32_to_i16_state_bytes) (const [**f32\_to\_i16\_state\_t**](structf32__to__i16__state__t.md) \* state) <br> |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) int16\_t | [**f32\_to\_i16\_step**](#function-f32_to_i16_step) ([**f32\_to\_i16\_state\_t**](structf32__to__i16__state__t.md) \* state, float x) <br>_Scale one float sample by_ `scale` _, round, and saturate to int16._ |
|  void | [**f32\_to\_i16\_steps**](#function-f32_to_i16_steps) ([**f32\_to\_i16\_state\_t**](structf32__to__i16__state__t.md) \* state, const float \* input, int16\_t \* output, size\_t n) <br>_Process a block of float samples to int16._  |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**F32\_TO\_I16\_STATE\_MAGIC**](f32__to__i16__core_8h.md#define-f32_to_i16_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('F','2','1','6')`<br> |
| define  | [**F32\_TO\_I16\_STATE\_VERSION**](f32__to__i16__core_8h.md#define-f32_to_i16_state_version)  `1u`<br> |

## Detailed Description


Multiplies the input by `scale`, rounds to the nearest integer, and saturates (clamps) the result to the int16 range `[-32768, 32767]`. The default scale of 32768.0 maps a normalised `[-1, +1]` float to the full Q15 integer range, making it the natural pair for I16ToF32. A sticky `clipped` flag is raised on any sample that saturates and is cleared only by reset().


Lifecycle: create -&gt; (step / steps / reset)\* -&gt; destroy



```C++
>>> from doppler.cvt import F32ToI16
>>> import numpy as np
>>> obj = F32ToI16(scale=32768.0)
>>> obj.step(0.5)
16384
>>> obj.step(-0.5)
-16384
>>> obj.clipped
False
>>> obj.step(1.0)
32767
>>> obj.clipped
True
>>> obj.reset()
>>> obj.clipped
False
>>> x = np.array([0.5, -0.5, 1.0], dtype=np.float32)
>>> obj.steps(x).tolist()
[16384, -16384, 32767]
```
 


    
## Public Functions Documentation




### function f32\_to\_i16\_create 

_Create a f32\_to\_i16 instance._ 
```C++
f32_to_i16_state_t * f32_to_i16_create (
    float scale
) 
```



Allocates state and stores `scale`. The `clipped` flag is initialised to 0. Returns NULL only on malloc failure; no parameter validation is performed (any finite float is a valid scale).




**Parameters:**


* `scale` Multiply factor applied before rounding and saturation (default: 32768.0f). Use 32768.0 to convert a normalised `[-1, +1]` signal to full Q15 range. 



**Returns:**

Heap-allocated state, or NULL on allocation failure. 




**Note:**

Caller must call [**f32\_to\_i16\_destroy()**](f32__to__i16__core_8h.md#function-f32_to_i16_destroy) when done. 





        

<hr>



### function f32\_to\_i16\_destroy 

_Destroy a f32\_to\_i16 instance and release all memory._ 
```C++
void f32_to_i16_destroy (
    f32_to_i16_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function f32\_to\_i16\_get\_state 

```C++
void f32_to_i16_get_state (
    const f32_to_i16_state_t * state,
    void * blob
) 
```




<hr>



### function f32\_to\_i16\_reset 

_Clear the sticky clip flag, starting a fresh saturation history._ 
```C++
void f32_to_i16_reset (
    f32_to_i16_state_t * state
) 
```



Zeroes `clipped` so a subsequent clipped query reflects only samples seen after this call; the immutable `scale` is preserved. Call it at a buffer or segment boundary so a saturation on one block does not leak into the next.




**Parameters:**


* `state` Must be non-NULL.


```C++
>>> from doppler.cvt import F32ToI16
>>> c = F32ToI16()
>>> c.step(9.0)          # out of range -> saturates, latches clipped
32767
>>> c.reset()            # forget the clip history
>>> c.clipped
False
```
 


        

<hr>



### function f32\_to\_i16\_set\_state 

```C++
int f32_to_i16_set_state (
    f32_to_i16_state_t * state,
    const void * blob
) 
```




<hr>



### function f32\_to\_i16\_state\_bytes 

```C++
size_t f32_to_i16_state_bytes (
    const f32_to_i16_state_t * state
) 
```




<hr>



### function f32\_to\_i16\_step 

_Scale one float sample by_ `scale` _, round, and saturate to int16._
```C++
JM_FORCEINLINE  JM_HOT int16_t f32_to_i16_step (
    f32_to_i16_state_t * state,
    float x
) 
```



Computes `round(x * scale)`, clamps to the int16 range `[-32768, 32767]`, and latches the sticky `clipped` flag if the scaled value fell outside that range before clamping. At the default scale of 32768 a normalised `[-1, +1]` input maps to the full Q15 code range.




**Parameters:**


* `state` Must be non-NULL. 
* `x` Input sample, normally a normalised float in `[-1, +1]`. 



**Returns:**

Saturated int16 code in `[-32768, 32767]`.



```C++
>>> from doppler.cvt import F32ToI16
>>> c = F32ToI16(scale=32768.0)   # normalised float -> full-scale Q15
>>> c.step(0.5)                    # 0.5 * 32768
16384
>>> c.step(2.0)                 # beyond +1.0 -> saturates to max
32767
>>> c.clipped                      # sticky flag latched by the clip
True
```
 


        

<hr>



### function f32\_to\_i16\_steps 

_Process a block of float samples to int16._ 
```C++
void f32_to_i16_steps (
    f32_to_i16_state_t * state,
    const float * input,
    int16_t * output,
    size_t n
) 
```



Applies step() to every element. The `clipped` flag is updated cumulatively across the block — a single saturating sample raises it for the entire call. Accepts an optional pre-allocated output array; allocates a fresh one when `output` is NULL.




**Parameters:**


* `state` Must be non-NULL. 
* `input` Input float32 array; must contain at least `n` elements. 
* `output` Output int16 array; must contain at least `n` elements. 
* `n` Number of samples to process.


```C++
>>> from doppler.cvt import F32ToI16
>>> import numpy as np
>>> x = np.array([0.0, 0.5, -1.0, 0.999], dtype=np.float32)
>>> F32ToI16().steps(x).tolist()   # scale=32768 -> full-scale i16
[0, 16384, -32768, 32735]
```
 


        

<hr>
## Macro Definition Documentation





### define F32\_TO\_I16\_STATE\_MAGIC 

```C++
#define F32_TO_I16_STATE_MAGIC `DP_FOURCC ('F','2','1','6')`
```




<hr>



### define F32\_TO\_I16\_STATE\_VERSION 

```C++
#define F32_TO_I16_STATE_VERSION `1u`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/f32_to_i16/f32_to_i16_core.h`

