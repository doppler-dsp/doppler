

# File pn\_core.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**pn**](dir_70aeca018f85f00e17d8853ee6bd0cbb.md) **>** [**pn\_core.h**](pn__core_8h.md)

[Go to the source code of this file](pn__core_8h_source.md)

_PN component API._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "jm_perf.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**pn\_state\_t**](structpn__state__t.md) <br> |


## Public Types

| Type | Name |
| ---: | :--- |
| enum  | [**pn\_\_core\_8h\_1aa5aa6f9f85a17c48ca6b7feb11fe83a7**](#enum-pn__core_8h_1aa5aa6f9f85a17c48ca6b7feb11fe83a7)  <br>_PN state._  |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**pn\_state\_t**](structpn__state__t.md) \* | [**pn\_create**](#function-pn_create) (uint64\_t poly, uint64\_t seed, uint32\_t length, int lfsr) <br>_Allocate and initialise a maximal-length-sequence LFSR. The register is seeded from_ `seed` _and will produce a pseudo-random binary sequence with period 2^length - 1 for any primitive_`poly` _. Both Galois and Fibonacci realizations share the same primitive polynomial and therefore the same period; they differ only in chip ordering/phase._ |
|  void | [**pn\_destroy**](#function-pn_destroy) ([**pn\_state\_t**](structpn__state__t.md) \* state) <br>_Destroy a pn instance and release all memory. Idempotent when_ `state` _is NULL; safe to call at any point in the lifecycle. After return the pointer is dangling — do not dereference it._ |
|  size\_t | [**pn\_generate**](#function-pn_generate) ([**pn\_state\_t**](structpn__state__t.md) \* state, size\_t n, uint8\_t \* out) <br>_Generate_ `n` _chips into_`out` _and advance the LFSR by_`n` _positions. Each element of_`out` _is 0 or 1. Requesting more than one MLS period is valid — the sequence simply wraps around. The Python binding returns a zero-copy NumPy uint8 view over a pre-allocated buffer; copy the result before calling generate again if you need a snapshot._ |
|  size\_t | [**pn\_generate\_max\_out**](#function-pn_generate_max_out) ([**pn\_state\_t**](structpn__state__t.md) \* state) <br> |
|  void | [**pn\_reset**](#function-pn_reset) ([**pn\_state\_t**](structpn__state__t.md) \* state) <br>_Reset PN to its post-create state. Reloads the LFSR register from the original seed so the sequence restarts from chip 0. Useful for reproducible captures without re-allocating._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) uint8\_t | [**pn\_step**](#function-pn_step) ([**pn\_state\_t**](structpn__state__t.md) \* state) <br>_Advance the LFSR one step and return the output chip (0 or 1). Both realizations output the register LSB and then shift right. Galois XORs the tap polynomial on a 1 output bit (internal feedback); Fibonacci computes the parity of all tapped positions and inserts it at the top (external feedback). Same primitive polynomial, same period. Inlined so per-sample modulators (e.g. synth's bpsk/qpsk data source) can pull chips in a tight hot loop without call overhead._  |




























## Detailed Description


Lifecycle: create -&gt; `[step / steps / reset]*` -&gt; destroy


Example: 
```C++
pn_state_t *obj = pn_create(96, 1, 7);
uint8_t y = pn_step(obj);
pn_destroy(obj);
```
 


    
## Public Types Documentation




### enum pn\_\_core\_8h\_1aa5aa6f9f85a17c48ca6b7feb11fe83a7 

_PN state._ 
```C++
enum pn__core_8h_1aa5aa6f9f85a17c48ca6b7feb11fe83a7 {
    PN_GALOIS = 0,
    PN_FIBONACCI = 1
};
```



Allocate with [**pn\_create()**](pn__core_8h.md#function-pn_create). LFSR realization: Galois (internal XOR) or Fibonacci (external XOR). 


        

<hr>
## Public Functions Documentation




### function pn\_create 

_Allocate and initialise a maximal-length-sequence LFSR. The register is seeded from_ `seed` _and will produce a pseudo-random binary sequence with period 2^length - 1 for any primitive_`poly` _. Both Galois and Fibonacci realizations share the same primitive polynomial and therefore the same period; they differ only in chip ordering/phase._
```C++
pn_state_t * pn_create (
    uint64_t poly,
    uint64_t seed,
    uint32_t length,
    int lfsr
) 
```





**Parameters:**


* `poly` Galois feedback tap polynomial (right-shift convention). The LSB is the tap at position 0 (always 1 for a primitive poly); bit k=1 means tap at position k. Default 96 (0x60) is primitive for length=7, giving period 127. The Fibonacci taps are derived automatically so you only supply one value. 
* `seed` Initial LFSR register state; must be non-zero (the all-zero state is a fixed point). Default 1. 
* `length` Register width in bits, 1..64. The sequence period is 2^length - 1 for a primitive polynomial. Default 7. 
* `lfsr` Realization: PN\_GALOIS (0, default) or PN\_FIBONACCI (1). 



**Returns:**

Heap-allocated state, or NULL on allocation failure. 




**Note:**

Caller must call [**pn\_destroy()**](pn__core_8h.md#function-pn_destroy) when done. 
```C++
>>> from doppler.wfm import PN
>>> import numpy as np
>>> p = PN(poly=96, seed=1, length=7)
>>> chips = p.generate(127)
>>> chips.dtype
dtype('uint8')
>>> int(chips.sum())   # 64 ones per MLS period (2^(n-1))
64
```
 





        

<hr>



### function pn\_destroy 

_Destroy a pn instance and release all memory. Idempotent when_ `state` _is NULL; safe to call at any point in the lifecycle. After return the pointer is dangling — do not dereference it._
```C++
void pn_destroy (
    pn_state_t * state
) 
```





**Parameters:**


* `state` Pointer to heap-allocated state; may be NULL (no-op). 
```C++
>>> from doppler.wfm import PN
>>> p = PN(poly=96, seed=1, length=7)
>>> p.destroy()   # explicit teardown; no exception
```
 




        

<hr>



### function pn\_generate 

_Generate_ `n` _chips into_`out` _and advance the LFSR by_`n` _positions. Each element of_`out` _is 0 or 1. Requesting more than one MLS period is valid — the sequence simply wraps around. The Python binding returns a zero-copy NumPy uint8 view over a pre-allocated buffer; copy the result before calling generate again if you need a snapshot._
```C++
size_t pn_generate (
    pn_state_t * state,
    size_t n,
    uint8_t * out
) 
```





**Parameters:**


* `state` Initialised PN state returned by `pn_create`. 
* `n` Number of chips to produce. 
* `out` Output buffer of at least `n` uint8 elements; each element receives 0 or 1. 



**Returns:**

`n` (the number of chips written; always equal to the request). 
```C++
>>> from doppler.wfm import PN
>>> import numpy as np
>>> p = PN(poly=96, seed=1, length=7)
>>> chips = p.generate(127)
>>> chips[:8].tolist()
[1, 0, 0, 0, 0, 0, 1, 1]
>>> int(chips.sum())   # 64 ones per MLS period
64
```
 





        

<hr>



### function pn\_generate\_max\_out 

```C++
size_t pn_generate_max_out (
    pn_state_t * state
) 
```




<hr>



### function pn\_reset 

_Reset PN to its post-create state. Reloads the LFSR register from the original seed so the sequence restarts from chip 0. Useful for reproducible captures without re-allocating._ 
```C++
void pn_reset (
    pn_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 
```C++
>>> from doppler.wfm import PN
>>> import numpy as np
>>> p = PN(poly=96, seed=1, length=7)
>>> a = p.generate(8).copy()
>>> p.reset()
>>> np.array_equal(a, p.generate(8))
True
```
 




        

<hr>



### function pn\_step 

_Advance the LFSR one step and return the output chip (0 or 1). Both realizations output the register LSB and then shift right. Galois XORs the tap polynomial on a 1 output bit (internal feedback); Fibonacci computes the parity of all tapped positions and inserts it at the top (external feedback). Same primitive polynomial, same period. Inlined so per-sample modulators (e.g. synth's bpsk/qpsk data source) can pull chips in a tight hot loop without call overhead._ 
```C++
JM_FORCEINLINE uint8_t pn_step (
    pn_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Output chip: 0 or 1 (register LSB before the shift). 





        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/pn/pn_core.h`

