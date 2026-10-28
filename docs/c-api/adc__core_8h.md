

# File adc\_core.h



[**FileList**](files.md) **>** [**adc**](dir_eddcd1edcea89729b407f545f79f2d08.md) **>** [**adc\_core.h**](adc__core_8h.md)

[Go to the source code of this file](adc__core_8h_source.md)

_Signed two's-complement ADC model._ [More...](#detailed-description)

* `#include "doppler/clib_common.h"`
* `#include "doppler/dp_state.h"`
* `#include "doppler/jm_perf.h"`
* `#include <math.h>`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**dp\_adc\_state\_t**](structdp__adc__state__t.md) <br>_ADC state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**dp\_adc\_state\_t**](structdp__adc__state__t.md) \* | [**dp\_adc\_create**](#function-dp_adc_create) (int bits, float dbfs, int dithering) <br>_Create an ADC instance._  |
|  void | [**dp\_adc\_destroy**](#function-dp_adc_destroy) ([**dp\_adc\_state\_t**](structdp__adc__state__t.md) \* state) <br>_Destroy an ADC instance and release all memory._  |
|  void | [**dp\_adc\_get\_state**](#function-dp_adc_get_state) (const [**dp\_adc\_state\_t**](structdp__adc__state__t.md) \* state, void \* blob) <br>_Serialize the full state (config, clip flag, dither PRNG)._  |
|  void | [**dp\_adc\_reset**](#function-dp_adc_reset) ([**dp\_adc\_state\_t**](structdp__adc__state__t.md) \* state) <br>_Clear the clip flag and re-seed the dither PRNG for a reproducible run._  |
|  int | [**dp\_adc\_set\_state**](#function-dp_adc_set_state) ([**dp\_adc\_state\_t**](structdp__adc__state__t.md) \* state, const void \* blob) <br>_Restore a state blob written by_ [_**dp\_adc\_get\_state()**_](adc__core_8h.md#function-dp_adc_get_state) _._ |
|  size\_t | [**dp\_adc\_state\_bytes**](#function-dp_adc_state_bytes) (const [**dp\_adc\_state\_t**](structdp__adc__state__t.md) \* state) <br>_Size in bytes of this instance's serialized state blob._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) int64\_t | [**dp\_adc\_step**](#function-dp_adc_step) ([**dp\_adc\_state\_t**](structdp__adc__state__t.md) \* state, float x) <br>_Quantise one float sample to a signed N-bit ADC code._  |
|  void | [**dp\_adc\_steps**](#function-dp_adc_steps) ([**dp\_adc\_state\_t**](structdp__adc__state__t.md) \* state, const float \* input, int64\_t \* output, size\_t n) <br>_Process a block of float samples to int64._  |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**ADC\_STATE\_MAGIC**](adc__core_8h.md#define-adc_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('A','D','C',' ')`<br> |
| define  | [**ADC\_STATE\_VERSION**](adc__core_8h.md#define-adc_state_version)  `1u`<br> |

## Detailed Description


Models an N-bit ADC with configurable full-scale reference (dBFS) and optional TPDF dither. A normalised float input is scaled by the pre-computed double-precision factor, optionally dithered with a triangular-PDF noise source, rounded, clamped to the signed integer range `[-(2^(bits-1)), 2^(bits-1)-1]`, and returned as int64\_t.


Scale derivation: 
```C++
scale = 2^(bits-1) * 10^(-dbfs / 20)
```



An input of amplitude 10^(dbfs/20) uses the full ADC code range. With the default dbfs=-10.0 a signal at -10 dBFS fills the converter exactly. step() works in double precision, so converters wider than 23 bits (float32 mantissa limit) are modelled without rounding artefacts. The undithered steps() fast path multiplies in float32 instead (see [**dp\_adc\_steps()**](adc__core_8h.md#function-dp_adc_steps)).


TPDF dither (dithering != 0): two xorshift32 uniform draws are summed to produce a triangular PDF over `[-1, +1]` LSB before rounding. This breaks correlated quantisation noise patterns at the cost of a slight noise-floor increase. The PRNG state is part of the object; reset() re-seeds it.


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




### function dp\_adc\_create 

_Create an ADC instance._ 
```C++
dp_adc_state_t * dp_adc_create (
    int bits,
    float dbfs,
    int dithering
) 
```



Computes `scale` = 2^(bits-1) \* 10^(-dbfs/20), sets the clip bounds, and seeds the xorshift32 PRNG to a fixed constant. Returns NULL if `bits` is outside `[1, 64]`, if `dithering` is negative, or on allocation failure.




**Parameters:**


* `bits` ADC resolution in bits (1..64; default 16). 
* `dbfs` Full-scale reference level in dBFS (typically negative, e.g. -10.0). A signal with amplitude 10^(dbfs/20) fills the converter's integer range exactly (default -10.0). 
* `dithering` 0 = no dither (default); positive = TPDF dither before rounding. A negative value is rejected (NULL). 



**Returns:**

Heap-allocated state, or NULL on invalid args or allocation failure. 




**Note:**

Caller must call [**dp\_adc\_destroy()**](adc__core_8h.md#function-dp_adc_destroy) when done. 





        

<hr>



### function dp\_adc\_destroy 

_Destroy an ADC instance and release all memory._ 
```C++
void dp_adc_destroy (
    dp_adc_state_t * state
) 
```





**Parameters:**


* `state` Instance from [**dp\_adc\_create()**](adc__core_8h.md#function-dp_adc_create); may be NULL (no-op). 




        

<hr>



### function dp\_adc\_get\_state 

_Serialize the full state (config, clip flag, dither PRNG)._ 
```C++
void dp_adc_get_state (
    const dp_adc_state_t * state,
    void * blob
) 
```



Writes the envelope then a byte copy of the struct, so an identically built instance restored from it continues bit-for-bit, dither included.




**Parameters:**


* `state` Instance to snapshot; non-NULL. 
* `blob` Destination of at least [**dp\_adc\_state\_bytes()**](adc__core_8h.md#function-dp_adc_state_bytes) bytes. 




        

<hr>



### function dp\_adc\_reset 

_Clear the clip flag and re-seed the dither PRNG for a reproducible run._ 
```C++
void dp_adc_reset (
    dp_adc_state_t * state
) 
```



Zeroes the sticky `clipped` flag and re-seeds the xorshift32 dither PRNG to its fixed initial value, so a dithered capture restarted after reset() is bit-for-bit reproducible. The immutable configuration (bits, scale, clip bounds) is preserved.




**Parameters:**


* `state` Instance to reset; must be non-NULL.


```C++
>>> from doppler.cvt import ADC
>>> adc = ADC(bits=8, dbfs=0.0, dithering=0)
>>> adc.step(9.0)          # beyond full scale -> saturates, clips
127
>>> adc.reset()           # clear clips, re-seed the dither PRNG
>>> adc.clipped
False
```
 


        

<hr>



### function dp\_adc\_set\_state 

_Restore a state blob written by_ [_**dp\_adc\_get\_state()**_](adc__core_8h.md#function-dp_adc_get_state) _._
```C++
int dp_adc_set_state (
    dp_adc_state_t * state,
    const void * blob
) 
```



Validates the envelope (magic, version, endianness, size) first and leaves `state` untouched on rejection. On success the WHOLE struct is overwritten, configuration (bits, dbfs, scale, clip bounds, dithering) included, not just the running fields.




**Parameters:**


* `state` Instance to overwrite; non-NULL. 
* `blob` Blob of [**dp\_adc\_state\_bytes()**](adc__core_8h.md#function-dp_adc_state_bytes) bytes. 



**Returns:**

DP\_OK, or DP\_ERR\_INVALID if the envelope does not match. 





        

<hr>



### function dp\_adc\_state\_bytes 

_Size in bytes of this instance's serialized state blob._ 
```C++
size_t dp_adc_state_bytes (
    const dp_adc_state_t * state
) 
```



A fixed size: the 16-byte [**dp\_state\_hdr\_t**](structdp__state__hdr__t.md) envelope plus sizeof(dp\_adc\_state\_t) (the whole struct is snapshotted).




**Parameters:**


* `state` Instance to size; not dereferenced. 



**Returns:**

Blob size in bytes; allocate this much for [**dp\_adc\_get\_state()**](adc__core_8h.md#function-dp_adc_get_state). 





        

<hr>



### function dp\_adc\_step 

_Quantise one float sample to a signed N-bit ADC code._ 
```C++
JM_FORCEINLINE  JM_HOT int64_t dp_adc_step (
    dp_adc_state_t * state,
    float x
) 
```



Multiplies `x` by the pre-computed double-precision `scale`, optionally adds TPDF dither (when the object was built with dithering enabled), rounds with `llround`, and clamps to the signed integer range `[clip_min, clip_max]`. Latches the sticky `clipped` flag when the scaled (and dithered) value lies outside `[clip_min, clip_max]` BEFORE rounding, so a value such as clip\_max + 0.3 latches it even though it rounds to clip\_max. A sample at amplitude 10^(dbfs/20) reaches full scale.




**Parameters:**


* `state` Instance to update (clip flag, dither PRNG); non-NULL. 
* `x` Input sample, normally a normalised float in `[-1, +1]`. 



**Returns:**

Signed ADC code in `[-(2^(bits-1)), 2^(bits-1)-1]`.



```C++
>>> from doppler.cvt import ADC
>>> adc = ADC(bits=8, dbfs=0.0, dithering=0)  # 8-bit, FS at 0 dBFS
>>> adc.step(0.5)            # 0.5 * 128 codes
64
>>> adc.step(2.0)            # beyond full scale -> clamps to +127
127
>>> adc.clipped              # sticky flag latched by the clamp
True
```
 


        

<hr>



### function dp\_adc\_steps 

_Process a block of float samples to int64._ 
```C++
void dp_adc_steps (
    dp_adc_state_t * state,
    const float * input,
    int64_t * output,
    size_t n
) 
```



When dithering is disabled and a SIMD tier is available ([**jm\_simd.h**](jm__simd_8h.md)), whole SIMD-width groups are scaled by `scale` rounded to float32, multiplied in float32, rounded with `llroundf` and clamped; the clip flag latches on the same double-precision pre-rounding value step() uses, so it does not depend on which path a sample took. The remaining tail samples, and every sample when dithering is enabled, go through [**dp\_adc\_step()**](adc__core_8h.md#function-dp_adc_step) (double precision, sequential PRNG). For converters wider than ~24 bits the float32 path can therefore differ from step() in the last codes.


The C function writes into `output` and never allocates; the Python binding allocates the output array when its optional `out` is omitted.




**Parameters:**


* `state` Instance to update (clip flag, dither PRNG); non-NULL. 
* `input` Input float32 array; must contain at least `n` elements. 
* `output` Output int64 array; must hold at least `n` elements and must be non-NULL. 
* `n` Number of samples to process (0 is a no-op).


```C++
>>> from doppler.cvt import ADC
>>> import numpy as np
>>> # ideal 12-bit ADC: full scale spans +-2**11 codes
>>> ADC(12, 0.0, 0).steps(np.array([0.0, 0.5, 0.999, -1.0],
...                                dtype=np.float32)).tolist()
[0, 1024, 2046, -2048]
```
 


        

<hr>
## Macro Definition Documentation





### define ADC\_STATE\_MAGIC 

```C++
#define ADC_STATE_MAGIC `DP_FOURCC ('A','D','C',' ')`
```



FourCC type tag ("ADC ") stamped in every ADC state blob's envelope. 


        

<hr>



### define ADC\_STATE\_VERSION 

```C++
#define ADC_STATE_VERSION `1u`
```



ADC state blob format version; a blob of another version is rejected. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/adc/adc_core.h`

