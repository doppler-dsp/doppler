

# File i16\_to\_f32\_core.h



[**FileList**](files.md) **>** [**i16\_to\_f32**](dir_5ec56354373793af7b5bc8e9296f5472.md) **>** [**i16\_to\_f32\_core.h**](i16__to__f32__core_8h.md)

[Go to the source code of this file](i16__to__f32__core_8h_source.md)

_int16-to-float converter with configurable inverse scale._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "jm_perf.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**i16\_to\_f32\_state\_t**](structi16__to__f32__state__t.md) <br>_I16ToF32 state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**i16\_to\_f32\_state\_t**](structi16__to__f32__state__t.md) \* | [**i16\_to\_f32\_create**](#function-i16_to_f32_create) (float scale) <br>_Create a i16\_to\_f32 instance._  |
|  void | [**i16\_to\_f32\_destroy**](#function-i16_to_f32_destroy) ([**i16\_to\_f32\_state\_t**](structi16__to__f32__state__t.md) \* state) <br>_Destroy a i16\_to\_f32 instance and release all memory._  |
|  void | [**i16\_to\_f32\_reset**](#function-i16_to_f32_reset) ([**i16\_to\_f32\_state\_t**](structi16__to__f32__state__t.md) \* state) <br>_No-op reset, provided only for lifecycle symmetry._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) float | [**i16\_to\_f32\_step**](#function-i16_to_f32_step) (const [**i16\_to\_f32\_state\_t**](structi16__to__f32__state__t.md) \* state, int16\_t x) <br>_Convert one signed int16 sample to a normalised float via_ `1/scale` _._ |
|  void | [**i16\_to\_f32\_steps**](#function-i16_to_f32_steps) ([**i16\_to\_f32\_state\_t**](structi16__to__f32__state__t.md) \* state, const int16\_t \* input, float \* output, size\_t n) <br>_Process a block of int16 samples to float32._  |




























## Detailed Description


Multiplies the signed int16 sample by `1/scale`. The default scale of 32768.0 maps the full Q15 range `[-32768, 32767]` to `[-1.0, ~+1.0)`, making it the exact inverse of F32ToI16 at its default scale. The inverse scale is pre-computed at construction time so each step is a single multiply with no division on the hot path.


Lifecycle: create -&gt; (step / steps / reset)\* -&gt; destroy



```C++
>>> from doppler.cvt import I16ToF32
>>> import numpy as np
>>> obj = I16ToF32(scale=32768.0)
>>> float(obj.step(-32768))
-1.0
>>> float(obj.step(0))
0.0
>>> x = np.array([-32768, 0, 32767], dtype=np.int16)
>>> [round(v, 6) for v in obj.steps(x).tolist()]
[-1.0, 0.0, 0.999969]
```
 


    
## Public Functions Documentation




### function i16\_to\_f32\_create 

_Create a i16\_to\_f32 instance._ 
```C++
i16_to_f32_state_t * i16_to_f32_create (
    float scale
) 
```



Pre-computes `iscale` = 1.0f / `scale` so the hot step path is a single multiply. Any non-zero finite float is a valid scale value.




**Parameters:**


* `scale` Denominator scale; 1/scale is applied to each sample (default: 32768.0f). Use 32768.0 to recover normalised `[-1, +1]` floats from a Q15 int16 stream. 



**Returns:**

Heap-allocated state, or NULL on allocation failure. 




**Note:**

Caller must call [**i16\_to\_f32\_destroy()**](i16__to__f32__core_8h.md#function-i16_to_f32_destroy) when done. 





        

<hr>



### function i16\_to\_f32\_destroy 

_Destroy a i16\_to\_f32 instance and release all memory._ 
```C++
void i16_to_f32_destroy (
    i16_to_f32_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function i16\_to\_f32\_reset 

_No-op reset, provided only for lifecycle symmetry._ 
```C++
void i16_to_f32_reset (
    i16_to_f32_state_t * state
) 
```



This converter carries no running state beyond the immutable `iscale`, so there is nothing to clear; the method exists so every converter in the module presents the same create / step / reset / destroy lifecycle.




**Parameters:**


* `state` Must be non-NULL.


```C++
>>> from doppler.cvt import I16ToF32
>>> c = I16ToF32()
>>> c.reset()           # stateless converter -> reset is a no-op
>>> round(c.step(-32768), 4)
-1.0
```
 


        

<hr>



### function i16\_to\_f32\_step 

_Convert one signed int16 sample to a normalised float via_ `1/scale` _._
```C++
JM_FORCEINLINE  JM_HOT float i16_to_f32_step (
    const i16_to_f32_state_t * state,
    int16_t x
) 
```



Returns ``(float)x \* iscale, a single multiply on the hot path. No saturation or clipping is possible — every int16 code maps cleanly to float32. At the default scale of 32768 the full Q15 range recovers `[-1.0, ~+1.0)`, the exact inverse of F32ToI16.




**Parameters:**


* `state` Must be non-NULL. 
* `x` Signed int16 code, normally a Q15 sample in `[-32768, 32767]`. 



**Returns:**

Normalised float, `x / scale`.



```C++
>>> from doppler.cvt import I16ToF32
>>> c = I16ToF32(scale=32768.0)   # Q15 int16 -> normalised float
>>> round(c.step(16384), 4)        # 16384 / 32768
0.5
>>> round(c.step(-32768), 4)       # full-negative code -> -1.0
-1.0
```
 


        

<hr>



### function i16\_to\_f32\_steps 

_Process a block of int16 samples to float32._ 
```C++
void i16_to_f32_steps (
    i16_to_f32_state_t * state,
    const int16_t * input,
    float * output,
    size_t n
) 
```



Applies step() to every element. Accepts an optional pre-allocated output array; allocates a fresh one when `output` is NULL.




**Parameters:**


* `state` Must be non-NULL. 
* `input` Input int16 array; must contain at least `n` elements. 
* `output` Output float32 array; must contain at least `n` elements. 
* `n` Number of samples to process.


```C++
>>> from doppler.cvt import I16ToF32
>>> import numpy as np
>>> I16ToF32().steps(
...     np.array([0, 16384, -32768], dtype=np.int16)).tolist()
[0.0, 0.5, -1.0]
```
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/i16_to_f32/i16_to_f32_core.h`

