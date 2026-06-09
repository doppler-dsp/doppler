

# File adc\_core.h



[**FileList**](files.md) **>** [**adc**](dir_a6be6b8cb61d5f2be55c0b2f94afbd88.md) **>** [**adc\_core.h**](adc__core_8h.md)

[Go to the source code of this file](adc__core_8h_source.md)

_Signed two's-complement ADC model._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "jm_perf.h"`
* `#include <math.h>`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**adc\_state\_t**](structadc__state__t.md) <br>_ADC state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**adc\_state\_t**](structadc__state__t.md) \* | [**adc\_create**](#function-adc_create) (int bits, float dbfs, int dithering) <br>_Create an ADC instance._  |
|  void | [**adc\_destroy**](#function-adc_destroy) ([**adc\_state\_t**](structadc__state__t.md) \* state) <br>_Destroy an ADC instance and release all memory._  |
|  void | [**adc\_reset**](#function-adc_reset) ([**adc\_state\_t**](structadc__state__t.md) \* state) <br>_Reset ADC to its post-create state._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) int64\_t | [**adc\_step**](#function-adc_step) ([**adc\_state\_t**](structadc__state__t.md) \* state, float x) <br>_Process one input sample._  |
|  void | [**adc\_steps**](#function-adc_steps) ([**adc\_state\_t**](structadc__state__t.md) \* state, const float \* input, int64\_t \* output, size\_t n) <br>_Process a block of float samples to int64._  |




























## Detailed Description


Models an N-bit ADC with configurable full-scale reference (dBFS) and optional TPDF dither. A normalised float input is scaled by the pre-computed double-precision factor, optionally dithered with a triangular-PDF noise source, rounded, clamped to the signed integer range [-(2^(bits-1)), 2^(bits-1)-1], and returned as int64\_t.


Scale derivation: 
```C++
scale = 2^(bits-1) * 10^(-dbfs / 20)
```



An input of amplitude 10^(dbfs/20) uses the full ADC code range. With the default dbfs=-10.0 a signal at -10 dBFS fills the converter exactly. double precision is used throughout so converters wider than 23 bits (float32 mantissa limit) are modelled without rounding artefacts.


TPDF dither (dithering != 0): two xorshift32 uniform draws are summed to produce a triangular PDF over [-1, +1] LSB before rounding. This breaks correlated quantisation noise patterns at the cost of a slight noise-floor increase. The PRNG state is part of the object; reset() re-seeds it.


Lifecycle: create -&gt; (step / steps / reset)\* -&gt; destroy



```C++
>>> from doppler.cvt import ADC
>>> import numpy as np
>>> obj = ADC(bits=8, dbfs=0.0, dithering=0)
>>> obj.scale
128.0
>>> obj.bits
8
>>> obj.step(0.0)
0
>>> obj.step(-1.0)
-128
>>> obj.clipped
False
>>> obj.step(1.0)
127
>>> obj.clipped
True
>>> obj.reset()
>>> obj.clipped
False
>>> x = np.array([-1.0, -0.5, 0.0, 0.5, 1.0], dtype=np.float32)
>>> obj.steps(x).tolist()
[-128, -64, 0, 64, 127]
```
 


    
## Public Functions Documentation




### function adc\_create 

_Create an ADC instance._ 
```C++
adc_state_t * adc_create (
    int bits,
    float dbfs,
    int dithering
) 
```



Computes `scale` = 2^(bits-1) \* 10^(-dbfs/20), sets the clip bounds, and seeds the xorshift32 PRNG to a fixed constant. Returns NULL if `bits` is outside [1, 64] or on allocation failure.




**Parameters:**


* `bits` ADC resolution in bits (1..64). 
* `dbfs` Full-scale reference level in dBFS (typically negative, e.g. -10.0). A signal with amplitude 10^(dbfs/20) fills the converter's integer range exactly. 
* `dithering` 0 = no dither; non-zero = TPDF dither before rounding. 



**Returns:**

Heap-allocated state, or NULL on invalid args or allocation failure. 




**Note:**

Caller must call [**adc\_destroy()**](adc__core_8h.md#function-adc_destroy) when done. 





        

<hr>



### function adc\_destroy 

_Destroy an ADC instance and release all memory._ 
```C++
void adc_destroy (
    adc_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function adc\_reset 

_Reset ADC to its post-create state._ 
```C++
void adc_reset (
    adc_state_t * state
) 
```



Clears the sticky `clipped` flag and re-seeds the xorshift32 PRNG to its initial value so dithered runs are reproducible after reset.




**Parameters:**


* `state` Must be non-NULL. 




        

<hr>



### function adc\_step 

_Process one input sample._ 
```C++
JM_FORCEINLINE  JM_HOT int64_t adc_step (
    adc_state_t * state,
    float x
) 
```



Multiplies `x` by the pre-computed double-precision `scale`, optionally adds TPDF dither, rounds with `llround`, and clamps to [clip\_min, clip\_max]. Sets the sticky `clipped` flag if clamping occurred.




**Parameters:**


* `state` Must be non-NULL. 
* `x` Normalised float input sample (typically in [-1, +1]). 



**Returns:**

Quantised signed integer in [-(2^(bits-1)), 2^(bits-1)-1]. 





        

<hr>



### function adc\_steps 

_Process a block of float samples to int64._ 
```C++
void adc_steps (
    adc_state_t * state,
    const float * input,
    int64_t * output,
    size_t n
) 
```



When dithering is disabled the float-to-double multiply can use SIMD widening ([**jm\_simd.h**](jm__simd_8h.md)); the int64\_t conversion and clamp remain scalar. When dithering is enabled the loop is scalar to preserve sequential PRNG state. Accepts an optional pre-allocated output array; allocates a fresh one when `output` is NULL.




**Parameters:**


* `state` Must be non-NULL. 
* `input` Input float32 array; must contain at least `n` elements. 
* `output` Output int64 array; must contain at least `n` elements. 
* `n` Number of samples to process. 




        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/adc/adc_core.h`

