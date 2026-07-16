

# File gold\_core.h



[**FileList**](files.md) **>** [**gold**](dir_eaad5c90f79e5666c89030cb43ebb96d.md) **>** [**gold\_core.h**](gold__core_8h.md)

[Go to the source code of this file](gold__core_8h_source.md)

_Gold code component API._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "dp_state.h"`
* `#include "jm_perf.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**gold\_state\_t**](structgold__state__t.md) <br>_Gold state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**gold\_state\_t**](structgold__state__t.md) \* | [**gold\_create**](#function-gold_create) (uint64\_t taps\_a, uint64\_t seed\_a, uint64\_t taps\_b, uint64\_t seed\_b, uint32\_t length) <br>_Allocate and initialise a CCSDS-style Gold code generator. Two independent Fibonacci LFSRs of the same_ `length` _free-run in lock-step; each output chip is the XOR of both registers' current top-bit (stage_`length` _, i.e. bit_`length-1` _). Both registers shift left one bit per chip: the new bit (parity of the tapped stages, read__before_ _the shift) enters at stage 1 (bit 0), and the old stage-_`length` _bit is discarded after being XORed into the output. The sequence period is_`2^length - 1` _for primitive_`taps_a` _/_`taps_b` _._ |
|  void | [**gold\_destroy**](#function-gold_destroy) ([**gold\_state\_t**](structgold__state__t.md) \* state) <br>_Destroy a gold instance and release all memory. Idempotent when_ `state` _is NULL; safe to call at any point in the lifecycle. After return the pointer is dangling — do not dereference it._ |
|  size\_t | [**gold\_generate**](#function-gold_generate) ([**gold\_state\_t**](structgold__state__t.md) \* state, size\_t n, uint8\_t \* out) <br>_Generate_ `n` _chips into_`out` _and advance both LFSRs by_`n` _positions. Each element of_`out` _is 0 or 1. Requesting more than one period is valid — the sequence simply wraps around. The Python binding returns a zero-copy NumPy uint8 view over a pre-allocated buffer; copy the result before calling generate again if you need a snapshot._ |
|  size\_t | [**gold\_generate\_max\_out**](#function-gold_generate_max_out) ([**gold\_state\_t**](structgold__state__t.md) \* state) <br> |
|  void | [**gold\_get\_state**](#function-gold_get_state) (const [**gold\_state\_t**](structgold__state__t.md) \* state, void \* blob) <br>_Serialize both LFSR registers into_ `blob` _._ |
|  void | [**gold\_reset**](#function-gold_reset) ([**gold\_state\_t**](structgold__state__t.md) \* state) <br>_Reset Gold to its post-create state. Reloads both LFSR registers from their original seeds so the sequence restarts from chip 0. Useful for reproducible captures without re-allocating._  |
|  int | [**gold\_set\_state**](#function-gold_set_state) ([**gold\_state\_t**](structgold__state__t.md) \* state, const void \* blob) <br>_Restore both registers; DP\_OK, or DP\_ERR\_INVALID if rejected._  |
|  size\_t | [**gold\_state\_bytes**](#function-gold_state_bytes) (const [**gold\_state\_t**](structgold__state__t.md) \* state) <br>_Serialized-state byte size._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) uint8\_t | [**gold\_step**](#function-gold_step) ([**gold\_state\_t**](structgold__state__t.md) \* state) <br>_Advance both LFSRs one chip and return the XOR-combined output chip (0 or 1). Each register outputs its current stage-_ `length` _bit (the top bit), computes its own feedback (parity of the tapped stages), and shifts left with the feedback bit entering at stage 1. Inlined so composing objects (e.g. a DSSS spreader) can pull chips in a tight hot loop without call overhead — mirrors_[_**pn\_core.h**_](pn__core_8h.md) _'s_[_**pn\_step()**_](pn__core_8h.md#function-pn_step) _._ |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**GOLD\_STATE\_MAGIC**](gold__core_8h.md#define-gold_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc)('G', 'O', 'L', 'D')`<br> |
| define  | [**GOLD\_STATE\_VERSION**](gold__core_8h.md#define-gold_state_version)  `1u`<br> |

## Detailed Description


CCSDS Command Link Gold Code Generator (CCSDS 415.0-G-1, section 5.2.2.4, Figure 5-1): two same-clocked Fibonacci LFSRs ("Register A" and "Register B"), each with its own fixed feedback-tap polynomial, XOR- combined chip-by-chip into a single 1023-chip (length=10) Gold code. The two m-sequences form a genuine "preferred pair" — their XOR family has a strict three-valued periodic autocorrelation/cross-correlation set {-1, -65, 63} (verified: see native/tests/test\_gold\_core.c). Register A's initial condition is "User dependent" per the standard — varying it walks the whole Gold-code family (2^length members); Register B's taps and initial condition are both fixed by the standard.


Lifecycle: create -&gt; [generate / reset]\* -&gt; destroy


Example: 
```C++
gold_state_t *obj = gold_create(934, 350, 567, 73, 10);
uint8_t chips[16];
gold_generate(obj, 16, chips);
gold_destroy(obj);
```
 


    
## Public Functions Documentation




### function gold\_create 

_Allocate and initialise a CCSDS-style Gold code generator. Two independent Fibonacci LFSRs of the same_ `length` _free-run in lock-step; each output chip is the XOR of both registers' current top-bit (stage_`length` _, i.e. bit_`length-1` _). Both registers shift left one bit per chip: the new bit (parity of the tapped stages, read__before_ _the shift) enters at stage 1 (bit 0), and the old stage-_`length` _bit is discarded after being XORed into the output. The sequence period is_`2^length - 1` _for primitive_`taps_a` _/_`taps_b` _._
```C++
gold_state_t * gold_create (
    uint64_t taps_a,
    uint64_t seed_a,
    uint64_t taps_b,
    uint64_t seed_b,
    uint32_t length
) 
```





**Parameters:**


* `taps_a` Register A feedback-tap mask; bit k set means stage k+1 is XORed into the feedback. Default 934 (stages 2,3,6,8,9,10 — the CCSDS-fixed Register A polynomial x^10+x^9+x^8+x^6+x^3+x^2+1). 
* `seed_a` Register A initial value; must be non-zero. Per CCSDS this is "User dependent" — any nonzero value selects a different member of the 1024-code Gold family. Default 350 is the worked example from CCSDS 415.0-G-1 Figure 5-2 (PN Code Library Table 1, Code Number 365). 
* `taps_b` Register B feedback-tap mask, same bit convention as `taps_a`. Default 567 (stages 1,2,3,5,6,10 — the CCSDS-fixed Register B polynomial). 
* `seed_b` Register B initial value; must be non-zero. Default 73 (stages 1,4,7 — CCSDS's fixed Register B initial value 1001001000, unique per the standard, not user-selectable). 
* `length` Register width in bits, 1..64. CCSDS command link uses 10 (period 1023). Default 10. 



**Returns:**

Heap-allocated state, or NULL on allocation failure or invalid arguments (zero seed, zero/out-of-range length). 




**Note:**

Caller must call [**gold\_destroy()**](gold__core_8h.md#function-gold_destroy) when done. 
```C++
>>> from doppler.wfm import Gold
>>> import numpy as np
>>> g = Gold()
>>> chips = g.generate(1023)
>>> chips.dtype
dtype('uint8')
>>> chips[:15].tolist()   # CCSDS Code #365 worked example
[0, 1, 0, 0, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1]
>>> int(chips.sum()), int((1 - chips).sum())   # balanced: 512 ones, 511 zeros
(512, 511)
```
 





        

<hr>



### function gold\_destroy 

_Destroy a gold instance and release all memory. Idempotent when_ `state` _is NULL; safe to call at any point in the lifecycle. After return the pointer is dangling — do not dereference it._
```C++
void gold_destroy (
    gold_state_t * state
) 
```





**Parameters:**


* `state` Pointer to heap-allocated state; may be NULL (no-op). 
```C++
>>> from doppler.wfm import Gold
>>> g = Gold()
>>> g.destroy()   # explicit teardown; no exception
```
 




        

<hr>



### function gold\_generate 

_Generate_ `n` _chips into_`out` _and advance both LFSRs by_`n` _positions. Each element of_`out` _is 0 or 1. Requesting more than one period is valid — the sequence simply wraps around. The Python binding returns a zero-copy NumPy uint8 view over a pre-allocated buffer; copy the result before calling generate again if you need a snapshot._
```C++
size_t gold_generate (
    gold_state_t * state,
    size_t n,
    uint8_t * out
) 
```





**Parameters:**


* `state` Initialised Gold state returned by `gold_create`. 
* `n` Number of chips to produce. 
* `out` Output buffer of at least `n` uint8 elements; each element receives 0 or 1. 



**Returns:**

`n` (the number of chips written; always equal to the request). 
```C++
>>> from doppler.wfm import Gold
>>> import numpy as np
>>> g = Gold()
>>> chips = g.generate(1023)
>>> len(chips)
1023
```
 





        

<hr>



### function gold\_generate\_max\_out 

```C++
size_t gold_generate_max_out (
    gold_state_t * state
) 
```




<hr>



### function gold\_get\_state 

_Serialize both LFSR registers into_ `blob` _._
```C++
void gold_get_state (
    const gold_state_t * state,
    void * blob
) 
```




<hr>



### function gold\_reset 

_Reset Gold to its post-create state. Reloads both LFSR registers from their original seeds so the sequence restarts from chip 0. Useful for reproducible captures without re-allocating._ 
```C++
void gold_reset (
    gold_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 
```C++
>>> from doppler.wfm import Gold
>>> import numpy as np
>>> g = Gold()
>>> a = g.generate(8).copy()
>>> g.reset()
>>> np.array_equal(a, g.generate(8))
True
```
 




        

<hr>



### function gold\_set\_state 

_Restore both registers; DP\_OK, or DP\_ERR\_INVALID if rejected._ 
```C++
int gold_set_state (
    gold_state_t * state,
    const void * blob
) 
```




<hr>



### function gold\_state\_bytes 

_Serialized-state byte size._ 
```C++
size_t gold_state_bytes (
    const gold_state_t * state
) 
```




<hr>



### function gold\_step 

_Advance both LFSRs one chip and return the XOR-combined output chip (0 or 1). Each register outputs its current stage-_ `length` _bit (the top bit), computes its own feedback (parity of the tapped stages), and shifts left with the feedback bit entering at stage 1. Inlined so composing objects (e.g. a DSSS spreader) can pull chips in a tight hot loop without call overhead — mirrors_[_**pn\_core.h**_](pn__core_8h.md) _'s_[_**pn\_step()**_](pn__core_8h.md#function-pn_step) _._
```C++
JM_FORCEINLINE uint8_t gold_step (
    gold_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Output chip: 0 or 1. 





        

<hr>
## Macro Definition Documentation





### define GOLD\_STATE\_MAGIC 

```C++
#define GOLD_STATE_MAGIC `DP_FOURCC ('G', 'O', 'L', 'D')`
```




<hr>



### define GOLD\_STATE\_VERSION 

```C++
#define GOLD_STATE_VERSION `1u`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/gold/gold_core.h`

