

# File cic\_core.h



[**FileList**](files.md) **>** [**cic**](dir_b18ea702eaae2e8004fd6583c8b1e641.md) **>** [**cic\_core.h**](cic__core_8h.md)

[Go to the source code of this file](cic__core_8h_source.md)

_CIC decimation filter — CIC\_N stages, one-sample combs, an integer pipeline in CIC\_REG\_BITS-bit registers._ [More...](#detailed-description)

* `#include "doppler/clib_common.h"`
* `#include "doppler/dp_state.h"`
* `#include "doppler/jm_perf.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**dp\_cic\_state\_t**](structdp__cic__state__t.md) <br>_CIC filter state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**dp\_cic\_state\_t**](structdp__cic__state__t.md) \* | [**dp\_cic\_create**](#function-dp_cic_create) (uint32\_t R) <br>_Create a CIC decimation filter of CIC\_N stages. Allocates the state struct on the heap and pre-computes the normalisation right-shift (CIC\_N \* log2(R) bits). All integrator and comb accumulators are zeroed; the first output arrives after R input samples. Returns NULL for invalid R or OOM. The filter is built at CIC\_PRECISION\_DEFAULT with that precision's headroom, so its input is bounded at CIC\_PAPR\_WIDE (_ [_**dp\_cic\_set\_precision()**_](cic__core_8h.md#function-dp_cic_set_precision) _,_[_**dp\_cic\_set\_headroom\_bits()**_](cic__core_8h.md#function-dp_cic_set_headroom_bits) _). A component at or beyond it is clipped at the boundary before any filtering; the sample stream gives no sign of it, so check the sticky_`clipped` _flag. Unlike doppler's floating-point blocks this one is not scale-free_ _scale the input into range first._ |
|  double | [**dp\_cic\_dc\_gain**](#function-dp_cic_dc_gain) (const [**dp\_cic\_state\_t**](structdp__cic__state__t.md) \* state) <br>_The filter's response to a constant input, from its own geometry._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) size\_t | [**dp\_cic\_decimate**](#function-dp_cic_decimate) ([**dp\_cic\_state\_t**](structdp__cic__state__t.md) \* state, const float \_Complex \* in, size\_t n\_in, float \_Complex \* out, size\_t max\_out) <br>_Decimate a block of CF32 samples through the CIC pipeline. Each sample is converted to 16-bit Q15, pushed through CIC\_N integrators (unsigned wrapping), and when the phase counter reaches R the integrated value is passed through CIC\_N one-sample combs and converted back to CF32. State persists between calls. A fresh or reset filter starts from silence: its first CIC\_N - 1 outputs are the filter filling with the input, and silence in gives exact zeros out from the first one. Feeding blocks that are multiples of R gives predictable output counts (exactly n\_in/R samples per block)._  |
|  size\_t | [**dp\_cic\_decimate\_max\_out**](#function-dp_cic_decimate_max_out) ([**dp\_cic\_state\_t**](structdp__cic__state__t.md) \* state) <br>_Upper bound on decimate output — returns 0 (lazy-alloc signal)._  |
|  void | [**dp\_cic\_destroy**](#function-dp_cic_destroy) ([**dp\_cic\_state\_t**](structdp__cic__state__t.md) \* state) <br>_Free the filter state._  |
|  uint32\_t | [**dp\_cic\_get\_bits**](#function-dp_cic_get_bits) (const [**dp\_cic\_state\_t**](structdp__cic__state__t.md) \* state) <br>_The input word's width in bits, sign included._  |
|  double | [**dp\_cic\_get\_full\_scale**](#function-dp_cic_get_full_scale) (const [**dp\_cic\_state\_t**](structdp__cic__state__t.md) \* state) <br>_The input bound: a component at or beyond it is clipped._  |
|  double | [**dp\_cic\_get\_group\_delay**](#function-dp_cic_get_group_delay) (const [**dp\_cic\_state\_t**](structdp__cic__state__t.md) \* state) <br>_The decimator's group delay, in its INPUT samples: where in the input timeline an output sample's content sits._  |
|  uint32\_t | [**dp\_cic\_get\_headroom\_bits**](#function-dp_cic_get_headroom_bits) (const [**dp\_cic\_state\_t**](structdp__cic__state__t.md) \* state) <br>_The headroom in use, in bits._  |
|  uint32\_t | [**dp\_cic\_get\_precision**](#function-dp_cic_get_precision) (const [**dp\_cic\_state\_t**](structdp__cic__state__t.md) \* state) <br>_The precision in use._  |
|  void | [**dp\_cic\_get\_state**](#function-dp_cic_get_state) (const [**dp\_cic\_state\_t**](structdp__cic__state__t.md) \* state, void \* blob) <br>_Serialize R, the integrator/comb/phase state and_ `clipped` _into_`blob` _. shift is not written: it is derived from R._ |
|  double | [**dp\_cic\_get\_step**](#function-dp_cic_get_step) (const [**dp\_cic\_state\_t**](structdp__cic__state__t.md) \* state) <br>_The input's step: a component smaller than this reads zero, and the output is a whole number of them._  |
|  int | [**dp\_cic\_headroom\_bits\_ok**](#function-dp_cic_headroom_bits_ok) (uint32\_t bits) <br>_Is this a headroom a CIC takes: 0 to CIC\_HEADROOM\_BITS\_MAX bits._  |
|  int | [**dp\_cic\_precision\_ok**](#function-dp_cic_precision_ok) (uint32\_t precision) <br>_Is this a precision: CIC\_PRECISION\_16, CIC\_PRECISION\_32 or CIC\_PRECISION\_64._  |
|  void | [**dp\_cic\_reconfigure**](#function-dp_cic_reconfigure) ([**dp\_cic\_state\_t**](structdp__cic__state__t.md) \* state, uint32\_t R) <br>_Change the decimation ratio in place and reset all filter state. Recomputes the normalisation shift (CIC\_N \* log2(R)) and zeros all accumulators so the filter behaves exactly like a freshly created one with the new R. Silently ignores R values that are not a power-of-two in_ `[2, 2048]` _(_`CIC_R_MAX` _) — the state is left unchanged in that case._ |
|  void | [**dp\_cic\_reset**](#function-dp_cic_reset) ([**dp\_cic\_state\_t**](structdp__cic__state__t.md) \* state) <br>_Zero all integrator and comb accumulators; preserve R and shift. The first output sample after reset arrives after R more input samples, matching post-create behaviour. Use between signal bursts to eliminate transient artefacts caused by residual pipeline state. Also clears the sticky_ `clipped` _flag._ |
|  int | [**dp\_cic\_set\_headroom\_bits**](#function-dp_cic_set_headroom_bits) ([**dp\_cic\_state\_t**](structdp__cic__state__t.md) \* state, uint32\_t bits) <br>_Set how much of the input word is headroom, and reset the filter._  |
|  int | [**dp\_cic\_set\_precision**](#function-dp_cic_set_precision) ([**dp\_cic\_state\_t**](structdp__cic__state__t.md) \* state, uint32\_t precision) <br>_Choose the input word, with that word's headroom, and reset the filter._  |
|  int | [**dp\_cic\_set\_state**](#function-dp_cic_set_state) ([**dp\_cic\_state\_t**](structdp__cic__state__t.md) \* state, const void \* blob) <br>_Restore R, the integrator/comb/phase state and_ `clipped` _from_`blob` _._ |
|  size\_t | [**dp\_cic\_state\_bytes**](#function-dp_cic_state_bytes) (const [**dp\_cic\_state\_t**](structdp__cic__state__t.md) \* state) <br>_Bytes_ [_**dp\_cic\_get\_state()**_](cic__core_8h.md#function-dp_cic_get_state) _writes (envelope + payload)._ |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**CIC\_FRACTION\_BITS\_MIN**](cic__core_8h.md#define-cic_fraction_bits_min)  `1u`<br> |
| define  | [**CIC\_HEADROOM\_BITS\_16**](cic__core_8h.md#define-cic_headroom_bits_16)  `1u`<br> |
| define  | [**CIC\_HEADROOM\_BITS\_MAX**](cic__core_8h.md#define-cic_headroom_bits_max)  `([**CIC\_PRECISION\_16**](cic__core_8h.md#define-cic_precision_16) - 1u - [**CIC\_FRACTION\_BITS\_MIN**](cic__core_8h.md#define-cic_fraction_bits_min))`<br> |
| define  | [**CIC\_HEADROOM\_BITS\_WIDE**](cic__core_8h.md#define-cic_headroom_bits_wide)  `2u`<br> |
| define  | [**CIC\_N**](cic__core_8h.md#define-cic_n)  `4`<br> |
| define  | [**CIC\_PAPR\_16**](cic__core_8h.md#define-cic_papr_16)  `((float)(1u &lt;&lt; [**CIC\_HEADROOM\_BITS\_16**](cic__core_8h.md#define-cic_headroom_bits_16)))`<br> |
| define  | [**CIC\_PAPR\_WIDE**](cic__core_8h.md#define-cic_papr_wide)  `((float)(1u &lt;&lt; [**CIC\_HEADROOM\_BITS\_WIDE**](cic__core_8h.md#define-cic_headroom_bits_wide)))`<br> |
| define  | [**CIC\_PRECISION\_16**](cic__core_8h.md#define-cic_precision_16)  `16u`<br> |
| define  | [**CIC\_PRECISION\_32**](cic__core_8h.md#define-cic_precision_32)  `32u`<br> |
| define  | [**CIC\_PRECISION\_64**](cic__core_8h.md#define-cic_precision_64)  `64u`<br> |
| define  | [**CIC\_PRECISION\_DEFAULT**](cic__core_8h.md#define-cic_precision_default)  `[**CIC\_PRECISION\_64**](cic__core_8h.md#define-cic_precision_64)`<br> |
| define  | [**CIC\_REG\_BITS**](cic__core_8h.md#define-cic_reg_bits)  `64u`<br> |
| define  | [**CIC\_R\_MAX**](cic__core_8h.md#define-cic_r_max)  `2048u`<br>_Largest decimation ratio a CIC will be built at._  |
| define  | [**CIC\_STATE\_MAGIC**](cic__core_8h.md#define-cic_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('C', 'I', 'C', '\_')`<br> |
| define  | [**CIC\_STATE\_VERSION**](cic__core_8h.md#define-cic_state_version)  `5u`<br> |

## Detailed Description


**INPUT AMPLITUDE IS BOUNDED** by the filter's full scale, [**dp\_cic\_get\_full\_scale()**](cic__core_8h.md#function-dp_cic_get_full_scale): CIC\_PAPR\_WIDE times unit amplitude as built. A component at or beyond it is CLIPPED at the boundary, before any filtering happens. Unlike the library's floating-point blocks this one is not scale-free — it is the one place where turning the input gain up changes the answer — and the clip is silent in the sample stream: no error, no NaN, just a degraded output that looks plausible.


The input is an integer word with two ends, and a filter's **precision** ([**dp\_cic\_set\_precision()**](cic__core_8h.md#function-dp_cic_set_precision)) names the word:



|precision   |word   |full scale, as built    |
|-----|-----|-----|
|CIC\_PRECISION\_16   |CIC\_PRECISION\_16 bits   |CIC\_PAPR\_16    |
|CIC\_PRECISION\_32   |CIC\_PRECISION\_32 bits, or what fits   |CIC\_PAPR\_WIDE    |
|CIC\_PRECISION\_64 (as built)   |all the register has left   |CIC\_PAPR\_WIDE   |







* **The top is the headroom.** Full scale is a power of two above unit amplitude ([**dp\_cic\_set\_headroom\_bits()**](cic__core_8h.md#function-dp_cic_set_headroom_bits)), because a signal's peaks are above its average: by 4 dB for a root-raised-cosine symbol stream and by 10 to 12 dB for a multi-carrier one.
* **The bottom is the step,** [**dp\_cic\_get\_step()**](cic__core_8h.md#function-dp_cic_get_step): a component smaller than one step reads zero. The register holds the word and the pipeline's gain of `CIC_N * log2(R)` bits, so the widest word is `CIC_REG_BITS - CIC_N * log2(R)` bits; a precision that asks for more gets that. What the headroom does not take is precision, so each bit of headroom doubles the step.




\*\*So check `clipped**` — a sticky flag raised by any saturating component and cleared by [**dp\_cic\_reset()**](cic__core_8h.md#function-dp_cic_reset), following the same convention as the quantizing `cvt` converters (adc, f32\_to\_uq15, ...). It is free: the four boundary comparisons run on every sample regardless, so recording that one fired costs a register OR. There is no reason to run a CIC without checking it at least once against real input.


(Why the input is bounded at all: the pipeline is integer, so the CF32 boundary is quantized. `docs/design/cic.md` is the budget.)


Fixed design parameters: CIC\_N stages (~19.2 dB of alias rejection a stage at f\_p = 0.1 \* f\_out) one-sample combs (differential delay 1) R = power-of-two decimation ratio, up to CIC\_R\_MAX (enforced at create)


Input/output boundary: CF32 (`float _Complex`), matching the doppler default signal type. Internally each sample is a signed integer, the word above, held in a register modulo 2^CIC\_REG\_BITS. The pipeline's gain is `CIC_N * log2(R)` bits, and the largest sum the output decode forms is `(2^word - 1) * R^CIC_N`, which is under 2^CIC\_REG\_BITS for every word a precision gives. At CIC\_PRECISION\_64 the word and the gain are the register exactly: nothing is left over, and nothing needs to be, since the pipeline is exact while the true result fits, and it does.


**Every register is unsigned**, because the pipeline depends on its registers wrapping and in C only an unsigned type promises that (C99 6.2.5p9): a signed one that overflows is undefined. The sample goes in as its value modulo 2^CIC\_REG\_BITS (offset binary, then the offset off again, both unsigned), and comes out by adding the offset back at the pipeline's gain before the normalising shift, so what is shifted is never negative. The offset is carried around the pipeline and not through it, which is why a filter that has only seen silence holds zeros and gives zeros: its first outputs after create or reset are the filter filling with the input, and nothing else.


The unsigned modular-arithmetic CIC property guarantees exact outputs: every intermediate overflow in the integrators cancels in the comb stages, provided the true result fits in the register. So the integrator/comb pipeline itself needs no saturation, no range checks and no floating-point — the one saturation in the block is at the CF32 encoder, and it is the input bound described at the top of this file, not an arithmetic guard. A NaN component is taken as over the bound: it reads as the top of the range and raises `clipped`.


With one-sample combs and CIC\_N fixed, the entire comb state is CIC\_N registers per channel — no heap allocation beyond the state struct itself.


Alias rejection : ~77 dB at f\_p = 0.1 \* f\_out, CIC\_N stages (independent of R) Passband droop : ~0.57 dB at f\_p = 0.1 \* f\_out (independent of R) Output precision: the input word's ([**dp\_cic\_get\_bits()**](cic__core_8h.md#function-dp_cic_get_bits))



```C++
dp_cic_state_t *cic = dp_cic_create(16);   // R = 16
float _Complex  in[1024] = { 0 }, out[64];
size_t n_out = dp_cic_decimate(cic, in, 1024, out, 64);   // 64
dp_cic_destroy(cic);
```
 


    
## Public Functions Documentation




### function dp\_cic\_create 

_Create a CIC decimation filter of CIC\_N stages. Allocates the state struct on the heap and pre-computes the normalisation right-shift (CIC\_N \* log2(R) bits). All integrator and comb accumulators are zeroed; the first output arrives after R input samples. Returns NULL for invalid R or OOM. The filter is built at CIC\_PRECISION\_DEFAULT with that precision's headroom, so its input is bounded at CIC\_PAPR\_WIDE (_ [_**dp\_cic\_set\_precision()**_](cic__core_8h.md#function-dp_cic_set_precision) _,_[_**dp\_cic\_set\_headroom\_bits()**_](cic__core_8h.md#function-dp_cic_set_headroom_bits) _). A component at or beyond it is clipped at the boundary before any filtering; the sample stream gives no sign of it, so check the sticky_`clipped` _flag. Unlike doppler's floating-point blocks this one is not scale-free_ _scale the input into range first._
```C++
dp_cic_state_t * dp_cic_create (
    uint32_t R
) 
```





**Parameters:**


* `R` Decimation ratio. Must be a power of two from 2 to CIC\_R\_MAX. Returns NULL for R=0, non-power-of-two, or a ratio above that cap. 



**Returns:**

Heap-allocated state, or NULL on invalid R or OOM.



```C++
>>> from doppler.resample import CIC
>>> cic = CIC(R=16)
>>> cic.R, cic.shift
(16, 16)
```
 


        

<hr>



### function dp\_cic\_dc\_gain 

_The filter's response to a constant input, from its own geometry._ 
```C++
double dp_cic_dc_gain (
    const dp_cic_state_t * state
) 
```



A CIC's pipeline gain is `R^N`, and this implementation removes it with a right-shift of `N*log2(R)` bits, so the DC gain is `R^N / 2^shift` — one exactly, whenever the shift matches R. Computed from `R` and the stored shift rather than measured, so a mismatch between the two is visible without running a signal through the filter.




**Parameters:**


* `state` State. Must be non-NULL. 



**Returns:**

The DC gain. 1.0 for every power-of-two R the filter accepts.



```C++
dp_cic_state_t *c = dp_cic_create (32);
printf ("%.4f\n", dp_cic_dc_gain (c));   // 1.0000
dp_cic_destroy (c);
```
 


        

<hr>



### function dp\_cic\_decimate 

_Decimate a block of CF32 samples through the CIC pipeline. Each sample is converted to 16-bit Q15, pushed through CIC\_N integrators (unsigned wrapping), and when the phase counter reaches R the integrated value is passed through CIC\_N one-sample combs and converted back to CF32. State persists between calls. A fresh or reset filter starts from silence: its first CIC\_N - 1 outputs are the filter filling with the input, and silence in gives exact zeros out from the first one. Feeding blocks that are multiples of R gives predictable output counts (exactly n\_in/R samples per block)._ 
```C++
JM_FORCEINLINE  JM_HOT size_t dp_cic_decimate (
    dp_cic_state_t * state,
    const float _Complex * in,
    size_t n_in,
    float _Complex * out,
    size_t max_out
) 
```





**Note:**

**Input amplitude is bounded: \|Re\| and \|Im\| &lt;= 2.0.** A component beyond +-2.0 is clipped at the boundary before filtering; the sample stream gives no sign of it, so check the sticky `clipped` flag. Scale the input into range first; see the file header.




**Parameters:**


* `state` Pointer to a valid [**dp\_cic\_state\_t**](structdp__cic__state__t.md). 
* `in` CF32 input block, \|Re\| and \|Im\| &lt;= 2.0 (clipped otherwise). 
* `n_in` Number of input samples. 
* `out` Output buffer; must hold at least max\_out elements. 
* `max_out` Capacity of `out` in samples. Normally n\_in (the loosest bound: at most one output per input). If it is smaller the integrators and combs still advance over every input sample  the pipeline is a running filter and cannot be left half-fed  but emission stops, so the samples past the capacity are dropped rather than written past the end. 



**Returns:**

Number of CF32 samples written to `out:` min(floor((phase + n\_in) / R), max\_out), where phase is the input count carried from the previous call.



```C++
>>> from doppler.resample import CIC
>>> import numpy as np
>>> cic = CIC(R=16)
>>> y = cic.decimate(np.zeros(64, dtype=np.complex64))
>>> y.tolist(), y.dtype                 # silence in, zeros from the first
([0j, 0j, 0j, 0j], dtype('complex64'))
>>> ones = cic.decimate(np.ones(96, dtype=np.complex64))
>>> [round(float(v.real), 3) for v in ones]  # then the filter fills
[0.059, 0.562, 0.972, 1.0, 1.0, 1.0]
```
 


        

<hr>



### function dp\_cic\_decimate\_max\_out 

_Upper bound on decimate output — returns 0 (lazy-alloc signal)._ 
```C++
size_t dp_cic_decimate_max_out (
    dp_cic_state_t * state
) 
```



The Python extension allocates n\_in elements on the first call. Since n\_in &gt;= ceil(n\_in/R) = n\_out for all R &gt;= 1, the buffer is always large enough as long as block size stays consistent.




**Parameters:**


* `state` CIC state (unused). 



**Returns:**

Always 0, meaning "no fixed cap": size the output to the input length. 





        

<hr>



### function dp\_cic\_destroy 

_Free the filter state._ 
```C++
void dp_cic_destroy (
    dp_cic_state_t * state
) 
```





**Parameters:**


* `state` State from [**dp\_cic\_create()**](cic__core_8h.md#function-dp_cic_create); NULL is a no-op. 




        

<hr>



### function dp\_cic\_get\_bits 

_The input word's width in bits, sign included._ 
```C++
uint32_t dp_cic_get_bits (
    const dp_cic_state_t * state
) 
```





**Parameters:**


* `state` Pointer to a valid [**dp\_cic\_state\_t**](structdp__cic__state__t.md). 



**Returns:**

The precision, or `CIC_REG_BITS - CIC_N * log2(R)` when the register has less than that left. 





        

<hr>



### function dp\_cic\_get\_full\_scale 

_The input bound: a component at or beyond it is clipped._ 
```C++
double dp_cic_get_full_scale (
    const dp_cic_state_t * state
) 
```





**Parameters:**


* `state` Pointer to a valid [**dp\_cic\_state\_t**](structdp__cic__state__t.md). 



**Returns:**

`2^headroom_bits`, as a voltage ratio over unit amplitude. 





        

<hr>



### function dp\_cic\_get\_group\_delay 

_The decimator's group delay, in its INPUT samples: where in the input timeline an output sample's content sits._ 
```C++
double dp_cic_get_group_delay (
    const dp_cic_state_t * state
) 
```



Output `k` of a fresh stream carries the input at index `k * R - group_delay`. From the structure, not measured: the response is CIC\_N boxcars of `R` inputs in a row, `CIC_N * (R - 1) + 1` inputs long and symmetric, so its center is `CIC_N * (R - 1) / 2` behind the newest input; and output `k` is formed as input `k * R + R - 1` arrives, which is `R - 1` after the instant `k * R`. The difference is the delay, the same at every frequency.




**Parameters:**


* `state` State. Must be non-NULL. 



**Returns:**

`(R - 1) * (CIC_N - 2) / 2`, in input samples: `R - 1` at CIC\_N 4.



```C++
dp_cic_state_t *c = dp_cic_create (8);
printf ("%.1f\n", dp_cic_get_group_delay (c));   // 7.0
dp_cic_destroy (c);
```
 


        

<hr>



### function dp\_cic\_get\_headroom\_bits 

_The headroom in use, in bits._ 
```C++
uint32_t dp_cic_get_headroom_bits (
    const dp_cic_state_t * state
) 
```





**Parameters:**


* `state` Pointer to a valid [**dp\_cic\_state\_t**](structdp__cic__state__t.md). 



**Returns:**

Bits of the input word above unit amplitude. 





        

<hr>



### function dp\_cic\_get\_precision 

_The precision in use._ 
```C++
uint32_t dp_cic_get_precision (
    const dp_cic_state_t * state
) 
```





**Parameters:**


* `state` Pointer to a valid [**dp\_cic\_state\_t**](structdp__cic__state__t.md). 



**Returns:**

CIC\_PRECISION\_16, CIC\_PRECISION\_32 or CIC\_PRECISION\_64. 





        

<hr>



### function dp\_cic\_get\_state 

_Serialize R, the integrator/comb/phase state and_ `clipped` _into_`blob` _. shift is not written: it is derived from R._
```C++
void dp_cic_get_state (
    const dp_cic_state_t * state,
    void * blob
) 
```





**Parameters:**


* `state` Pointer to a valid [**dp\_cic\_state\_t**](structdp__cic__state__t.md). 
* `blob` Destination; at least [**dp\_cic\_state\_bytes()**](cic__core_8h.md#function-dp_cic_state_bytes) bytes. 




        

<hr>



### function dp\_cic\_get\_step 

_The input's step: a component smaller than this reads zero, and the output is a whole number of them._ 
```C++
double dp_cic_get_step (
    const dp_cic_state_t * state
) 
```





**Parameters:**


* `state` Pointer to a valid [**dp\_cic\_state\_t**](structdp__cic__state__t.md). 



**Returns:**

`full_scale / 2^(bits - 1)`, in the input's units. 





        

<hr>



### function dp\_cic\_headroom\_bits\_ok 

_Is this a headroom a CIC takes: 0 to CIC\_HEADROOM\_BITS\_MAX bits._ 
```C++
int dp_cic_headroom_bits_ok (
    uint32_t bits
) 
```



The ONE predicate: [**dp\_cic\_set\_headroom\_bits()**](cic__core_8h.md#function-dp_cic_set_headroom_bits) and [**dp\_cic\_set\_state()**](cic__core_8h.md#function-dp_cic_set_state) refuse outside it, and a composition that takes the figure from a caller checks it first.




**Parameters:**


* `bits` Bits of the input word above unit amplitude. 



**Returns:**

1 inside the domain, 0 outside it. 





        

<hr>



### function dp\_cic\_precision\_ok 

_Is this a precision: CIC\_PRECISION\_16, CIC\_PRECISION\_32 or CIC\_PRECISION\_64._ 
```C++
int dp_cic_precision_ok (
    uint32_t precision
) 
```





**Parameters:**


* `precision` The candidate. 



**Returns:**

1 for one of the three, 0 otherwise. 





        

<hr>



### function dp\_cic\_reconfigure 

_Change the decimation ratio in place and reset all filter state. Recomputes the normalisation shift (CIC\_N \* log2(R)) and zeros all accumulators so the filter behaves exactly like a freshly created one with the new R. Silently ignores R values that are not a power-of-two in_ `[2, 2048]` _(_`CIC_R_MAX` _) — the state is left unchanged in that case._
```C++
void dp_cic_reconfigure (
    dp_cic_state_t * state,
    uint32_t R
) 
```





**Parameters:**


* `state` Pointer to a valid [**dp\_cic\_state\_t**](structdp__cic__state__t.md). 
* `R` New decimation ratio. Same constraints as [**dp\_cic\_create()**](cic__core_8h.md#function-dp_cic_create).


```C++
>>> from doppler.resample import CIC
>>> cic = CIC(R=4)
>>> cic.reconfigure(8)
>>> cic.R, cic.shift
(8, 12)
```
 


        

<hr>



### function dp\_cic\_reset 

_Zero all integrator and comb accumulators; preserve R and shift. The first output sample after reset arrives after R more input samples, matching post-create behaviour. Use between signal bursts to eliminate transient artefacts caused by residual pipeline state. Also clears the sticky_ `clipped` _flag._
```C++
void dp_cic_reset (
    dp_cic_state_t * state
) 
```





**Parameters:**


* `state` Pointer to a valid [**dp\_cic\_state\_t**](structdp__cic__state__t.md).


```C++
>>> from doppler.resample import CIC
>>> cic = CIC(R=16)
>>> cic.reset()
>>> cic.R
16
```
 


        

<hr>



### function dp\_cic\_set\_headroom\_bits 

_Set how much of the input word is headroom, and reset the filter._ 
```C++
int dp_cic_set_headroom_bits (
    dp_cic_state_t * state,
    uint32_t bits
) 
```



`bits` of the word sit above unit amplitude, so full scale is `2^bits` ([**dp\_cic\_get\_full\_scale()**](cic__core_8h.md#function-dp_cic_get_full_scale)), and the rest is precision, so the step is `2^bits / 2^(word - 1)` ([**dp\_cic\_get\_step()**](cic__core_8h.md#function-dp_cic_get_step)): every bit of headroom is 6 dB more room for peaks and 6 dB more of step. [**dp\_cic\_set\_precision()**](cic__core_8h.md#function-dp_cic_set_precision) puts it back to the precision's own.


The registers hold samples at the old scale, so the filter is reset. A `bits` outside [**dp\_cic\_headroom\_bits\_ok()**](cic__core_8h.md#function-dp_cic_headroom_bits_ok) is refused and nothing changes.




**Parameters:**


* `state` Pointer to a valid [**dp\_cic\_state\_t**](structdp__cic__state__t.md). 
* `bits` Bits of headroom, 0 to CIC\_HEADROOM\_BITS\_MAX. 



**Returns:**

DP\_OK, or DP\_ERR\_INVALID with nothing changed.



```C++
>>> from doppler.resample import CIC
>>> cic = CIC(R=64)
>>> cic.headroom_bits, cic.full_scale
(2, 4.0)
>>> cic.headroom_bits = 4       # 24 dB
>>> cic.full_scale, cic.step
(16.0, 2.9103830456733704e-11)
```
 


        

<hr>



### function dp\_cic\_set\_precision 

_Choose the input word, with that word's headroom, and reset the filter._ 
```C++
int dp_cic_set_precision (
    dp_cic_state_t * state,
    uint32_t precision
) 
```



CIC\_PRECISION\_16 is a 16-bit word with full scale CIC\_PAPR\_16: the filter as it was before it had a precision, output for output. CIC\_PRECISION\_32 is a 32-bit word and CIC\_PRECISION\_64 the widest the register has left, both with full scale CIC\_PAPR\_WIDE; where the ratio's gain leaves less than 32 bits the two are the same word ([**dp\_cic\_get\_bits()**](cic__core_8h.md#function-dp_cic_get_bits)). The headroom is then [**dp\_cic\_set\_headroom\_bits()**](cic__core_8h.md#function-dp_cic_set_headroom_bits)'s to change.


The registers hold samples at the old scale, so the filter is reset, as [**dp\_cic\_reconfigure()**](cic__core_8h.md#function-dp_cic_reconfigure) resets it. A `precision` that is not one of the three is refused and nothing changes.




**Parameters:**


* `state` Pointer to a valid [**dp\_cic\_state\_t**](structdp__cic__state__t.md). 
* `precision` CIC\_PRECISION\_16, CIC\_PRECISION\_32 or CIC\_PRECISION\_64. 



**Returns:**

DP\_OK, or DP\_ERR\_INVALID with nothing changed.



```C++
>>> from doppler.resample import CIC
>>> cic = CIC(R=64)
>>> cic.precision, cic.bits, cic.full_scale, cic.step
(64, 40, 4.0, 7.275957614183426e-12)
>>> cic.precision = 32
>>> cic.bits, cic.full_scale, cic.step
(32, 4.0, 1.862645149230957e-09)
>>> cic.precision = 16
>>> cic.bits, cic.full_scale, cic.step
(16, 2.0, 6.103515625e-05)
```
 


        

<hr>



### function dp\_cic\_set\_state 

_Restore R, the integrator/comb/phase state and_ `clipped` _from_`blob` _._
```C++
int dp_cic_set_state (
    dp_cic_state_t * state,
    const void * blob
) 
```



R is the writer's, as [**dp\_cic\_reconfigure()**](cic__core_8h.md#function-dp_cic_reconfigure) would set it, and shift is recomputed from it, so a blob restores into a CIC built with any R (doppler#2642). A blob whose R is not one [**dp\_cic\_create()**](cic__core_8h.md#function-dp_cic_create) accepts, or whose phase is not below that R, is refused and `state` is unchanged.




**Parameters:**


* `state` Pointer to a valid [**dp\_cic\_state\_t**](structdp__cic__state__t.md). 
* `blob` A blob from [**dp\_cic\_get\_state()**](cic__core_8h.md#function-dp_cic_get_state). 



**Returns:**

DP\_OK, or DP\_ERR\_INVALID if the envelope rejects, R is invalid or the phase is not below R. 





        

<hr>



### function dp\_cic\_state\_bytes 

_Bytes_ [_**dp\_cic\_get\_state()**_](cic__core_8h.md#function-dp_cic_get_state) _writes (envelope + payload)._
```C++
size_t dp_cic_state_bytes (
    const dp_cic_state_t * state
) 
```





**Parameters:**


* `state` CIC state (unused: the size is fixed). 



**Returns:**

`sizeof(dp_state_hdr_t) + 4 + 4 * CIC_N * 8 + 4 + 1` bytes, the same for every R. 





        

<hr>
## Macro Definition Documentation





### define CIC\_FRACTION\_BITS\_MIN 

```C++
#define CIC_FRACTION_BITS_MIN `1u`
```



A word keeps its sign and at least this many bits under unity. 


        

<hr>



### define CIC\_HEADROOM\_BITS\_16 

```C++
#define CIC_HEADROOM_BITS_16 `1u`
```



Bits of headroom CIC\_PRECISION\_16 starts with: full scale CIC\_PAPR\_16. 


        

<hr>



### define CIC\_HEADROOM\_BITS\_MAX 

```C++
#define CIC_HEADROOM_BITS_MAX `( CIC_PRECISION_16 - 1u - CIC_FRACTION_BITS_MIN )`
```



The most headroom any word takes: the narrowest word, CIC\_PRECISION\_16 bits, less its sign and CIC\_FRACTION\_BITS\_MIN. 


        

<hr>



### define CIC\_HEADROOM\_BITS\_WIDE 

```C++
#define CIC_HEADROOM_BITS_WIDE `2u`
```



Bits of headroom CIC\_PRECISION\_32 and CIC\_PRECISION\_64 start with: full scale CIC\_PAPR\_WIDE, 12 dB, a multi-carrier signal's peaks. 


        

<hr>



### define CIC\_N 

```C++
#define CIC_N `4`
```



Fixed stage count. Alias rejection ~19.2 dB/stage at f\_p=0.1. 


        

<hr>



### define CIC\_PAPR\_16 

```C++
#define CIC_PAPR_16 `((float)(1u << CIC_HEADROOM_BITS_16 ))`
```



Full scale at CIC\_HEADROOM\_BITS\_16, as a voltage ratio. 


        

<hr>



### define CIC\_PAPR\_WIDE 

```C++
#define CIC_PAPR_WIDE `((float)(1u << CIC_HEADROOM_BITS_WIDE ))`
```



Full scale at CIC\_HEADROOM\_BITS\_WIDE, as a voltage ratio. 


        

<hr>



### define CIC\_PRECISION\_16 

```C++
#define CIC_PRECISION_16 `16u`
```



A 16-bit word at every ratio. With CIC\_PAPR\_16 it is the filter this one was before it had a precision, output for output. 


        

<hr>



### define CIC\_PRECISION\_32 

```C++
#define CIC_PRECISION_32 `32u`
```



A 32-bit word, or the widest the register has left when the ratio's gain leaves less than that (past R = 256). 


        

<hr>



### define CIC\_PRECISION\_64 

```C++
#define CIC_PRECISION_64 `64u`
```



The widest word the register has left: `CIC_REG_BITS - CIC_N * log2(R)` bits. What [**dp\_cic\_create()**](cic__core_8h.md#function-dp_cic_create) builds. 


        

<hr>



### define CIC\_PRECISION\_DEFAULT 

```C++
#define CIC_PRECISION_DEFAULT `CIC_PRECISION_64`
```



The precision [**dp\_cic\_create()**](cic__core_8h.md#function-dp_cic_create) builds. 


        

<hr>



### define CIC\_REG\_BITS 

```C++
#define CIC_REG_BITS `64u`
```



Bits in a pipeline register: the input word and the pipeline's gain of `CIC_N * log2(R)` bits both come out of these. 


        

<hr>



### define CIC\_R\_MAX 

_Largest decimation ratio a CIC will be built at._ 
```C++
#define CIC_R_MAX `2048u`
```



The register is CIC\_REG\_BITS bits and the pipeline's gain takes `CIC_N * log2(R)` of them. At the cap that leaves a word of CIC\_REG\_BITS - CIC\_N \* log2(CIC\_R\_MAX) bits, still wider than CIC\_PRECISION\_16; one doubling further it would be CIC\_PRECISION\_16 exactly, with nothing for the wider precisions to add. Past the cap the planner hands the residual to the resampler stage. 


        

<hr>



### define CIC\_STATE\_MAGIC 

```C++
#define CIC_STATE_MAGIC `DP_FOURCC ('C', 'I', 'C', '_')`
```



FourCC type tag ("CIC\_") stamped in a CIC state blob's header. 


        

<hr>



### define CIC\_STATE\_VERSION 

```C++
#define CIC_STATE_VERSION `5u`
```



Layout version of the CIC state blob (2 added `clipped`, 3 added R; 4: the registers hold the sample without its offset; 5: the precision and the headroom travel, and the word is theirs). 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/cic/cic_core.h`

