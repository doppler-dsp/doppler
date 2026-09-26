

# File fir\_core.h



[**FileList**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**fir**](dir_057b0102e9f7b23965a3d08d49a38aec.md) **>** [**fir\_core.h**](fir__core_8h.md)

[Go to the source code of this file](fir__core_8h_source.md)

_Direct-form FIR filter — real-tap and complex-tap variants._ [More...](#detailed-description)

* `#include "doppler/clib_common.h"`
* `#include "doppler/dp_state.h"`
* `#include "doppler/jm_perf.h"`
* `#include "doppler/dp_complex.h"`
* `#include <stddef.h>`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**dp\_fir\_state\_t**](structdp__fir__state__t.md) <br> |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**dp\_fir\_state\_t**](structdp__fir__state__t.md) \* | [**dp\_fir\_create**](#function-dp_fir_create) (const float \_Complex \* taps, size\_t taps\_len) <br>_Create a FIR filter from complex CF32 tap coefficients. Implements a direct-form FIR convolution:_ `y[n]` _= sum\_k_`h[k]` _\*_`x[n-k]` _. The tap array is copied at creation; the caller may free it afterward. Use_[_**fir\_create\_real()**_](fir__core_8h.md#function-fir_create_real) _instead when all imaginary parts are zero — that path costs 1 FMA/tap versus 2 FMA + permute + mul here._ |
|  void | [**dp\_fir\_destroy**](#function-dp_fir_destroy) ([**dp\_fir\_state\_t**](structdp__fir__state__t.md) \* state) <br>_Release all heap resources owned by the filter state. Frees the tap array, delay line, and scratch buffer, then the state struct itself. Passing NULL is a no-op. The Python wrapper calls this automatically in_ **del** _and_**exit** _; call it explicitly only when you want deterministic resource release before GC._ |
|  size\_t | [**dp\_fir\_execute**](#function-dp_fir_execute) ([**dp\_fir\_state\_t**](structdp__fir__state__t.md) \* state, const float \_Complex \* in, size\_t n\_in, float \_Complex \* out) <br>_Filter n\_in CF32 samples and write the results to out. Each output sample is the inner product of the tap vector with the current delay line. The delay line is updated with each input sample so state carries over across successive calls — process frames of any size without gaps or overlap. The scratch buffer is grown lazily on the first call and reused on subsequent calls of the same size._  |
|  size\_t | [**dp\_fir\_execute\_max\_out**](#function-dp_fir_execute_max_out) ([**dp\_fir\_state\_t**](structdp__fir__state__t.md) \* state) <br>_Always 0_  _FIR is a 1:1 transform, not a bounded-capacity one._ |
|  int | [**dp\_fir\_get\_is\_real**](#function-dp_fir_get_is_real) (const [**dp\_fir\_state\_t**](structdp__fir__state__t.md) \* state) <br>_True when the filter was created with real-valued tap coefficients. Real-tap filters (fir\_create\_real) use a cheaper inner loop: 1 FMA/tap versus the 2 FMA + lane permute required for complex multiplication. Use this flag to confirm which constructor path was used at runtime._  |
|  void | [**dp\_fir\_get\_state**](#function-dp_fir_get_state) (const [**dp\_fir\_state\_t**](structdp__fir__state__t.md) \* state, void \* blob) <br>_Serialize_ `state's` _delay line into_`blob` _._ |
|  void | [**dp\_fir\_reset**](#function-dp_fir_reset) ([**dp\_fir\_state\_t**](structdp__fir__state__t.md) \* state) <br>_Zero the delay line; preserve taps and scratch capacity. After a reset the filter behaves identically to a freshly constructed instance of the same length, without paying the allocation cost again. Call this between unrelated signal segments to prevent inter-segment leakage through the delay line._  |
|  int | [**dp\_fir\_set\_state**](#function-dp_fir_set_state) ([**dp\_fir\_state\_t**](structdp__fir__state__t.md) \* state, const void \* blob) <br>_Restore the delay line from_ `blob` _(same num\_taps)._ |
|  size\_t | [**dp\_fir\_state\_bytes**](#function-dp_fir_state_bytes) (const [**dp\_fir\_state\_t**](structdp__fir__state__t.md) \* state) <br>_Bytes_ [_**dp\_fir\_get\_state()**_](fir__core_8h.md#function-dp_fir_get_state) _writes for_`state` _(envelope + payload)._ |
|  [**dp\_fir\_state\_t**](structdp__fir__state__t.md) \* | [**fir\_create\_real**](#function-fir_create_real) (const float \* taps, size\_t num\_taps) <br>_Create a FIR filter from real float tap coefficients._  |
|  double | [**fir\_dc\_gain**](#function-fir_dc_gain) (const [**dp\_fir\_state\_t**](structdp__fir__state__t.md) \* state) <br>_The filter's response to a constant input: the sum of its taps._  |
|  size\_t | [**fir\_get\_num\_taps**](#function-fir_get_num_taps) (const [**dp\_fir\_state\_t**](structdp__fir__state__t.md) \* state) <br>_Number of tap coefficients supplied at creation. This equals the filter group delay plus one, and determines the minimum input block length for which no latency is observable._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) float \_Complex | [**fir\_step**](#function-fir_step) ([**dp\_fir\_state\_t**](structdp__fir__state__t.md) \* s, float \_Complex x) <br>_Single-sample direct-form FIR step (inline composition API)._  |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**FIR\_STATE\_MAGIC**](fir__core_8h.md#define-fir_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('F', 'I', 'R', '\_')`<br> |
| define  | [**FIR\_STATE\_VERSION**](fir__core_8h.md#define-fir_state_version)  `1u`<br> |

## Detailed Description


Two constructors select the tap type at creation time:


[**dp\_fir\_create()**](fir__core_8h.md#function-dp_fir_create) — complex CF32 taps (general case) [**fir\_create\_real()**](fir__core_8h.md#function-fir_create_real) — real float taps (1 FMA/tap; use for real-valued designs)


All execute functions accept CF32 input and write CF32 output. The internal scratch buffer (delay + input) is allocated lazily on the first execute call and grown as needed.



```C++
float taps[63] = { ... };
dp_fir_state_t *fir = fir_create_real(taps, 63);
float _Complex out[4096];
dp_fir_execute(fir, signal, 4096, out);
dp_fir_destroy(fir);
```
 


    
## Public Functions Documentation




### function dp\_fir\_create 

_Create a FIR filter from complex CF32 tap coefficients. Implements a direct-form FIR convolution:_ `y[n]` _= sum\_k_`h[k]` _\*_`x[n-k]` _. The tap array is copied at creation; the caller may free it afterward. Use_[_**fir\_create\_real()**_](fir__core_8h.md#function-fir_create_real) _instead when all imaginary parts are zero — that path costs 1 FMA/tap versus 2 FMA + permute + mul here._
```C++
dp_fir_state_t * dp_fir_create (
    const float _Complex * taps,
    size_t taps_len
) 
```





**Parameters:**


* `taps` Array of taps\_len CF32 coefficients (I+jQ each), copied. 
* `taps_len` Filter length (&gt;= 1). 



**Returns:**

Heap-allocated state, or NULL on allocation failure. 
```C++
>>> import numpy as np
>>> from doppler.filter import FIR
>>> taps = np.array([0.25+0j, 0.5+0j, 0.25+0j], dtype=np.complex64)
>>> fir = FIR(taps)
>>> fir.num_taps
3
>>> fir.is_real
False
```
 





        

<hr>



### function dp\_fir\_destroy 

_Release all heap resources owned by the filter state. Frees the tap array, delay line, and scratch buffer, then the state struct itself. Passing NULL is a no-op. The Python wrapper calls this automatically in_ **del** _and_**exit** _; call it explicitly only when you want deterministic resource release before GC._
```C++
void dp_fir_destroy (
    dp_fir_state_t * state
) 
```




```C++
>>> import numpy as np
>>> from doppler.filter import FIR
>>> taps = np.array([0.25+0j, 0.5+0j, 0.25+0j], dtype=np.complex64)
>>> with FIR(taps) as fir:
...     y = fir.execute(1.0+0j)
...     y.dtype
dtype('complex64')
```
 


        

<hr>



### function dp\_fir\_execute 

_Filter n\_in CF32 samples and write the results to out. Each output sample is the inner product of the tap vector with the current delay line. The delay line is updated with each input sample so state carries over across successive calls — process frames of any size without gaps or overlap. The scratch buffer is grown lazily on the first call and reused on subsequent calls of the same size._ 
```C++
size_t dp_fir_execute (
    dp_fir_state_t * state,
    const float _Complex * in,
    size_t n_in,
    float _Complex * out
) 
```





**Parameters:**


* `state` Filter state (delay line + taps). 
* `in` Input array of n\_in CF32 samples. 
* `n_in` Number of input samples to process. 
* `out` Output buffer; caller must provide space for n\_in CF32 values. 



**Returns:**

Number of output samples written (always == n\_in). 
```C++
>>> import numpy as np
>>> from doppler.filter import FIR
>>> taps = np.array([0.25+0j, 0.5+0j, 0.25+0j], dtype=np.complex64)
>>> fir = FIR(taps)
>>> x = np.array([1+0j, 0+0j, 0+0j], dtype=np.complex64)
>>> y = fir.execute(x)
>>> y.dtype
dtype('complex64')
>>> y.shape
(3,)
>>> [round(float(v.real), 4) for v in y]
[0.25, 0.5, 0.25]
```
 





        

<hr>



### function dp\_fir\_execute\_max\_out 

_Always 0_  _FIR is a 1:1 transform, not a bounded-capacity one._
```C++
size_t dp_fir_execute_max_out (
    dp_fir_state_t * state
) 
```



[**dp\_fir\_execute()**](fir__core_8h.md#function-dp_fir_execute) always writes exactly n\_in samples; there is no call-independent upper bound smaller than the input length for this function to report. An `out=` buffer must be sized to exactly `len(x)`, not to this function's return value. 


        

<hr>



### function dp\_fir\_get\_is\_real 

_True when the filter was created with real-valued tap coefficients. Real-tap filters (fir\_create\_real) use a cheaper inner loop: 1 FMA/tap versus the 2 FMA + lane permute required for complex multiplication. Use this flag to confirm which constructor path was used at runtime._ 
```C++
int dp_fir_get_is_real (
    const dp_fir_state_t * state
) 
```




```C++
>>> import numpy as np
>>> from doppler.filter import FIR
>>> taps = np.array([0.25+0j, 0.5+0j, 0.25+0j], dtype=np.complex64)
>>> FIR(taps).is_real
False
```
 


        

<hr>



### function dp\_fir\_get\_state 

_Serialize_ `state's` _delay line into_`blob` _._
```C++
void dp_fir_get_state (
    const dp_fir_state_t * state,
    void * blob
) 
```




<hr>



### function dp\_fir\_reset 

_Zero the delay line; preserve taps and scratch capacity. After a reset the filter behaves identically to a freshly constructed instance of the same length, without paying the allocation cost again. Call this between unrelated signal segments to prevent inter-segment leakage through the delay line._ 
```C++
void dp_fir_reset (
    dp_fir_state_t * state
) 
```




```C++
>>> import numpy as np
>>> from doppler.filter import FIR
>>> taps = np.array([0.25+0j, 0.5+0j, 0.25+0j], dtype=np.complex64)
>>> fir = FIR(taps)
>>> x = np.array([1+0j, 0+0j, 0+0j], dtype=np.complex64)
>>> _ = fir.execute(x)
>>> fir.reset()
>>> y = fir.execute(x)
>>> [round(float(v.real), 4) for v in y]
[0.25, 0.5, 0.25]
```
 


        

<hr>



### function dp\_fir\_set\_state 

_Restore the delay line from_ `blob` _(same num\_taps)._
```C++
int dp_fir_set_state (
    dp_fir_state_t * state,
    const void * blob
) 
```





**Returns:**

DP\_OK, or DP\_ERR\_INVALID if the blob's envelope rejects. 





        

<hr>



### function dp\_fir\_state\_bytes 

_Bytes_ [_**dp\_fir\_get\_state()**_](fir__core_8h.md#function-dp_fir_get_state) _writes for_`state` _(envelope + payload)._
```C++
size_t dp_fir_state_bytes (
    const dp_fir_state_t * state
) 
```




<hr>



### function fir\_create\_real 

_Create a FIR filter from real float tap coefficients._ 
```C++
dp_fir_state_t * fir_create_real (
    const float * taps,
    size_t num_taps
) 
```



Real taps cost 1 FMA/tap instead of 2 FMA + permute + mul. Use for filters designed with e.g. scipy.signal.firwin.




**Parameters:**


* `taps` Pointer to num\_taps real tap coefficients (copied). 
* `num_taps` Filter length (&gt;= 1). 



**Returns:**

Heap-allocated state, or NULL on allocation failure. 





        

<hr>



### function fir\_dc\_gain 

_The filter's response to a constant input: the sum of its taps._ 
```C++
double fir_dc_gain (
    const dp_fir_state_t * state
) 
```



Computed from the stored coefficients, so a caller — or a gate — can ask what gain this filter contributes without running a signal through it. A complex-tap filter's DC response is itself complex; this returns its real part, which is the whole answer for the real-tap case that unity-gain questions are usually about.




**Parameters:**


* `state` State. Must be non-NULL. 



**Returns:**

Sum of the taps (real part for a complex-tap filter).



```C++
float h[3] = { 0.25f, 0.5f, 0.25f };
dp_fir_state_t *f = fir_create_real (h, 3);
printf ("%.4f\n", fir_dc_gain (f));   // 1.0000
dp_fir_destroy (f);
```
 


        

<hr>



### function fir\_get\_num\_taps 

_Number of tap coefficients supplied at creation. This equals the filter group delay plus one, and determines the minimum input block length for which no latency is observable._ 
```C++
size_t fir_get_num_taps (
    const dp_fir_state_t * state
) 
```




```C++
>>> import numpy as np
>>> from doppler.filter import FIR
>>> taps = np.array([0.25+0j, 0.5+0j, 0.25+0j], dtype=np.complex64)
>>> FIR(taps).num_taps
3
```
 


        

<hr>



### function fir\_step 

_Single-sample direct-form FIR step (inline composition API)._ 
```C++
JM_FORCEINLINE  JM_HOT float _Complex fir_step (
    dp_fir_state_t * s,
    float _Complex x
) 
```



Filters one sample and advances the delay line: returns `y = sum_k h[k] * x[n-k]` and shifts `x` into the length-`num_taps-1` delay line (dropping the oldest sample). This is the per-sample counterpart to [**dp\_fir\_execute()**](fir__core_8h.md#function-dp_fir_execute) — a tracking receiver inlines it into its own sample loop (e.g. a matched filter feeding a symbol-timing loop) where [**dp\_fir\_execute()**](fir__core_8h.md#function-dp_fir_execute)'s block interface cannot. It mirrors [**dp\_fir\_execute()**](fir__core_8h.md#function-dp_fir_execute)'s real-tap scalar accumulation term for term, so a [**fir\_step()**](fir__core_8h.md#function-fir_step) stream matches [**dp\_fir\_execute()**](fir__core_8h.md#function-dp_fir_execute) to within floating-point rounding: equal in exact arithmetic; a contracted FMA can differ by ~1 ULP across translation units, and [**dp\_fir\_execute()**](fir__core_8h.md#function-dp_fir_execute) on a multi-sample block can differ a little more from SIMD reassociation. Cost is `num_taps` MACs plus an O(num\_taps) delay-line shift per sample.




**Note:**

**Real-tap filters only** (fir\_create\_real). Pulse-shape matched filters — RRC, raised-cosine, integrate-and-dump — are real-valued, which is the streaming use case this serves; a complex-tap variant would add a complex MAC branch and is left until a consumer needs it.




**Parameters:**


* `s` Real-tap filter state (fir\_create\_real). Must be non-NULL. 
* `x` One input sample. 



**Returns:**

The filtered output sample. 





        

<hr>
## Macro Definition Documentation





### define FIR\_STATE\_MAGIC 

```C++
#define FIR_STATE_MAGIC `DP_FOURCC ('F', 'I', 'R', '_')`
```




<hr>



### define FIR\_STATE\_VERSION 

```C++
#define FIR_STATE_VERSION `1u`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/fir/fir_core.h`

