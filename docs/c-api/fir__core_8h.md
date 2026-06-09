

# File fir\_core.h



[**FileList**](files.md) **>** [**fir**](dir_37fd0118bf34c485dd22fe4d261d6eac.md) **>** [**fir\_core.h**](fir__core_8h.md)

[Go to the source code of this file](fir__core_8h_source.md)

_Direct-form FIR filter — real-tap and complex-tap variants._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "jm_perf.h"`
* `#include <complex.h>`
* `#include <stddef.h>`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**fir\_state\_t**](structfir__state__t.md) <br> |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**fir\_state\_t**](structfir__state__t.md) \* | [**fir\_create**](#function-fir_create) (const float complex \* taps, size\_t num\_taps) <br>_Create a FIR filter from complex CF32 tap coefficients. Implements a direct-form FIR convolution: y[n] = sum\_k h[k]\*x[n-k]. The tap array is copied at creation; the caller may free it afterward. Use_ [_**fir\_create\_real()**_](fir__core_8h.md#function-fir_create_real) _instead when all imaginary parts are zero — that path costs 1 FMA/tap versus 2 FMA + permute + mul here._ |
|  [**fir\_state\_t**](structfir__state__t.md) \* | [**fir\_create\_real**](#function-fir_create_real) (const float \* taps, size\_t num\_taps) <br>_Create a FIR filter from real float tap coefficients._  |
|  void | [**fir\_destroy**](#function-fir_destroy) ([**fir\_state\_t**](structfir__state__t.md) \* state) <br>_Release all heap resources owned by the filter state. Frees the tap array, delay line, and scratch buffer, then the state struct itself. Passing NULL is a no-op. The Python wrapper calls this automatically in_ **del** _and_**exit** _; call it explicitly only when you want deterministic resource release before GC._ |
|  size\_t | [**fir\_execute**](#function-fir_execute) ([**fir\_state\_t**](structfir__state__t.md) \* state, const float complex \* in, size\_t n\_in, float complex \* out) <br>_Filter n\_in CF32 samples and write the results to out. Each output sample is the inner product of the tap vector with the current delay line. The delay line is updated with each input sample so state carries over across successive calls — process frames of any size without gaps or overlap. The scratch buffer is grown lazily on the first call and reused on subsequent calls of the same size._  |
|  size\_t | [**fir\_execute\_max\_out**](#function-fir_execute_max_out) ([**fir\_state\_t**](structfir__state__t.md) \* state) <br>_Upper bound on execute output samples (always == n\_in for FIR)._  |
|  int | [**fir\_get\_is\_real**](#function-fir_get_is_real) (const [**fir\_state\_t**](structfir__state__t.md) \* state) <br>_True when the filter was created with real-valued tap coefficients. Real-tap filters (fir\_create\_real) use a cheaper inner loop: 1 FMA/tap versus the 2 FMA + lane permute required for complex multiplication. Use this flag to confirm which constructor path was used at runtime._  |
|  size\_t | [**fir\_get\_num\_taps**](#function-fir_get_num_taps) (const [**fir\_state\_t**](structfir__state__t.md) \* state) <br>_Number of tap coefficients supplied at creation. This equals the filter group delay plus one, and determines the minimum input block length for which no latency is observable._  |
|  void | [**fir\_reset**](#function-fir_reset) ([**fir\_state\_t**](structfir__state__t.md) \* state) <br>_Zero the delay line; preserve taps and scratch capacity. After a reset the filter behaves identically to a freshly constructed instance of the same length, without paying the allocation cost again. Call this between unrelated signal segments to prevent inter-segment leakage through the delay line._  |




























## Detailed Description


Two constructors select the tap type at creation time:


[**fir\_create()**](fir__core_8h.md#function-fir_create) — complex CF32 taps (general case) [**fir\_create\_real()**](fir__core_8h.md#function-fir_create_real) — real float taps (1 FMA/tap; use for real-valued designs)


All execute functions accept CF32 input and write CF32 output. The internal scratch buffer (delay + input) is allocated lazily on the first execute call and grown as needed.



```C++
float taps[63] = { ... };
fir_state_t *fir = fir_create_real(taps, 63);
float complex out[4096];
fir_execute(fir, signal, 4096, out);
fir_destroy(fir);
```
 


    
## Public Functions Documentation




### function fir\_create 

_Create a FIR filter from complex CF32 tap coefficients. Implements a direct-form FIR convolution: y[n] = sum\_k h[k]\*x[n-k]. The tap array is copied at creation; the caller may free it afterward. Use_ [_**fir\_create\_real()**_](fir__core_8h.md#function-fir_create_real) _instead when all imaginary parts are zero — that path costs 1 FMA/tap versus 2 FMA + permute + mul here._
```C++
fir_state_t * fir_create (
    const float complex * taps,
    size_t num_taps
) 
```





**Parameters:**


* `taps` Array of num\_taps CF32 coefficients (I+jQ each), copied. 
* `num_taps` Filter length (&gt;= 1). 



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



### function fir\_create\_real 

_Create a FIR filter from real float tap coefficients._ 
```C++
fir_state_t * fir_create_real (
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



### function fir\_destroy 

_Release all heap resources owned by the filter state. Frees the tap array, delay line, and scratch buffer, then the state struct itself. Passing NULL is a no-op. The Python wrapper calls this automatically in_ **del** _and_**exit** _; call it explicitly only when you want deterministic resource release before GC._
```C++
void fir_destroy (
    fir_state_t * state
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



### function fir\_execute 

_Filter n\_in CF32 samples and write the results to out. Each output sample is the inner product of the tap vector with the current delay line. The delay line is updated with each input sample so state carries over across successive calls — process frames of any size without gaps or overlap. The scratch buffer is grown lazily on the first call and reused on subsequent calls of the same size._ 
```C++
size_t fir_execute (
    fir_state_t * state,
    const float complex * in,
    size_t n_in,
    float complex * out
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



### function fir\_execute\_max\_out 

_Upper bound on execute output samples (always == n\_in for FIR)._ 
```C++
size_t fir_execute_max_out (
    fir_state_t * state
) 
```



Used by the generated ext.c to size the output buffer. Returns 0 at creation time (n\_in unknown); buffer grows on first call. 


        

<hr>



### function fir\_get\_is\_real 

_True when the filter was created with real-valued tap coefficients. Real-tap filters (fir\_create\_real) use a cheaper inner loop: 1 FMA/tap versus the 2 FMA + lane permute required for complex multiplication. Use this flag to confirm which constructor path was used at runtime._ 
```C++
int fir_get_is_real (
    const fir_state_t * state
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



### function fir\_get\_num\_taps 

_Number of tap coefficients supplied at creation. This equals the filter group delay plus one, and determines the minimum input block length for which no latency is observable._ 
```C++
size_t fir_get_num_taps (
    const fir_state_t * state
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



### function fir\_reset 

_Zero the delay line; preserve taps and scratch capacity. After a reset the filter behaves identically to a freshly constructed instance of the same length, without paying the allocation cost again. Call this between unrelated signal segments to prevent inter-segment leakage through the delay line._ 
```C++
void fir_reset (
    fir_state_t * state
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

------------------------------
The documentation for this class was generated from the following file `native/inc/fir/fir_core.h`

