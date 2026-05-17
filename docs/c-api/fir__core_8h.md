

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
|  [**fir\_state\_t**](structfir__state__t.md) \* | [**fir\_create**](#function-fir_create) (const float complex \* taps, size\_t num\_taps) <br>_Create a FIR filter from complex CF32 tap coefficients._  |
|  [**fir\_state\_t**](structfir__state__t.md) \* | [**fir\_create\_real**](#function-fir_create_real) (const float \* taps, size\_t num\_taps) <br>_Create a FIR filter from real float tap coefficients._  |
|  void | [**fir\_destroy**](#function-fir_destroy) ([**fir\_state\_t**](structfir__state__t.md) \* state) <br>_Destroy the filter; safe to pass NULL._  |
|  size\_t | [**fir\_execute**](#function-fir_execute) ([**fir\_state\_t**](structfir__state__t.md) \* state, const float complex \* in, size\_t n\_in, float complex \* out) <br>_Filter n\_in CF32 samples; write results to out._  |
|  size\_t | [**fir\_execute\_max\_out**](#function-fir_execute_max_out) ([**fir\_state\_t**](structfir__state__t.md) \* state) <br>_Upper bound on execute output samples (always == n\_in for FIR)._  |
|  int | [**fir\_get\_is\_real**](#function-fir_get_is_real) (const [**fir\_state\_t**](structfir__state__t.md) \* state) <br>_1 if filter was created with real taps, 0 if complex._  |
|  size\_t | [**fir\_get\_num\_taps**](#function-fir_get_num_taps) (const [**fir\_state\_t**](structfir__state__t.md) \* state) <br>_Number of tap coefficients._  |
|  void | [**fir\_reset**](#function-fir_reset) ([**fir\_state\_t**](structfir__state__t.md) \* state) <br>_Zero the delay line; preserve taps and scratch capacity._  |




























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

_Create a FIR filter from complex CF32 tap coefficients._ 
```C++
fir_state_t * fir_create (
    const float complex * taps,
    size_t num_taps
) 
```





**Parameters:**


* `taps` Pointer to num\_taps complex tap coefficients (copied). 
* `num_taps` Filter length (&gt;= 1). 



**Returns:**

Heap-allocated state, or NULL on allocation failure. 





        

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

_Destroy the filter; safe to pass NULL._ 
```C++
void fir_destroy (
    fir_state_t * state
) 
```




<hr>



### function fir\_execute 

_Filter n\_in CF32 samples; write results to out._ 
```C++
size_t fir_execute (
    fir_state_t * state,
    const float complex * in,
    size_t n_in,
    float complex * out
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `in` Input array of n\_in float complex samples. 
* `n_in` Number of input samples. 
* `out` Output array (may alias in for in-place). 



**Returns:**

n\_in on success, 0 on allocation failure. 





        

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

_1 if filter was created with real taps, 0 if complex._ 
```C++
int fir_get_is_real (
    const fir_state_t * state
) 
```




<hr>



### function fir\_get\_num\_taps 

_Number of tap coefficients._ 
```C++
size_t fir_get_num_taps (
    const fir_state_t * state
) 
```




<hr>



### function fir\_reset 

_Zero the delay line; preserve taps and scratch capacity._ 
```C++
void fir_reset (
    fir_state_t * state
) 
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/fir/fir_core.h`

