

# File cic\_core.h



[**FileList**](files.md) **>** [**cic**](dir_cf560077cc62991e7289ea57a3d930a1.md) **>** [**cic\_core.h**](cic__core_8h.md)

[Go to the source code of this file](cic__core_8h_source.md)

_CIC decimation filter — 4-stage, M=1, UQ16 integer pipeline._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "dp_state.h"`
* `#include "jm_perf.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**cic\_state\_t**](structcic__state__t.md) <br>_CIC filter state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**cic\_state\_t**](structcic__state__t.md) \* | [**cic\_create**](#function-cic_create) (uint32\_t R) <br>_Create a 4-stage, M=1 CIC decimation filter. Allocates the state struct on the heap and pre-computes the normalisation right-shift (CIC\_N \* log2(R) bits). All integrator and comb accumulators are zeroed; the first output arrives after R input samples. Returns NULL for invalid R or OOM._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) size\_t | [**cic\_decimate**](#function-cic_decimate) ([**cic\_state\_t**](structcic__state__t.md) \* state, const float complex \* in, size\_t n\_in, float complex \* out) <br>_Decimate a block of CF32 samples through the CIC pipeline. Each sample is converted to offset-binary UQ16, pushed through CIC\_N integrators (unsigned wrapping), and when the phase counter reaches R the integrated value is passed through CIC\_N M=1 comb stages and converted back to CF32. State persists between calls. Feeding blocks that are multiples of R gives predictable output counts (exactly n\_in/R samples per block)._  |
|  size\_t | [**cic\_decimate\_max\_out**](#function-cic_decimate_max_out) ([**cic\_state\_t**](structcic__state__t.md) \* state) <br>_Upper bound on decimate output — returns 0 (lazy-alloc signal)._  |
|  void | [**cic\_destroy**](#function-cic_destroy) ([**cic\_state\_t**](structcic__state__t.md) \* state) <br> |
|  void | [**cic\_get\_state**](#function-cic_get_state) (const [**cic\_state\_t**](structcic__state__t.md) \* state, void \* blob) <br>_Serialize the integrator/comb/phase state into_ `blob` _._ |
|  void | [**cic\_reconfigure**](#function-cic_reconfigure) ([**cic\_state\_t**](structcic__state__t.md) \* state, uint32\_t R) <br>_Change the decimation ratio in place and reset all filter state. Recomputes the normalisation shift (CIC\_N \* log2(R)) and zeros all accumulators so the filter behaves exactly like a freshly created one with the new R. Silently ignores R values that are not a power-of-two in_ `[2, 4096]` _— the state is left unchanged in that case._ |
|  void | [**cic\_reset**](#function-cic_reset) ([**cic\_state\_t**](structcic__state__t.md) \* state) <br>_Zero all integrator and comb accumulators; preserve R and shift. The first output sample after reset arrives after R more input samples, matching post-create behaviour. Use between signal bursts to eliminate transient artefacts caused by residual pipeline state._  |
|  int | [**cic\_set\_state**](#function-cic_set_state) ([**cic\_state\_t**](structcic__state__t.md) \* state, const void \* blob) <br>_Restore the integrator/comb/phase state from_ `blob` _._ |
|  size\_t | [**cic\_state\_bytes**](#function-cic_state_bytes) (const [**cic\_state\_t**](structcic__state__t.md) \* state) <br>_Bytes_ [_**cic\_get\_state()**_](cic__core_8h.md#function-cic_get_state) _writes (envelope + payload)._ |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**CIC\_N**](cic__core_8h.md#define-cic_n)  `4`<br> |
| define  | [**CIC\_STATE\_MAGIC**](cic__core_8h.md#define-cic_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('C', 'I', 'C', '\_')`<br> |
| define  | [**CIC\_STATE\_VERSION**](cic__core_8h.md#define-cic_state_version)  `1u`<br> |

## Detailed Description


Fixed design parameters: N = 4 stages (~77 dB alias rejection at f\_p = 0.1 \* f\_out) M = 1 (differential delay — one-sample comb) R = power-of-two decimation ratio (enforced at create time)


Input/output boundary: CF32 (`float _Complex`), matching the doppler default signal type. Internally, each sample is converted to UQ16 — offset-binary: v\_q15 + 32768 → `[0, 65535]` in a uint64\_t — giving 48 bits of headroom for the pipeline gain of N \* log2(R) bits. For R &lt;= 4096 (log2 = 12) the gain is 48 bits; max accumulation = 65535 \* R^N = (2^16 - 1) \* 2^48 = 2^64 - 2^48 &lt; 2^64, so no overflow occurs.


All arithmetic is unsigned: inputs are non-negative `[0, 65535]`, wrapping is defined (mod 2^64), and the output decode subtracts the offset in floating-point — no signed integer casts anywhere in the hot path.


The unsigned modular-arithmetic CIC property guarantees exact outputs: every intermediate overflow in the integrators cancels in the comb stages, provided the true result fits in 64 bits. No saturation, no range checks, no floating-point in the inner loop.


With M=1 and N fixed, the entire comb state is four uint64\_t values per channel — no heap allocation beyond the state struct itself.


Alias rejection : ~77 dB at f\_p = 0.1 \* f\_out (independent of R) Passband droop : ~0.57 dB at f\_p = 0.1 \* f\_out (independent of R) Output precision: 16-bit Q15 (independent of R and N)



```C++
cic_state_t *cic = cic_create(16);   // R=16, N=4, M=1
size_t n_out = cic_decimate(cic, in, 1024, out);
cic_destroy(cic);
```
 


    
## Public Functions Documentation




### function cic\_create 

_Create a 4-stage, M=1 CIC decimation filter. Allocates the state struct on the heap and pre-computes the normalisation right-shift (CIC\_N \* log2(R) bits). All integrator and comb accumulators are zeroed; the first output arrives after R input samples. Returns NULL for invalid R or OOM._ 
```C++
cic_state_t * cic_create (
    uint32_t R
) 
```





**Parameters:**


* `R` Decimation ratio. Must be a power of two in `[2, 4096]`. Returns NULL for R=0, non-power-of-two, or R &gt; 4096. 



**Returns:**

Heap-allocated state, or NULL on invalid R or OOM.



```C++
>>> from doppler.resample import CIC
>>> cic = CIC(R=16)
>>> cic.R, cic.shift
(16, 16)
```
 


        

<hr>



### function cic\_decimate 

_Decimate a block of CF32 samples through the CIC pipeline. Each sample is converted to offset-binary UQ16, pushed through CIC\_N integrators (unsigned wrapping), and when the phase counter reaches R the integrated value is passed through CIC\_N M=1 comb stages and converted back to CF32. State persists between calls. Feeding blocks that are multiples of R gives predictable output counts (exactly n\_in/R samples per block)._ 
```C++
JM_FORCEINLINE  JM_HOT size_t cic_decimate (
    cic_state_t * state,
    const float complex * in,
    size_t n_in,
    float complex * out
) 
```





**Parameters:**


* `state` Pointer to a valid [**cic\_state\_t**](structcic__state__t.md). 
* `in` CF32 input block. 
* `n_in` Number of input samples. 
* `out` Output buffer; must hold at least n\_in elements. 



**Returns:**

CF32 output array; length is floor((phase + n\_in) / R).



```C++
>>> from doppler.resample import CIC
>>> import numpy as np
>>> cic = CIC(R=16)
>>> for _ in range(4):
...     _ = cic.decimate(np.zeros(16, dtype=np.complex64))
>>> y = cic.decimate(np.zeros(16, dtype=np.complex64))
>>> y.tolist(), y.dtype
([0j], dtype('complex64'))
```
 


        

<hr>



### function cic\_decimate\_max\_out 

_Upper bound on decimate output — returns 0 (lazy-alloc signal)._ 
```C++
size_t cic_decimate_max_out (
    cic_state_t * state
) 
```



The Python extension allocates n\_in elements on the first call. Since n\_in &gt;= ceil(n\_in/R) = n\_out for all R &gt;= 1, the buffer is always large enough as long as block size stays consistent. 


        

<hr>



### function cic\_destroy 

```C++
void cic_destroy (
    cic_state_t * state
) 
```



Free resources. NULL is a no-op. 


        

<hr>



### function cic\_get\_state 

_Serialize the integrator/comb/phase state into_ `blob` _._
```C++
void cic_get_state (
    const cic_state_t * state,
    void * blob
) 
```




<hr>



### function cic\_reconfigure 

_Change the decimation ratio in place and reset all filter state. Recomputes the normalisation shift (CIC\_N \* log2(R)) and zeros all accumulators so the filter behaves exactly like a freshly created one with the new R. Silently ignores R values that are not a power-of-two in_ `[2, 4096]` _— the state is left unchanged in that case._
```C++
void cic_reconfigure (
    cic_state_t * state,
    uint32_t R
) 
```





**Parameters:**


* `state` Pointer to a valid [**cic\_state\_t**](structcic__state__t.md). 
* `R` New decimation ratio. Same constraints as [**cic\_create()**](cic__core_8h.md#function-cic_create).


```C++
>>> from doppler.resample import CIC
>>> cic = CIC(R=4)
>>> cic.reconfigure(8)
>>> cic.R, cic.shift
(8, 12)
```
 


        

<hr>



### function cic\_reset 

_Zero all integrator and comb accumulators; preserve R and shift. The first output sample after reset arrives after R more input samples, matching post-create behaviour. Use between signal bursts to eliminate transient artefacts caused by residual pipeline state._ 
```C++
void cic_reset (
    cic_state_t * state
) 
```




```C++
>>> from doppler.resample import CIC
>>> cic = CIC(R=16)
>>> cic.reset()
>>> cic.R
16
```
 


        

<hr>



### function cic\_set\_state 

_Restore the integrator/comb/phase state from_ `blob` _._
```C++
int cic_set_state (
    cic_state_t * state,
    const void * blob
) 
```





**Returns:**

DP\_OK, or DP\_ERR\_INVALID if the blob's envelope rejects. 





        

<hr>



### function cic\_state\_bytes 

_Bytes_ [_**cic\_get\_state()**_](cic__core_8h.md#function-cic_get_state) _writes (envelope + payload)._
```C++
size_t cic_state_bytes (
    const cic_state_t * state
) 
```




<hr>
## Macro Definition Documentation





### define CIC\_N 

```C++
#define CIC_N `4`
```



Fixed stage count. Alias rejection ~19.2 dB/stage at f\_p=0.1. 


        

<hr>



### define CIC\_STATE\_MAGIC 

```C++
#define CIC_STATE_MAGIC `DP_FOURCC ('C', 'I', 'C', '_')`
```




<hr>



### define CIC\_STATE\_VERSION 

```C++
#define CIC_STATE_VERSION `1u`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/cic/cic_core.h`

