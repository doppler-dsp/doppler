

# File cic\_core.h



[**FileList**](files.md) **>** [**cic**](dir_cf560077cc62991e7289ea57a3d930a1.md) **>** [**cic\_core.h**](cic__core_8h.md)

[Go to the source code of this file](cic__core_8h_source.md)

_CIC decimation filter — 4-stage, M=1, UQ16 integer pipeline._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "jm_perf.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**cic\_state\_t**](structcic__state__t.md) <br>_CIC filter state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**cic\_state\_t**](structcic__state__t.md) \* | [**cic\_create**](#function-cic_create) (uint32\_t R) <br>_Create a CIC decimation filter._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) size\_t | [**cic\_decimate**](#function-cic_decimate) ([**cic\_state\_t**](structcic__state__t.md) \* state, const float complex \* in, size\_t n\_in, float complex \* out) <br>_Decimate n\_in CF32 samples; write output to out._  |
|  size\_t | [**cic\_decimate\_max\_out**](#function-cic_decimate_max_out) ([**cic\_state\_t**](structcic__state__t.md) \* state) <br>_Upper bound on decimate output — returns 0 (lazy-alloc signal)._  |
|  void | [**cic\_destroy**](#function-cic_destroy) ([**cic\_state\_t**](structcic__state__t.md) \* state) <br> |
|  void | [**cic\_reconfigure**](#function-cic_reconfigure) ([**cic\_state\_t**](structcic__state__t.md) \* state, uint32\_t R) <br>_Change the decimation ratio in place; resets all filter state._  |
|  void | [**cic\_reset**](#function-cic_reset) ([**cic\_state\_t**](structcic__state__t.md) \* state) <br>_Zero all filter state; preserve R and shift._  |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**CIC\_N**](cic__core_8h.md#define-cic_n)  `4`<br> |

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

_Create a CIC decimation filter._ 
```C++
cic_state_t * cic_create (
    uint32_t R
) 
```





**Parameters:**


* `R` Decimation ratio. Must be a power of two in `[2, 4096]`. Returns NULL for R=0, non-power-of-two, or R &gt; 4096. 



**Returns:**

Heap-allocated state, or NULL on invalid R or OOM. 





        

<hr>



### function cic\_decimate 

_Decimate n\_in CF32 samples; write output to out._ 
```C++
JM_FORCEINLINE  JM_HOT size_t cic_decimate (
    cic_state_t * state,
    const float complex * in,
    size_t n_in,
    float complex * out
) 
```



Each sample is converted to UQ16, run through CIC\_N integrators, tested against the decimation phase, then (if a decimation boundary) passed through CIC\_N comb stages and converted back to CF32.




**Parameters:**


* `state` Must be non-NULL. 
* `in` CF32 input array, length n\_in. 
* `n_in` Number of input samples. 
* `out` Output buffer; must hold at least ceil((state-&gt;phase + n\_in) / state-&gt;R) elements. 



**Returns:**

Number of output samples written. 





        

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



### function cic\_reconfigure 

_Change the decimation ratio in place; resets all filter state._ 
```C++
void cic_reconfigure (
    cic_state_t * state,
    uint32_t R
) 
```



Silently ignores invalid R (non-power-of-two, out of range).




**Parameters:**


* `state` Filter state to reconfigure. Must be non-NULL. 
* `R` New decimation ratio. Same constraints as [**cic\_create()**](cic__core_8h.md#function-cic_create). 




        

<hr>



### function cic\_reset 

_Zero all filter state; preserve R and shift._ 
```C++
void cic_reset (
    cic_state_t * state
) 
```



The first output sample after reset is produced on input sample R-1, matching post-create behaviour. 


        

<hr>
## Macro Definition Documentation





### define CIC\_N 

```C++
#define CIC_N `4`
```



Fixed stage count. Alias rejection ~19.2 dB/stage at f\_p=0.1. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/cic/cic_core.h`

