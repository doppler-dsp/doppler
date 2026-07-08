

# File nco\_core.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**nco**](dir_2f9ed967bc16fefd26d0244d883adb58.md) **>** [**nco\_core.h**](nco__core_8h.md)

[Go to the source code of this file](nco__core_8h_source.md)

_Pure 32-bit phase-accumulator NCO._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "dp_state.h"`
* `#include "jm_perf.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**nco\_state\_t**](structnco__state__t.md) <br>_NCO state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**nco\_state\_t**](structnco__state__t.md) \* | [**nco\_create**](#function-nco_create) (double norm\_freq, uint32\_t nmax) <br>_Create an NCO instance. Allocates and initialises the phase accumulator to zero, converts norm\_freq to the integer phase\_inc = floor(frac(norm\_freq) × 2^32), and stores nmax for scaled output. The NCO is immediately ready to call nco\_steps\_u32 / nco\_steps\_u32\_scaled / nco\_steps\_u32\_ovf._  |
|  void | [**nco\_destroy**](#function-nco_destroy) ([**nco\_state\_t**](structnco__state__t.md) \* state) <br> |
|  double | [**nco\_get\_norm\_freq**](#function-nco_get_norm_freq) (const [**nco\_state\_t**](structnco__state__t.md) \* state) <br>_Normalised frequency (read/write). Setting norm\_freq recomputes phase\_inc = floor(frac(v) × 2^32) and takes effect on the next nco\_steps\_\* call; phase is NOT reset._  |
|  uint32\_t | [**nco\_get\_phase**](#function-nco_get_phase) (const [**nco\_state\_t**](structnco__state__t.md) \* state) <br>_Current phase accumulator value (read/write). Reading returns the current integer phase in_ `[0, 2^32)` _. Writing overrides the accumulator directly, allowing arbitrary phase offsets without re-creating the NCO._ |
|  uint32\_t | [**nco\_get\_phase\_inc**](#function-nco_get_phase_inc) (const [**nco\_state\_t**](structnco__state__t.md) \* state) <br>_Per-sample phase increment (read-only). Derived from norm\_freq as floor(frac(norm\_freq) × 2^32). Updated automatically whenever norm\_freq is written. A freq of 0.25 gives phase\_inc = 1073741824 (0x40000000)._  |
|  void | [**nco\_get\_state**](#function-nco_get_state) (const [**nco\_state\_t**](structnco__state__t.md) \* state, void \* blob) <br>_Serialize the phase accumulator into_ `blob` _._ |
|  void | [**nco\_reset**](#function-nco_reset) ([**nco\_state\_t**](structnco__state__t.md) \* state) <br>_Zero the phase accumulator. Sets phase to 0 so the next nco\_steps\_u32 call starts from the beginning of the cycle. norm\_freq, phase\_inc, and nmax are unchanged; the NCO is ready to generate samples again immediately._  |
|  void | [**nco\_set\_norm\_freq**](#function-nco_set_norm_freq) ([**nco\_state\_t**](structnco__state__t.md) \* state, double norm\_freq) <br> |
|  void | [**nco\_set\_phase**](#function-nco_set_phase) ([**nco\_state\_t**](structnco__state__t.md) \* state, uint32\_t phase) <br> |
|  int | [**nco\_set\_state**](#function-nco_set_state) ([**nco\_state\_t**](structnco__state__t.md) \* state, const void \* blob) <br>_Restore phase; DP\_OK, or DP\_ERR\_INVALID if the envelope rejects._  |
|  size\_t | [**nco\_state\_bytes**](#function-nco_state_bytes) (const [**nco\_state\_t**](structnco__state__t.md) \* state) <br>_Serialized-state byte size._  |
|  size\_t | [**nco\_steps\_u32**](#function-nco_steps_u32) ([**nco\_state\_t**](structnco__state__t.md) \* state, size\_t n, uint32\_t \* out) <br>_Advance n samples; write raw uint32 accumulator values. Each element is the phase value BEFORE the increment fires, so_ `out[0]` _is the phase at the moment of the call. The accumulator wraps silently at 2^32, giving the full-resolution integer ramp that the scaled and carry variants derive from. Returns n._ |
|  size\_t | [**nco\_steps\_u32\_max\_out**](#function-nco_steps_u32_max_out) ([**nco\_state\_t**](structnco__state__t.md) \* state) <br>_Maximum samples per call (determines pre-allocated buffer size)._  |
|  size\_t | [**nco\_steps\_u32\_ovf**](#function-nco_steps_u32_ovf) ([**nco\_state\_t**](structnco__state__t.md) \* state, size\_t n, uint32\_t \* out, uint8\_t \* out1) <br>_Advance n samples; write raw phase values and per-sample carry. Identical to nco\_steps\_u32 for the phase array, but simultaneously fills a parallel uint8 carry buffer:_ `out1[i]` _is 1 if the add that produced_`out[i]` _'s post-increment phase wrapped past 2^32, else 0. The carry marks the exact boundary of one input period and is the primitive for polyphase sample-clock and rational resampling engines. Returns n._ |
|  size\_t | [**nco\_steps\_u32\_ovf\_max\_out**](#function-nco_steps_u32_ovf_max_out) ([**nco\_state\_t**](structnco__state__t.md) \* state) <br> |
|  size\_t | [**nco\_steps\_u32\_scaled**](#function-nco_steps_u32_scaled) ([**nco\_state\_t**](structnco__state__t.md) \* state, size\_t n, uint32\_t \* out) <br>_Advance n samples; values scaled to_ `[0, nmax)` _. Uses the branchless fixed-point identity_`out[i]` _= (uint64\_t)phase \* nmax &gt;&gt; 32 to map the full accumulator range uniformly onto [0, nmax) without a modulo operation. When nmax == 0 falls back to the raw accumulator (identical to nco\_steps\_u32). Useful for polyphase filter bank indexing and direct LUT addressing. Returns n._ |
|  size\_t | [**nco\_steps\_u32\_scaled\_max\_out**](#function-nco_steps_u32_scaled_max_out) ([**nco\_state\_t**](structnco__state__t.md) \* state) <br> |


## Public Static Functions

| Type | Name |
| ---: | :--- |
|  uint8\_t | [**nco\_add\_ovf\_**](#function-nco_add_ovf_) (uint32\_t a, uint32\_t b, uint32\_t \* res) <br>_Wrapping add with carry detection._  |

























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**NCO\_ADD\_OVF**](nco__core_8h.md#define-nco_add_ovf) (a, b, res) `nco\_add\_ovf\_ ((a), (b), (res))`<br> |
| define  | [**NCO\_STATE\_MAGIC**](nco__core_8h.md#define-nco_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('N', 'C', 'O', '\_')`<br> |
| define  | [**NCO\_STATE\_VERSION**](nco__core_8h.md#define-nco_state_version)  `1u`<br> |

## Detailed Description


Implements a numerically-controlled oscillator whose 32-bit phase register advances by phase\_inc every sample and wraps naturally at 2^32, giving exact integer arithmetic with no floating-point drift. Three output mappings expose different views of the accumulator:


nco\_steps\_u32 raw accumulator value `[0, 2^32)` nco\_steps\_u32\_scaled (uint64)phase \* nmax &gt;&gt; 32 → [0, nmax) nco\_steps\_u32\_ovf raw phase + per-sample carry flag


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

_Create an NCO instance. Allocates and initialises the phase accumulator to zero, converts norm\_freq to the integer phase\_inc = floor(frac(norm\_freq) × 2^32), and stores nmax for scaled output. The NCO is immediately ready to call nco\_steps\_u32 / nco\_steps\_u32\_scaled / nco\_steps\_u32\_ovf._ 
```C++
nco_state_t * nco_create (
    double norm_freq,
    uint32_t nmax
) 
```





**Parameters:**


* `norm_freq` Normalised frequency in cycles per sample. Any real value; only the fractional part matters. Negative values fold correctly (−0.25 → 3×2^30). 
* `nmax` Wrap target for nco\_steps\_u32\_scaled. Pass 0 to return the raw 32-bit accumulator. 



**Returns:**

Heap-allocated state, or NULL on allocation failure. 
```C++
>>> from doppler.source import NCO
>>> nco = NCO(norm_freq=0.25, nmax=0)
>>> nco.phase_inc
1073741824
```
 





        

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

_Normalised frequency (read/write). Setting norm\_freq recomputes phase\_inc = floor(frac(v) × 2^32) and takes effect on the next nco\_steps\_\* call; phase is NOT reset._ 
```C++
double nco_get_norm_freq (
    const nco_state_t * state
) 
```




```C++
>>> from doppler.source import NCO
>>> nco = NCO(0.25, 0)
>>> nco.norm_freq
0.25
>>> nco.norm_freq = 0.5
>>> nco.phase_inc
2147483648
```
 


        

<hr>



### function nco\_get\_phase 

_Current phase accumulator value (read/write). Reading returns the current integer phase in_ `[0, 2^32)` _. Writing overrides the accumulator directly, allowing arbitrary phase offsets without re-creating the NCO._
```C++
uint32_t nco_get_phase (
    const nco_state_t * state
) 
```




```C++
>>> from doppler.source import NCO
>>> nco = NCO(0.25, 0)
>>> nco.phase
0
>>> nco.phase = 2147483648
>>> nco.phase
2147483648
```
 


        

<hr>



### function nco\_get\_phase\_inc 

_Per-sample phase increment (read-only). Derived from norm\_freq as floor(frac(norm\_freq) × 2^32). Updated automatically whenever norm\_freq is written. A freq of 0.25 gives phase\_inc = 1073741824 (0x40000000)._ 
```C++
uint32_t nco_get_phase_inc (
    const nco_state_t * state
) 
```




```C++
>>> from doppler.source import NCO
>>> nco = NCO(0.25, 0)
>>> nco.phase_inc
1073741824
```
 


        

<hr>



### function nco\_get\_state 

_Serialize the phase accumulator into_ `blob` _._
```C++
void nco_get_state (
    const nco_state_t * state,
    void * blob
) 
```




<hr>



### function nco\_reset 

_Zero the phase accumulator. Sets phase to 0 so the next nco\_steps\_u32 call starts from the beginning of the cycle. norm\_freq, phase\_inc, and nmax are unchanged; the NCO is ready to generate samples again immediately._ 
```C++
void nco_reset (
    nco_state_t * state
) 
```




```C++
>>> from doppler.source import NCO
>>> nco = NCO(0.25, 0)
>>> _ = nco.steps_u32(2)
>>> nco.phase
2147483648
>>> nco.reset()
>>> nco.phase
0
>>> nco.norm_freq
0.25
```
 


        

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



### function nco\_set\_state 

_Restore phase; DP\_OK, or DP\_ERR\_INVALID if the envelope rejects._ 
```C++
int nco_set_state (
    nco_state_t * state,
    const void * blob
) 
```




<hr>



### function nco\_state\_bytes 

_Serialized-state byte size._ 
```C++
size_t nco_state_bytes (
    const nco_state_t * state
) 
```




<hr>



### function nco\_steps\_u32 

_Advance n samples; write raw uint32 accumulator values. Each element is the phase value BEFORE the increment fires, so_ `out[0]` _is the phase at the moment of the call. The accumulator wraps silently at 2^32, giving the full-resolution integer ramp that the scaled and carry variants derive from. Returns n._
```C++
size_t nco_steps_u32 (
    nco_state_t * state,
    size_t n,
    uint32_t * out
) 
```





**Parameters:**


* `state` NCO state returned by [**nco\_create()**](nco__core_8h.md#function-nco_create). 
* `n` Number of samples to generate. 
* `out` Output buffer; must hold at least n uint32\_t values. 



**Returns:**

n (always). 
```C++
>>> from doppler.source import NCO
>>> nco = NCO(0.25, 0)
>>> out = nco.steps_u32(4)
>>> out.dtype
dtype('uint32')
>>> out.tolist()
[0, 1073741824, 2147483648, 3221225472]
```
 





        

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

_Advance n samples; write raw phase values and per-sample carry. Identical to nco\_steps\_u32 for the phase array, but simultaneously fills a parallel uint8 carry buffer:_ `out1[i]` _is 1 if the add that produced_`out[i]` _'s post-increment phase wrapped past 2^32, else 0. The carry marks the exact boundary of one input period and is the primitive for polyphase sample-clock and rational resampling engines. Returns n._
```C++
size_t nco_steps_u32_ovf (
    nco_state_t * state,
    size_t n,
    uint32_t * out,
    uint8_t * out1
) 
```





**Parameters:**


* `state` NCO state returned by [**nco\_create()**](nco__core_8h.md#function-nco_create). 
* `n` Number of samples to generate. 
* `out` Phase output buffer; must hold at least n uint32\_t values. 
* `out1` Carry output buffer; must hold at least n uint8\_t values. 



**Returns:**

n (always). 
```C++
>>> from doppler.source import NCO
>>> nco = NCO(0.5, 0)
>>> ph, carry = nco.steps_u32_ovf(4)
>>> ph.tolist()
[0, 2147483648, 0, 2147483648]
>>> carry.tolist()
[0, 1, 0, 1]
>>> carry.dtype
dtype('uint8')
```
 





        

<hr>



### function nco\_steps\_u32\_ovf\_max\_out 

```C++
size_t nco_steps_u32_ovf_max_out (
    nco_state_t * state
) 
```




<hr>



### function nco\_steps\_u32\_scaled 

_Advance n samples; values scaled to_ `[0, nmax)` _. Uses the branchless fixed-point identity_`out[i]` _= (uint64\_t)phase \* nmax &gt;&gt; 32 to map the full accumulator range uniformly onto [0, nmax) without a modulo operation. When nmax == 0 falls back to the raw accumulator (identical to nco\_steps\_u32). Useful for polyphase filter bank indexing and direct LUT addressing. Returns n._
```C++
size_t nco_steps_u32_scaled (
    nco_state_t * state,
    size_t n,
    uint32_t * out
) 
```





**Parameters:**


* `state` NCO state returned by [**nco\_create()**](nco__core_8h.md#function-nco_create). 
* `n` Number of samples to generate. 
* `out` Output buffer; must hold at least n uint32\_t values. 



**Returns:**

n (always). 
```C++
>>> from doppler.source import NCO
>>> nco = NCO(0.25, 4)
>>> out = nco.steps_u32_scaled(4)
>>> out.dtype
dtype('uint32')
>>> out.tolist()
[0, 1, 2, 3]
```
 





        

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



### define NCO\_STATE\_MAGIC 

```C++
#define NCO_STATE_MAGIC `DP_FOURCC ('N', 'C', 'O', '_')`
```




<hr>



### define NCO\_STATE\_VERSION 

```C++
#define NCO_STATE_VERSION `1u`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/nco/nco_core.h`

