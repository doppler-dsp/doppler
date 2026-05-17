

# File nco\_core.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**nco**](dir_2f9ed967bc16fefd26d0244d883adb58.md) **>** [**nco\_core.h**](nco__core_8h.md)

[Go to the source code of this file](nco__core_8h_source.md)

_Pure 32-bit phase-accumulator NCO._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "jm_perf.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**nco\_state\_t**](structnco__state__t.md) <br>_NCO state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**nco\_state\_t**](structnco__state__t.md) \* | [**nco\_create**](#function-nco_create) (double norm\_freq, uint32\_t nmax) <br>_Create an NCO instance._  |
|  void | [**nco\_destroy**](#function-nco_destroy) ([**nco\_state\_t**](structnco__state__t.md) \* state) <br> |
|  double | [**nco\_get\_norm\_freq**](#function-nco_get_norm_freq) (const [**nco\_state\_t**](structnco__state__t.md) \* state) <br> |
|  uint32\_t | [**nco\_get\_phase**](#function-nco_get_phase) (const [**nco\_state\_t**](structnco__state__t.md) \* state) <br> |
|  uint32\_t | [**nco\_get\_phase\_inc**](#function-nco_get_phase_inc) (const [**nco\_state\_t**](structnco__state__t.md) \* state) <br> |
|  void | [**nco\_reset**](#function-nco_reset) ([**nco\_state\_t**](structnco__state__t.md) \* state) <br> |
|  void | [**nco\_set\_norm\_freq**](#function-nco_set_norm_freq) ([**nco\_state\_t**](structnco__state__t.md) \* state, double norm\_freq) <br> |
|  void | [**nco\_set\_phase**](#function-nco_set_phase) ([**nco\_state\_t**](structnco__state__t.md) \* state, uint32\_t phase) <br> |
|  size\_t | [**nco\_steps\_u32**](#function-nco_steps_u32) ([**nco\_state\_t**](structnco__state__t.md) \* state, size\_t n, uint32\_t \* out) <br>_Advance n samples; write raw uint32 accumulator values._  |
|  size\_t | [**nco\_steps\_u32\_max\_out**](#function-nco_steps_u32_max_out) ([**nco\_state\_t**](structnco__state__t.md) \* state) <br>_Maximum samples per call (determines pre-allocated buffer size)._  |
|  size\_t | [**nco\_steps\_u32\_ovf**](#function-nco_steps_u32_ovf) ([**nco\_state\_t**](structnco__state__t.md) \* state, size\_t n, uint32\_t \* out, uint8\_t \* out1) <br>_Advance n samples; write raw phase values and per-sample carry._  |
|  size\_t | [**nco\_steps\_u32\_ovf\_max\_out**](#function-nco_steps_u32_ovf_max_out) ([**nco\_state\_t**](structnco__state__t.md) \* state) <br> |
|  size\_t | [**nco\_steps\_u32\_scaled**](#function-nco_steps_u32_scaled) ([**nco\_state\_t**](structnco__state__t.md) \* state, size\_t n, uint32\_t \* out) <br>_Advance n samples; values scaled to [0, nmax)._  |
|  size\_t | [**nco\_steps\_u32\_scaled\_max\_out**](#function-nco_steps_u32_scaled_max_out) ([**nco\_state\_t**](structnco__state__t.md) \* state) <br> |


## Public Static Functions

| Type | Name |
| ---: | :--- |
|  uint8\_t | [**nco\_add\_ovf\_**](#function-nco_add_ovf_) (uint32\_t a, uint32\_t b, uint32\_t \* res) <br>_Wrapping add with carry detection._  |

























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**NCO\_ADD\_OVF**](nco__core_8h.md#define-nco_add_ovf) (a, b, res) `nco\_add\_ovf\_ ((a), (b), (res))`<br> |

## Detailed Description


Phase increments by phase\_inc each sample and wraps naturally at 2^32. Three output mappings:


nco\_steps\_u32 raw accumulator value [0, 2^32) nco\_steps\_u32\_scaled (uint64)phase \* nmax &gt;&gt; 32 → [0, nmax) nco\_steps\_u32\_ovf raw phase + per-sample carry flag


nmax=0 in nco\_steps\_u32\_scaled is treated identically to nco\_steps\_u32 (returns raw accumulator unchanged).


Normalised-frequency → phase\_inc conversion: phase\_inc = floor((norm\_freq mod 1.0) × 2^32)


Negative frequencies fold correctly: −0.25 → phase\_inc = 3×2^30.


reset() zeroes phase only; norm\_freq and nmax are unchanged.


Lifecycle: nco\_create → (steps / reset)\* → nco\_destroy



```C++
nco_state_t *nco = nco_create(0.25, 0);
uint32_t out[4];
nco_steps_u32(nco, 4, out);
// out[0]=0x00000000, out[1]=0x40000000,
// out[2]=0x80000000, out[3]=0xC0000000
nco_destroy(nco);
```
 


    
## Public Functions Documentation




### function nco\_create 

_Create an NCO instance._ 
```C++
nco_state_t * nco_create (
    double norm_freq,
    uint32_t nmax
) 
```





**Parameters:**


* `norm_freq` Normalised frequency (cycles per sample). Any value; fractional part used internally. 
* `nmax` Wrap target for nco\_steps\_u32\_scaled. Pass 0 to return the raw 32-bit accumulator. 



**Returns:**

Heap-allocated state, or NULL on allocation failure. 





        

<hr>



### function nco\_destroy 

```C++
void nco_destroy (
    nco_state_t * state
) 
```



Free all resources. May be NULL (no-op). 


        

<hr>



### function nco\_get\_norm\_freq 

```C++
double nco_get_norm_freq (
    const nco_state_t * state
) 
```




<hr>



### function nco\_get\_phase 

```C++
uint32_t nco_get_phase (
    const nco_state_t * state
) 
```




<hr>



### function nco\_get\_phase\_inc 

```C++
uint32_t nco_get_phase_inc (
    const nco_state_t * state
) 
```




<hr>



### function nco\_reset 

```C++
void nco_reset (
    nco_state_t * state
) 
```



Zero the phase accumulator. norm\_freq and nmax are unchanged. 


        

<hr>



### function nco\_set\_norm\_freq 

```C++
void nco_set_norm_freq (
    nco_state_t * state,
    double norm_freq
) 
```




<hr>



### function nco\_set\_phase 

```C++
void nco_set_phase (
    nco_state_t * state,
    uint32_t phase
) 
```




<hr>



### function nco\_steps\_u32 

_Advance n samples; write raw uint32 accumulator values._ 
```C++
size_t nco_steps_u32 (
    nco_state_t * state,
    size_t n,
    uint32_t * out
) 
```



Output is emitted before increment: out(0) = current phase, out(1) = phase + phase\_inc, etc. Returns n. 


        

<hr>



### function nco\_steps\_u32\_max\_out 

_Maximum samples per call (determines pre-allocated buffer size)._ 
```C++
size_t nco_steps_u32_max_out (
    nco_state_t * state
) 
```



The Python extension pre-allocates output buffers of this size at create time. Requesting more samples per call is undefined behaviour. 


        

<hr>



### function nco\_steps\_u32\_ovf 

_Advance n samples; write raw phase values and per-sample carry._ 
```C++
size_t nco_steps_u32_ovf (
    nco_state_t * state,
    size_t n,
    uint32_t * out,
    uint8_t * out1
) 
```



out(i) — raw 32-bit phase value (same as nco\_steps\_u32). out1(i) — 1 when the accumulator wrapped on sample i, 0 otherwise. The carry marks the boundary of one input period; useful for polyphase sample-clock generation. 


        

<hr>



### function nco\_steps\_u32\_ovf\_max\_out 

```C++
size_t nco_steps_u32_ovf_max_out (
    nco_state_t * state
) 
```




<hr>



### function nco\_steps\_u32\_scaled 

_Advance n samples; values scaled to [0, nmax)._ 
```C++
size_t nco_steps_u32_scaled (
    nco_state_t * state,
    size_t n,
    uint32_t * out
) 
```



Uses the branchless fixed-point identity: out(i) = (uint64\_t)phase \* nmax &gt;&gt; 32 When nmax == 0 falls back to the raw accumulator. 


        

<hr>



### function nco\_steps\_u32\_scaled\_max\_out 

```C++
size_t nco_steps_u32_scaled_max_out (
    nco_state_t * state
) 
```




<hr>
## Public Static Functions Documentation




### function nco\_add\_ovf\_ 

_Wrapping add with carry detection._ 
```C++
static inline uint8_t nco_add_ovf_ (
    uint32_t a,
    uint32_t b,
    uint32_t * res
) 
```



[**NCO\_ADD\_OVF(a, b, res)**](nco__core_8h.md#define-nco_add_ovf) computes \*res = a + b and returns 1 if the addition wrapped (carry out), 0 otherwise. Branchless on x86/AArch64. 


        

<hr>
## Macro Definition Documentation





### define NCO\_ADD\_OVF 

```C++
#define NCO_ADD_OVF (
    a,
    b,
    res
) `nco_add_ovf_ ((a), (b), (res))`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/nco/nco_core.h`

