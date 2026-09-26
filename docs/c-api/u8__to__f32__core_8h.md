

# File u8\_to\_f32\_core.h



[**FileList**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**u8\_to\_f32**](dir_b468cb3e760d860bad02e34079834f5d.md) **>** [**u8\_to\_f32\_core.h**](u8__to__f32__core_8h.md)

[Go to the source code of this file](u8__to__f32__core_8h_source.md)

_Offset-binary uint8 to float converter — the RTL-SDR_ `cu8` _front end._[More...](#detailed-description)

* `#include "doppler/clib_common.h"`
* `#include "doppler/jm_perf.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**u8\_to\_f32\_state\_t**](structu8__to__f32__state__t.md) <br>_U8ToF32 state._  |


## Public Types

| Type | Name |
| ---: | :--- |
| enum  | [**u8\_to\_f32\_mode\_t**](#enum-u8_to_f32_mode_t)  <br>_The two mappings, in the order of the Python_ `mode` _string enum._ |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**u8\_to\_f32\_state\_t**](structu8__to__f32__state__t.md) \* | [**u8\_to\_f32\_create**](#function-u8_to_f32_create) (int mode) <br>_Create a u8\_to\_f32 instance._  |
|  void | [**u8\_to\_f32\_destroy**](#function-u8_to_f32_destroy) ([**u8\_to\_f32\_state\_t**](structu8__to__f32__state__t.md) \* state) <br>_Destroy a u8\_to\_f32 instance and release all memory._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) float | [**u8\_to\_f32\_midpoint**](#function-u8_to_f32_midpoint) (const [**u8\_to\_f32\_state\_t**](structu8__to__f32__state__t.md) \* state, uint8\_t x) <br>_The_ `midpoint` _mapping of one code:_`(x - 127.5) * (1/127.5)` _._ |
|  void | [**u8\_to\_f32\_reset**](#function-u8_to_f32_reset) ([**u8\_to\_f32\_state\_t**](structu8__to__f32__state__t.md) \* state) <br>_No-op reset, provided only for lifecycle symmetry._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) float | [**u8\_to\_f32\_shift**](#function-u8_to_f32_shift) (uint8\_t x) <br>_The_ `shift` _mapping of one code:_`(x - 128) * 2^-7` _, exactly._ |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) float | [**u8\_to\_f32\_step**](#function-u8_to_f32_step) (const [**u8\_to\_f32\_state\_t**](structu8__to__f32__state__t.md) \* state, uint8\_t x) <br>_Convert one offset-binary code to a normalised float._  |
|  void | [**u8\_to\_f32\_steps**](#function-u8_to_f32_steps) ([**u8\_to\_f32\_state\_t**](structu8__to__f32__state__t.md) \* state, const uint8\_t \* input, float \* output, size\_t n) <br>_Convert a block of offset-binary codes to float32._  |




























## Detailed Description


An RTL-SDR (RTL2832U) streams interleaved I/Q as UNSIGNED 8-bit offset-binary samples, `cu8`: code 0 is the most negative level, 255 the most positive, and the analog zero sits between codes 127 and 128, at 127.5. This is not the signed-8 format of I8ToF32 (HackRF's `cs8`) — read as signed, every sample above 127 wraps to a large negative value.


The converter works element by element on the FLAT interleaved buffer, so `2N` bytes of `I,Q,I,Q,...` become `2N` floats, which is `N` complex samples in `float _Complex` layout. From C, pass the complex output buffer cast to `float *`; from Python, `.view(np.complex64)` the result.


Two mappings, chosen at construction by `mode:` 



|mode   |formula   |range   |cost    |
|-----|-----|-----|-----|
|`shift`   |`(x - 128) * 2^-7`   |`[-1, 127/128]`   |integer subtract, convert, power-of-two scale    |
|`midpoint`   |`(x - 127.5) * (1/127.5)`   |`[-1, +1]`   |float subtract and multiply   |






\*\*`shift` (the default) is exact and the fast path.\*\* Every step is exact in float: the integer subtract, the conversion of a value in `[-128, 127]`, and the power-of-two scale. Both modes vectorise (GCC 14 on AArch64 emits NEON `scvtf` + `fmul` on four lanes; it does not fold the 2^-7 into the convert's fractional-bits operand, though the instruction has one), so the gap between them is small — 4% on a Cortex-A53-class core, 20% on a desktop x86 (`bench_u8_to_f32_core`). It is identical, bit for bit, to I8ToF32 at `scale=128` applied to `x ^ 0x80`. The price is a DC bias: it centres on code 128 while the hardware centres on 127.5, so every sample reads `0.5/128` low — an analog zero dithers between codes 127 and 128, which map to `-1/128` and `0` — a constant at about -48 dBFS. In a receiver that tunes the wanted signal off DC — the normal way to use an RTL-SDR, whose own DC spike sits at the same place — the downstream down-converter's channel filter removes it, so the fast path costs nothing that survives.


\*\*`midpoint` is the opt-in unbiased mapping\*\*: centred on 127.5 and symmetric, so a zero-mean input stays zero-mean and both rails map to exactly -1 and +1. Use it when DC matters: a zero-IF capture, a power measurement taken before any filtering, or a spectrum whose DC bin you intend to read. The scale is a pre-computed reciprocal, so a result can differ from the true quotient `(x - 127.5)/127.5` in the last bit.


Stateless: nothing survives between calls, so there is no state to serialize and reset() is a no-op.


Lifecycle: create -&gt; `[step / steps / reset]*` -&gt; destroy



```C++
>>> from doppler.cvt import U8ToF32
>>> import numpy as np
>>> iq = np.array([0, 128, 255, 128], dtype=np.uint8)   # I,Q,I,Q
>>> U8ToF32().steps(iq).tolist()                        # mode="shift"
[-1.0, 0.0, 0.9921875, 0.0]
>>> U8ToF32().steps(iq).view(np.complex64).tolist()
[(-1+0j), (0.9921875+0j)]
>>> U8ToF32(mode="midpoint").steps(iq)[[0, 2]].tolist()  # both rails
[-1.0, 1.0]
```
 


    
## Public Types Documentation




### enum u8\_to\_f32\_mode\_t 

_The two mappings, in the order of the Python_ `mode` _string enum._
```C++
enum u8_to_f32_mode_t {
    U8_TO_F32_SHIFT = 0,
    U8_TO_F32_MIDPOINT = 1
};
```



The Python binding passes the index of `"shift"` / `"midpoint"`, so these values ARE that order; a C caller may use either spelling. 


        

<hr>
## Public Functions Documentation




### function u8\_to\_f32\_create 

_Create a u8\_to\_f32 instance._ 
```C++
u8_to_f32_state_t * u8_to_f32_create (
    int mode
) 
```





**Parameters:**


* `mode` U8\_TO\_F32\_SHIFT (0) or U8\_TO\_F32\_MIDPOINT (1); from Python, the string `"shift"` (default) or `"midpoint"`. 



**Returns:**

Heap-allocated state, or NULL for an unknown `mode`. The allocation itself cannot fail visibly: it aborts on out-of-memory (dp\_xcalloc), as every fixed-size internal allocation does. 




**Note:**

Caller must call [**u8\_to\_f32\_destroy()**](u8__to__f32__core_8h.md#function-u8_to_f32_destroy) when done. 





        

<hr>



### function u8\_to\_f32\_destroy 

_Destroy a u8\_to\_f32 instance and release all memory._ 
```C++
void u8_to_f32_destroy (
    u8_to_f32_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function u8\_to\_f32\_midpoint 

_The_ `midpoint` _mapping of one code:_`(x - 127.5) * (1/127.5)` _._
```C++
JM_FORCEINLINE float u8_to_f32_midpoint (
    const u8_to_f32_state_t * state,
    uint8_t x
) 
```



The one definition of the unbiased path; step() and steps() both call it.




**Parameters:**


* `state` Must be non-NULL (supplies the pre-computed reciprocal). 
* `x` Offset-binary code in `[0, 255]`. 



**Returns:**

`(x - 127.5) / 127.5` to within the last bit, in `[-1, +1]`. 





        

<hr>



### function u8\_to\_f32\_reset 

_No-op reset, provided only for lifecycle symmetry._ 
```C++
void u8_to_f32_reset (
    u8_to_f32_state_t * state
) 
```



The mode and its reciprocal are fixed at construction and nothing else is held, so there is nothing to clear; the method exists so every converter in the module presents the same create / step / reset / destroy lifecycle.




**Parameters:**


* `state` Must be non-NULL.


```C++
>>> from doppler.cvt import U8ToF32
>>> c = U8ToF32()
>>> c.reset()          # stateless converter -> reset is a no-op
>>> c.step(0)
-1.0
```
 


        

<hr>



### function u8\_to\_f32\_shift 

_The_ `shift` _mapping of one code:_`(x - 128) * 2^-7` _, exactly._
```C++
JM_FORCEINLINE float u8_to_f32_shift (
    uint8_t x
) 
```



The one definition of the fast path; step() and steps() both call it. The subtract is done in `int32_t` rather than by flipping the top bit into an `int8_t`, which gives the same value without C's implementation-defined narrowing conversion.




**Parameters:**


* `x` Offset-binary code in `[0, 255]`. 



**Returns:**

`(x - 128) / 128`, in `[-1, 127/128]`. 





        

<hr>



### function u8\_to\_f32\_step 

_Convert one offset-binary code to a normalised float._ 
```C++
JM_FORCEINLINE  JM_HOT float u8_to_f32_step (
    const u8_to_f32_state_t * state,
    uint8_t x
) 
```



Dispatches on the mode chosen at construction. For a block, steps() resolves the mode once and runs a branch-free loop instead.




**Parameters:**


* `state` Must be non-NULL. 
* `x` Offset-binary code in `[0, 255]`. 



**Returns:**

The mapped sample (see the table at the top of this file).



```C++
>>> from doppler.cvt import U8ToF32
>>> c = U8ToF32()             # mode="shift": (x - 128) / 128, exact
>>> c.step(0), c.step(128), c.step(192)
(-1.0, 0.0, 0.5)
>>> m = U8ToF32(mode="midpoint")
>>> m.step(0), m.step(255)    # symmetric: both rails reach full scale
(-1.0, 1.0)
```
 


        

<hr>



### function u8\_to\_f32\_steps 

_Convert a block of offset-binary codes to float32._ 
```C++
void u8_to_f32_steps (
    u8_to_f32_state_t * state,
    const uint8_t * input,
    float * output,
    size_t n
) 
```



The mode is resolved once for the block, then one branch-free loop runs, so the per-sample work is only the mapping itself. Feed it the flat interleaved I/Q buffer; the output is then complex samples in `float _Complex` layout.




**Parameters:**


* `state` Must be non-NULL. 
* `input` Input uint8 array; must contain at least `n` elements. 
* `output` Output float32 array; must contain at least `n` elements. 
* `n` Number of samples (bytes) to convert.


```C++
>>> from doppler.cvt import U8ToF32
>>> import numpy as np
>>> cu8 = np.array([128, 0, 192, 64], dtype=np.uint8)  # 2 I/Q pairs
>>> U8ToF32().steps(cu8).view(np.complex64).tolist()
[-1j, (0.5-0.5j)]
```
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/u8_to_f32/u8_to_f32_core.h`

