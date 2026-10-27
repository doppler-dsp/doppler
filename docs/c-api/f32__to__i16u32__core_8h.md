

# File f32\_to\_i16u32\_core.h



[**FileList**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**f32\_to\_i16u32**](dir_8ed3b501fdbf556e99afaad1b0fb95c8.md) **>** [**f32\_to\_i16u32\_core.h**](f32__to__i16u32__core_8h.md)

[Go to the source code of this file](f32__to__i16u32__core_8h_source.md)

_Scale-and-saturate float to Q15-in-uint32 converter._ [More...](#detailed-description)

* `#include "doppler/clib_common.h"`
* `#include "doppler/dp_state.h"`
* `#include "doppler/jm_perf.h"`
* `#include <math.h>`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**dp\_f32\_to\_i16u32\_state\_t**](structdp__f32__to__i16u32__state__t.md) <br>_F32ToI16U32 state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**dp\_f32\_to\_i16u32\_state\_t**](structdp__f32__to__i16u32__state__t.md) \* | [**dp\_f32\_to\_i16u32\_create**](#function-dp_f32_to_i16u32_create) (float scale) <br>_Create a f32\_to\_i16u32 instance._  |
|  void | [**dp\_f32\_to\_i16u32\_destroy**](#function-dp_f32_to_i16u32_destroy) ([**dp\_f32\_to\_i16u32\_state\_t**](structdp__f32__to__i16u32__state__t.md) \* state) <br>_Destroy a f32\_to\_i16u32 instance and release all memory._  |
|  void | [**dp\_f32\_to\_i16u32\_get\_state**](#function-dp_f32_to_i16u32_get_state) (const [**dp\_f32\_to\_i16u32\_state\_t**](structdp__f32__to__i16u32__state__t.md) \* state, void \* blob) <br>_Snapshot the whole state struct (scale and clipped) into_ `blob` _._ |
|  void | [**dp\_f32\_to\_i16u32\_reset**](#function-dp_f32_to_i16u32_reset) ([**dp\_f32\_to\_i16u32\_state\_t**](structdp__f32__to__i16u32__state__t.md) \* state) <br>_Clear the sticky clip flag, starting a fresh saturation history._  |
|  int | [**dp\_f32\_to\_i16u32\_set\_state**](#function-dp_f32_to_i16u32_set_state) ([**dp\_f32\_to\_i16u32\_state\_t**](structdp__f32__to__i16u32__state__t.md) \* state, const void \* blob) <br>_Restore the whole state struct from_ `blob` _._ |
|  size\_t | [**dp\_f32\_to\_i16u32\_state\_bytes**](#function-dp_f32_to_i16u32_state_bytes) (const [**dp\_f32\_to\_i16u32\_state\_t**](structdp__f32__to__i16u32__state__t.md) \* state) <br>_Bytes_ [_**dp\_f32\_to\_i16u32\_get\_state()**_](f32__to__i16u32__core_8h.md#function-dp_f32_to_i16u32_get_state) _writes (envelope + payload)._ |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) uint32\_t | [**dp\_f32\_to\_i16u32\_step**](#function-dp_f32_to_i16u32_step) ([**dp\_f32\_to\_i16u32\_state\_t**](structdp__f32__to__i16u32__state__t.md) \* state, float x) <br>_Scale one float sample to a saturated Q15 code packed in a uint32._  |
|  void | [**dp\_f32\_to\_i16u32\_steps**](#function-dp_f32_to_i16u32_steps) ([**dp\_f32\_to\_i16u32\_state\_t**](structdp__f32__to__i16u32__state__t.md) \* state, const float \* input, uint32\_t \* output, size\_t n) <br>_Process a block of float samples to Q15-in-uint32._  |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**F32\_TO\_I16U32\_STATE\_MAGIC**](f32__to__i16u32__core_8h.md#define-f32_to_i16u32_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('F','U','3','2')`<br> |
| define  | [**F32\_TO\_I16U32\_STATE\_VERSION**](f32__to__i16u32__core_8h.md#define-f32_to_i16u32_state_version)  `1u`<br> |

## Detailed Description


Converts a float to a saturated int16, then zero-extends the 16-bit two's complement bit pattern into the lower 16 bits of a uint32 (upper 16 bits are always zero). The upper bits leave room for a consumer that accumulates in place. The CIC does not take this format: it takes CF32 and encodes to UQ16 internally (`cic_core.h`).


input +1.0 → int16 32767 → uint32 0x00007FFF input -1.0 → int16 -32768 → uint32 0x00008000


The default scale of 32768.0 maps `[-1, +1]` float to Q15 range. A sticky `clipped` flag is raised on saturation and cleared only by reset().


Lifecycle: create -&gt; (step / steps / reset)\* -&gt; destroy



```C++
>>> from doppler.cvt import F32ToI16U32
>>> import numpy as np
>>> obj = F32ToI16U32(scale=32768.0)
>>> hex(obj.step(-1.0))
'0x8000'
>>> hex(obj.step(1.0))
'0x7fff'
>>> obj.step(0.0)
0
>>> x = np.array([-1.0, 0.0, 1.0], dtype=np.float32)
>>> obj.steps(x).tolist()
[32768, 0, 32767]
```
 


    
## Public Functions Documentation




### function dp\_f32\_to\_i16u32\_create 

_Create a f32\_to\_i16u32 instance._ 
```C++
dp_f32_to_i16u32_state_t * dp_f32_to_i16u32_create (
    float scale
) 
```



Stores `scale` and initialises the sticky `clipped` flag to 0.




**Parameters:**


* `scale` Multiply factor applied before quantisation and saturation (default: 32768.0f). Use 32768.0 to convert normalised `[-1, +1]` samples to Q15 packed into a uint32. Must be &gt; 0. 



**Returns:**

Heap-allocated state, or NULL if `scale` &lt;= 0 or allocation fails. 




**Note:**

Caller must call [**dp\_f32\_to\_i16u32\_destroy()**](f32__to__i16u32__core_8h.md#function-dp_f32_to_i16u32_destroy) when done. 





        

<hr>



### function dp\_f32\_to\_i16u32\_destroy 

_Destroy a f32\_to\_i16u32 instance and release all memory._ 
```C++
void dp_f32_to_i16u32_destroy (
    dp_f32_to_i16u32_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function dp\_f32\_to\_i16u32\_get\_state 

_Snapshot the whole state struct (scale and clipped) into_ `blob` _._
```C++
void dp_f32_to_i16u32_get_state (
    const dp_f32_to_i16u32_state_t * state,
    void * blob
) 
```





**Parameters:**


* `state` State. Must be non-NULL. Not modified. 
* `blob` Caller-owned buffer of at least [**dp\_f32\_to\_i16u32\_state\_bytes()**](f32__to__i16u32__core_8h.md#function-dp_f32_to_i16u32_state_bytes) bytes. 




        

<hr>



### function dp\_f32\_to\_i16u32\_reset 

_Clear the sticky clip flag, starting a fresh saturation history._ 
```C++
void dp_f32_to_i16u32_reset (
    dp_f32_to_i16u32_state_t * state
) 
```



Zeroes `clipped` so a subsequent clipped query reflects only samples seen after this call; `scale` is preserved. Call it at a buffer or segment boundary so a saturation on one block does not leak into the next.




**Parameters:**


* `state` Must be non-NULL.


```C++
>>> from doppler.cvt import F32ToI16U32
>>> c = F32ToI16U32()
>>> c.step(5.0)          # out of range -> saturates, latches clipped
32767
>>> c.reset()            # forget the clip history
>>> c.clipped
False
```
 


        

<hr>



### function dp\_f32\_to\_i16u32\_set\_state 

_Restore the whole state struct from_ `blob` _._
```C++
int dp_f32_to_i16u32_set_state (
    dp_f32_to_i16u32_state_t * state,
    const void * blob
) 
```



The envelope (magic, version, endianness, size) is validated first; on success both `scale` and `clipped` are overwritten from the blob, so the instance takes the source's scale too.




**Parameters:**


* `state` State. Must be non-NULL; unchanged on rejection. 
* `blob` Blob from [**dp\_f32\_to\_i16u32\_get\_state()**](f32__to__i16u32__core_8h.md#function-dp_f32_to_i16u32_get_state). 



**Returns:**

DP\_OK, or DP\_ERR\_INVALID if the envelope rejects. 





        

<hr>



### function dp\_f32\_to\_i16u32\_state\_bytes 

_Bytes_ [_**dp\_f32\_to\_i16u32\_get\_state()**_](f32__to__i16u32__core_8h.md#function-dp_f32_to_i16u32_get_state) _writes (envelope + payload)._
```C++
size_t dp_f32_to_i16u32_state_bytes (
    const dp_f32_to_i16u32_state_t * state
) 
```





**Parameters:**


* `state` State; not read (the size is fixed). 



**Returns:**

sizeof(dp\_state\_hdr\_t) + sizeof(dp\_f32\_to\_i16u32\_state\_t). 





        

<hr>



### function dp\_f32\_to\_i16u32\_step 

_Scale one float sample to a saturated Q15 code packed in a uint32._ 
```C++
JM_FORCEINLINE  JM_HOT uint32_t dp_f32_to_i16u32_step (
    dp_f32_to_i16u32_state_t * state,
    float x
) 
```



Computes `round(x * scale)`, saturates to `[-32768, 32767]`, then zero-extends the 16-bit two's-complement pattern into the lower 16 bits of a uint32 (upper 16 bits are always zero). Latches the sticky `clipped` flag on saturation. A NaN input maps to 0x8000 (-32768) without setting `clipped`.




**Parameters:**


* `state` Must be non-NULL. 
* `x` Input sample, normally a normalised float in `[-1, +1]`. 



**Returns:**

Q15 code in the low 16 bits of a uint32; e.g. -32768 -&gt; 0x8000.



```C++
>>> from doppler.cvt import F32ToI16U32
>>> c = F32ToI16U32(scale=32768.0)
>>> c.step(0.5)              # 0.5 -> Q15 16384, upper 16 bits zero
16384
>>> hex(c.step(-1.0))        # -32768 as an unsigned low-16 pattern
'0x8000'
```
 


        

<hr>



### function dp\_f32\_to\_i16u32\_steps 

_Process a block of float samples to Q15-in-uint32._ 
```C++
void dp_f32_to_i16u32_steps (
    dp_f32_to_i16u32_state_t * state,
    const float * input,
    uint32_t * output,
    size_t n
) 
```



Applies step() to every element. The `clipped` flag is updated cumulatively across the block. In C the caller supplies `output`; the Python binding accepts an optional pre-allocated `out` array and allocates a fresh one when it is omitted.




**Parameters:**


* `state` Must be non-NULL. 
* `input` Input float32 array; must contain at least `n` elements. 
* `output` Output uint32 array; must be non-NULL and contain at least `n` elements. 
* `n` Number of samples to process.


```C++
>>> from doppler.cvt import F32ToI16U32
>>> import numpy as np
>>> F32ToI16U32().steps(
...     np.array([0.0, 0.5], dtype=np.float32)).tolist()
[0, 16384]
```
 


        

<hr>
## Macro Definition Documentation





### define F32\_TO\_I16U32\_STATE\_MAGIC 

```C++
#define F32_TO_I16U32_STATE_MAGIC `DP_FOURCC ('F','U','3','2')`
```



Envelope magic ("FU32") that tags an f32\_to\_i16u32 state blob. 


        

<hr>



### define F32\_TO\_I16U32\_STATE\_VERSION 

```C++
#define F32_TO_I16U32_STATE_VERSION `1u`
```



Payload layout version of an f32\_to\_i16u32 state blob. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/f32_to_i16u32/f32_to_i16u32_core.h`

