

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
|  [**cic\_state\_t**](structcic__state__t.md) \* | [**cic\_create**](#function-cic_create) (uint32\_t R) <br>_Create a 4-stage, M=1 CIC decimation filter. Allocates the state struct on the heap and pre-computes the normalisation right-shift (CIC\_N \* log2(R) bits). All integrator and comb accumulators are zeroed; the first output arrives after R input samples. Returns NULL for invalid R or OOM. Input amplitude is bounded: \|Re\| and \|Im\| &lt;= 1.0. A component beyond +-1.0 is clipped at the boundary before any filtering; the sample stream gives no sign of it, so check the sticky_ `clipped` _flag. Unlike doppler's floating-point blocks this one is not scale-free_ _scale the input into range first._ |
|  double | [**cic\_dc\_gain**](#function-cic_dc_gain) (const [**cic\_state\_t**](structcic__state__t.md) \* state) <br>_The filter's response to a constant input, from its own geometry._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) size\_t | [**cic\_decimate**](#function-cic_decimate) ([**cic\_state\_t**](structcic__state__t.md) \* state, const float \_Complex \* in, size\_t n\_in, float \_Complex \* out, size\_t max\_out) <br>_Decimate a block of CF32 samples through the CIC pipeline. Each sample is converted to offset-binary UQ16, pushed through CIC\_N integrators (unsigned wrapping), and when the phase counter reaches R the integrated value is passed through CIC\_N M=1 comb stages and converted back to CF32. State persists between calls. Feeding blocks that are multiples of R gives predictable output counts (exactly n\_in/R samples per block)._  |
|  size\_t | [**cic\_decimate\_max\_out**](#function-cic_decimate_max_out) ([**cic\_state\_t**](structcic__state__t.md) \* state) <br>_Upper bound on decimate output — returns 0 (lazy-alloc signal)._  |
|  void | [**cic\_destroy**](#function-cic_destroy) ([**cic\_state\_t**](structcic__state__t.md) \* state) <br> |
|  void | [**cic\_get\_state**](#function-cic_get_state) (const [**cic\_state\_t**](structcic__state__t.md) \* state, void \* blob) <br>_Serialize the integrator/comb/phase state into_ `blob` _._ |
|  void | [**cic\_reconfigure**](#function-cic_reconfigure) ([**cic\_state\_t**](structcic__state__t.md) \* state, uint32\_t R) <br>_Change the decimation ratio in place and reset all filter state. Recomputes the normalisation shift (CIC\_N \* log2(R)) and zeros all accumulators so the filter behaves exactly like a freshly created one with the new R. Silently ignores R values that are not a power-of-two in_ `[2, 2048]` _(_`CIC_R_MAX` _) — the state is left unchanged in that case._ |
|  void | [**cic\_reset**](#function-cic_reset) ([**cic\_state\_t**](structcic__state__t.md) \* state) <br>_Zero all integrator and comb accumulators; preserve R and shift. The first output sample after reset arrives after R more input samples, matching post-create behaviour. Use between signal bursts to eliminate transient artefacts caused by residual pipeline state._  |
|  int | [**cic\_set\_state**](#function-cic_set_state) ([**cic\_state\_t**](structcic__state__t.md) \* state, const void \* blob) <br>_Restore the integrator/comb/phase state from_ `blob` _._ |
|  size\_t | [**cic\_state\_bytes**](#function-cic_state_bytes) (const [**cic\_state\_t**](structcic__state__t.md) \* state) <br>_Bytes_ [_**cic\_get\_state()**_](cic__core_8h.md#function-cic_get_state) _writes (envelope + payload)._ |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**CIC\_N**](cic__core_8h.md#define-cic_n)  `4`<br> |
| define  | [**CIC\_PAPR\_HEADROOM**](cic__core_8h.md#define-cic_papr_headroom)  `2.0f`<br>_Peak-to-average headroom the input encoding reserves, as a voltage ratio (2.0 = 6 dB)._  |
| define  | [**CIC\_R\_MAX**](cic__core_8h.md#define-cic_r_max)  `2048u`<br>_Largest decimation ratio a CIC will be built at._  |
| define  | [**CIC\_STATE\_MAGIC**](cic__core_8h.md#define-cic_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('C', 'I', 'C', '\_')`<br> |
| define  | [**CIC\_STATE\_VERSION**](cic__core_8h.md#define-cic_state_version)  `2u`<br> |

## Detailed Description


**INPUT AMPLITUDE IS BOUNDED: \|Re\| and \|Im\| &lt;= 2.0.** A component beyond that is CLIPPED at the boundary, before any filtering happens. Unlike the library's floating-point blocks this one is not scale-free — it is the one place where turning the input gain up changes the answer — and the clip is silent in the sample stream: no error, no NaN, just a degraded output that looks plausible. Measured cost: an RRC-BPSK waveform driven into the clip matched-filters to -25 dB EVM where the same waveform well inside it reaches -50 dB.


The bound is `CIC_PAPR_HEADROOM` (2.0, i.e. 6 dB) and not 1.0 because that headroom is exactly what it buys: the encode scale is `32768 / CIC_PAPR_HEADROOM`, so full scale sits 6 dB above unity amplitude and a signal whose PEAKS exceed its unit average — every pulse-shaped waveform — has somewhere to put them. Budgeting the DC gain alone left the peaks clipping against a bound the average never approached.


\*\*So check `clipped**` — a sticky flag raised by any saturating component and cleared only by [**cic\_reset()**](cic__core_8h.md#function-cic_reset), following the same convention as the quantizing `cvt` converters (adc, f32\_to\_uq15, ...). It is free: the four boundary comparisons run on every sample regardless, so recording that one fired costs a register OR. There is no reason to run a CIC without checking it at least once against real input.


(Why the input is bounded at all: the pipeline is integer, so the CF32 boundary is quantized. That is an implementation detail — the input constraint above is the whole of what a caller needs. See `docs/design/QUANTIZATION.md` for the encoding and the headroom budget.)


Fixed design parameters: N = 4 stages (~77 dB alias rejection at f\_p = 0.1 \* f\_out) M = 1 (differential delay — one-sample comb) R = power-of-two decimation ratio (enforced at create time)


Input/output boundary: CF32 (`float _Complex`), matching the doppler default signal type. Internally, each sample is converted to UQ16 — offset-binary: v\_q15 + 32768 → `[0, 65535]` in a uint64\_t — giving 48 bits of headroom for the pipeline gain of N \* log2(R) bits. At the cap `CIC_R_MAX` (2048, log2 = 11) the gain is 44 bits; max accumulation = 65535 \* R^N = (2^16 - 1) \* 2^44, which is 16x inside 2^64. R = 4096 also fits, but to within one part in 65536 — see CIC\_R\_MAX for why the cap is a halving below it.


All arithmetic is unsigned: inputs are non-negative `[0, 65535]`, wrapping is defined (mod 2^64), and the output decode subtracts the offset in floating-point — no signed integer casts anywhere in the hot path.


The unsigned modular-arithmetic CIC property guarantees exact outputs: every intermediate overflow in the integrators cancels in the comb stages, provided the true result fits in 64 bits. So the integrator/comb pipeline itself needs no saturation, no range checks and no floating-point — the one saturation in the block is at the CF32 encoder, and it is the +-1.0 input bound described at the top of this file, not an arithmetic guard.


With M=1 and N fixed, the entire comb state is four uint64\_t values per channel — no heap allocation beyond the state struct itself.


Alias rejection : ~77 dB at f\_p = 0.1 \* f\_out (independent of R) Passband droop : ~0.57 dB at f\_p = 0.1 \* f\_out (independent of R) Output precision: 16-bit Q15 (independent of R and N)



```C++
cic_state_t *cic = cic_create(16);   // R=16, N=4, M=1
size_t n_out = cic_decimate(cic, in, 1024, out, 1024);
cic_destroy(cic);
```
 


    
## Public Functions Documentation




### function cic\_create 

_Create a 4-stage, M=1 CIC decimation filter. Allocates the state struct on the heap and pre-computes the normalisation right-shift (CIC\_N \* log2(R) bits). All integrator and comb accumulators are zeroed; the first output arrives after R input samples. Returns NULL for invalid R or OOM. Input amplitude is bounded: \|Re\| and \|Im\| &lt;= 1.0. A component beyond +-1.0 is clipped at the boundary before any filtering; the sample stream gives no sign of it, so check the sticky_ `clipped` _flag. Unlike doppler's floating-point blocks this one is not scale-free_ _scale the input into range first._
```C++
cic_state_t * cic_create (
    uint32_t R
) 
```





**Parameters:**


* `R` Decimation ratio. Must be a power of two in `[2, 2048]` (`CIC_R_MAX`). Returns NULL for R=0, non-power-of-two, or a ratio above that cap. 



**Returns:**

Heap-allocated state, or NULL on invalid R or OOM.



```C++
>>> from doppler.resample import CIC
>>> cic = CIC(R=16)
>>> cic.R, cic.shift
(16, 16)
```
 


        

<hr>



### function cic\_dc\_gain 

_The filter's response to a constant input, from its own geometry._ 
```C++
double cic_dc_gain (
    const cic_state_t * state
) 
```



A CIC's pipeline gain is `R^N`, and this implementation removes it with a right-shift of `N*log2(R)` bits, so the DC gain is `R^N / 2^shift` — one exactly, whenever the shift matches R. Computed from `R` and the stored shift rather than measured, so a mismatch between the two is visible without running a signal through the filter.




**Parameters:**


* `state` State. Must be non-NULL. 



**Returns:**

The DC gain. 1.0 for every power-of-two R the filter accepts.



```C++
cic_state_t *c = cic_create (32);
printf ("%.4f\n", cic_dc_gain (c));   // 1.0000
cic_destroy (c);
```
 


        

<hr>



### function cic\_decimate 

_Decimate a block of CF32 samples through the CIC pipeline. Each sample is converted to offset-binary UQ16, pushed through CIC\_N integrators (unsigned wrapping), and when the phase counter reaches R the integrated value is passed through CIC\_N M=1 comb stages and converted back to CF32. State persists between calls. Feeding blocks that are multiples of R gives predictable output counts (exactly n\_in/R samples per block)._ 
```C++
JM_FORCEINLINE  JM_HOT size_t cic_decimate (
    cic_state_t * state,
    const float _Complex * in,
    size_t n_in,
    float _Complex * out,
    size_t max_out
) 
```





**Note:**

**Input amplitude is bounded: \|Re\| and \|Im\| &lt;= 1.0.** A component beyond +-1.0 is clipped at the boundary before filtering; the sample stream gives no sign of it, so check the sticky `clipped` flag. Scale the input into range first; see the file header.




**Parameters:**


* `state` Pointer to a valid [**cic\_state\_t**](structcic__state__t.md). 
* `in` CF32 input block, \|Re\| and \|Im\| &lt;= 1.0 (clipped otherwise). 
* `n_in` Number of input samples. 
* `out` Output buffer; must hold at least max\_out elements. 
* `max_out` Capacity of `out` in samples. Normally n\_in (the loosest bound: at most one output per input). If it is smaller the integrators and combs still advance over every input sample  the pipeline is a running filter and cannot be left half-fed  but emission stops, so the samples past the capacity are dropped rather than written past the end. 



**Returns:**

CF32 output array; length is min(floor((phase + n\_in) / R), max\_out).



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

_Change the decimation ratio in place and reset all filter state. Recomputes the normalisation shift (CIC\_N \* log2(R)) and zeros all accumulators so the filter behaves exactly like a freshly created one with the new R. Silently ignores R values that are not a power-of-two in_ `[2, 2048]` _(_`CIC_R_MAX` _) — the state is left unchanged in that case._
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



### define CIC\_PAPR\_HEADROOM 

_Peak-to-average headroom the input encoding reserves, as a voltage ratio (2.0 = 6 dB)._ 
```C++
#define CIC_PAPR_HEADROOM `2.0f`
```



A fixed-point CIC has TWO input-budget terms, and only one of them is the accumulator's. The **DC gain** `R^N` is budgeted there — 16-bit input plus 48 bits of pipeline gain would fill the 64-bit accumulator exactly at R = 4096, which is why CIC\_R\_MAX sits a halving below it. The **PAPR** is budgeted here, at the encoder, because a signal's peak is not its symbol amplitude: a root-raised-cosine symbol stream peaks at **1.582x** its symbol amplitude (measured, and a pulse property — identical at every samples-per-symbol).


Encoding at full scale therefore clipped any signal presented at its natural amplitude, and the caller had to back off by the PAPR — 4 dB that nothing downstream restored, leaving a timing loop under-driven by the square of it, 2.5x. Reserving the headroom here instead lets a unit- amplitude signal through unclipped and costs 2 dB of quantisation SNR against a caller who backs off perfectly, which no caller did.


This changes only the encode/decode scale PAIR, never the normalising shift, so the DC gain stays exactly one — see [**cic\_dc\_gain()**](cic__core_8h.md#function-cic_dc_gain). 


        

<hr>



### define CIC\_R\_MAX 

_Largest decimation ratio a CIC will be built at._ 
```C++
#define CIC_R_MAX `2048u`
```



The 64-bit accumulator holds `65535 * R^CIC_N`. At R = 4096 that is `(2^16 - 1) * 2^48 = 2^64 - 2^48` — it fits, and fills the accumulator to **within one part in 65536**. "Fits exactly" is not headroom: it is the value at which any further term overflows, and the CIC's exactness argument (every intermediate overflow cancels in the combs) holds only while the TRUE result fits in 64 bits.


2048 gives `2^60 - 2^44` — **16x margin** — for one halving of the largest single-stage ratio. Nothing in the tree asked for more: the planner is the only thing that can reach the cap, and past it it already hands the residual to the resampler stage, so a lower cap costs a slightly larger residual and nothing else. 


        

<hr>



### define CIC\_STATE\_MAGIC 

```C++
#define CIC_STATE_MAGIC `DP_FOURCC ('C', 'I', 'C', '_')`
```




<hr>



### define CIC\_STATE\_VERSION 

```C++
#define CIC_STATE_VERSION `2u`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/cic/cic_core.h`

