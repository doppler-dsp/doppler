

# File lo\_core.h



[**FileList**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**lo**](dir_939b5fbb8d3e3ebd3276389efab5bbba.md) **>** [**lo\_core.h**](lo__core_8h.md)

[Go to the source code of this file](lo__core_8h_source.md)

_Local oscillator: NCO + 2^16 sin/cos LUT → CF32 phasors._ [More...](#detailed-description)

* `#include "doppler/clib_common.h"`
* `#include "doppler/dp_state.h"`
* `#include "doppler/jm_perf.h"`
* `#include "doppler/nco/nco_core.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**dp\_lo\_state\_t**](structdp__lo__state__t.md) <br>_LO state._  |






## Public Attributes

| Type | Name |
| ---: | :--- |
|  float | [**lo\_sin\_lut**](#variable-lo_sin_lut)  <br>_Shared 2^16-entry sine LUT (read-only after init)._  |
















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**dp\_lo\_state\_t**](structdp__lo__state__t.md) \* | [**dp\_lo\_create**](#function-dp_lo_create) (double norm\_freq) <br>_Create an LO instance. Allocates state, sets phase to 0, and derives phase\_inc from norm\_freq. Initialises the shared 65536-entry float LUT on the first call (single-threaded concern: call_ [_**dp\_lo\_create()**_](lo__core_8h.md#function-dp_lo_create) _before spawning threads that share LO instances)._ |
|  void | [**dp\_lo\_destroy**](#function-dp_lo_destroy) ([**dp\_lo\_state\_t**](structdp__lo__state__t.md) \* state) <br> |
|  double | [**dp\_lo\_get\_norm\_freq**](#function-dp_lo_get_norm_freq) (const [**dp\_lo\_state\_t**](structdp__lo__state__t.md) \* state) <br>_Normalised frequency (read/write). Setting norm\_freq recomputes phase\_inc = floor(frac(v) × 2^32) and takes effect on the next dp\_lo\_steps call; phase is NOT reset._  |
|  uint32\_t | [**dp\_lo\_get\_phase**](#function-dp_lo_get_phase) (const [**dp\_lo\_state\_t**](structdp__lo__state__t.md) \* state) <br>_Current phase accumulator value (read/write). Returns the current integer phase in_ `[0, 2^32)` _. Writing overrides the accumulator directly for phase-coherent frequency switching._ |
|  uint32\_t | [**dp\_lo\_get\_phase\_inc**](#function-dp_lo_get_phase_inc) (const [**dp\_lo\_state\_t**](structdp__lo__state__t.md) \* state) <br>_Per-sample phase increment (read-only). Derived from norm\_freq as floor(frac(norm\_freq) × 2^32). A freq of 0.25 gives phase\_inc = 1073741824 (0x40000000)._  |
|  void | [**dp\_lo\_get\_state**](#function-dp_lo_get_state) (const [**dp\_lo\_state\_t**](structdp__lo__state__t.md) \* state, void \* blob) <br>_Serialize_ `state's` _mutable state into_`blob` _(&gt;= dp\_lo\_state\_bytes)._ |
|  void | [**dp\_lo\_reset**](#function-dp_lo_reset) ([**dp\_lo\_state\_t**](structdp__lo__state__t.md) \* state) <br>_Zero the phase accumulator. Sets phase to 0 so the next dp\_lo\_steps call starts at angle 0 (1+0j). norm\_freq and phase\_inc are unchanged._  |
|  void | [**dp\_lo\_set\_norm\_freq**](#function-dp_lo_set_norm_freq) ([**dp\_lo\_state\_t**](structdp__lo__state__t.md) \* state, double norm\_freq) <br> |
|  void | [**dp\_lo\_set\_phase**](#function-dp_lo_set_phase) ([**dp\_lo\_state\_t**](structdp__lo__state__t.md) \* state, uint32\_t phase) <br> |
|  int | [**dp\_lo\_set\_state**](#function-dp_lo_set_state) ([**dp\_lo\_state\_t**](structdp__lo__state__t.md) \* state, const void \* blob) <br>_Restore mutable state from_ `blob` _._ |
|  size\_t | [**dp\_lo\_state\_bytes**](#function-dp_lo_state_bytes) (const [**dp\_lo\_state\_t**](structdp__lo__state__t.md) \* state) <br>_Bytes_ [_**dp\_lo\_get\_state()**_](lo__core_8h.md#function-dp_lo_get_state) _writes for_`state` _(envelope + payload)._ |
|  size\_t | [**dp\_lo\_steps**](#function-dp_lo_steps) ([**dp\_lo\_state\_t**](structdp__lo__state__t.md) \* state, size\_t n, float \_Complex \* out, size\_t max\_out) <br>_Generate n CF32 phasors at the current norm\_freq. Each sample is cos(θ) + j·sin(θ) where θ is the phase BEFORE the accumulator is advanced, giving a unit-magnitude complex sinusoid via the 65536-entry LUT. SFDR is ≥ 90 dBc at any frequency and ~96 dBc at a typical one — see the file header for why those are two different numbers. Returns n._  |
|  size\_t | [**dp\_lo\_steps\_ctrl**](#function-dp_lo_steps_ctrl) ([**dp\_lo\_state\_t**](structdp__lo__state__t.md) \* state, const double \* ctrl, size\_t ctrl\_len, float \_Complex \* out, size\_t max\_out) <br>_Generate CF32 phasors with per-sample FM deviation. For each sample i,_ `ctrl[i]` _'s fractional part is converted to a delta phase-increment (delta = floor(frac(_`ctrl[i]` _) × 2^32)) that is added on top of the base phase\_inc for that one step only. The base norm\_freq and phase\_inc are NOT modified; the deviation is transient per sample, making this the natural API for FM synthesis and frequency-hopping. Output length equals ctrl\_len. Returns ctrl\_len._ |
|  size\_t | [**dp\_lo\_steps\_ctrl\_max\_out**](#function-dp_lo_steps_ctrl_max_out) ([**dp\_lo\_state\_t**](structdp__lo__state__t.md) \* state) <br> |
|  size\_t | [**dp\_lo\_steps\_max\_out**](#function-dp_lo_steps_max_out) ([**dp\_lo\_state\_t**](structdp__lo__state__t.md) \* state) <br>_Maximum samples per call (determines pre-allocated buffer size)._  |
|  void | [**lo\_init**](#function-lo_init) ([**dp\_lo\_state\_t**](structdp__lo__state__t.md) \* state, double norm\_freq) <br>_Initialise an LO in place (no allocation)._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) float \_Complex | [**lo\_step**](#function-lo_step) ([**dp\_lo\_state\_t**](structdp__lo__state__t.md) \* state) <br>_Emit the current CF32 phasor, then advance the accumulator._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) float \_Complex | [**lo\_step\_ctrl**](#function-lo_step_ctrl) ([**dp\_lo\_state\_t**](structdp__lo__state__t.md) \* state, double ctrl) <br>_Emit the current CF32 phasor, then advance by phase\_inc + control._  |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**LO\_LUT\_BITS**](lo__core_8h.md#define-lo_lut_bits)  `16u`<br> |
| define  | [**LO\_LUT\_QTR**](lo__core_8h.md#define-lo_lut_qtr)  `([**LO\_LUT\_SIZE**](lo__core_8h.md#define-lo_lut_size) &gt;&gt; 2u)  /\* 16384  (π/2 phase shift) \*/`<br> |
| define  | [**LO\_LUT\_SIZE**](lo__core_8h.md#define-lo_lut_size)  `(1u &lt;&lt; [**LO\_LUT\_BITS**](lo__core_8h.md#define-lo_lut_bits)) /\* 65536                    \*/`<br> |
| define  | [**LO\_STATE\_MAGIC**](lo__core_8h.md#define-lo_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('L', 'O', '\_', '\_')`<br> |
| define  | [**LO\_STATE\_VERSION**](lo__core_8h.md#define-lo_state_version)  `1u`<br> |

## Detailed Description


Wraps the integer NCO in a CF32 phasor generator. The 32-bit phase accumulator drives a static 65536-entry float sine LUT; the top 16 bits of the phase select the LUT index, and a quarter-cycle offset (LUT\_QTR = 16384) converts sin to cos without extra storage:


idx = phase &gt;&gt; 16 out(i) = cos(θ) + j·sin(θ) = lut((idx + LUT\_QTR) & 0xFFFF) + j·lut(idx)


Output is emitted BEFORE the phase is incremented (same convention as NCO).


### Spurious content — what the 16-bit index costs



The index keeps the top 16 bits of a 32-bit phase, so unless the increment is a whole number of LUT bins there is a per-sample phase error, and that error is periodic: its period is set by the LOW 16 bits of phase\_inc, NOT by the frequency. Three regimes, measured (see src/doppler/source/tests/validation/lo/results.md, which regenerates them):


phase\_inc & 0xFFFF == 0 no truncation at all; spur-free to the float32 floor, ~146 dBc a generic remainder ~96 dBc, flat  400 random rates span 96.32 to 96.33 dBc remainder == 0x8000 (half a 92.4 dBc: the error alternates with bin), and small-denominator period 2 and all of it lands in one remainders near it spur. This is the classical 6.02\*B - 3.92 phase-truncation bound.


**The guarantee is SFDR &gt;= 90 dBc at any frequency** (worst measured 92.40, over 8 carrier positions x 2 capture lengths). The familiar ~96 dBc is the TYPICAL figure, not a bound  a design sizing its spur budget must use 90.


Amplitude quantization is not a contributor: the table is float32, so \|phasor\| - 1 stays under 6e-08, four orders below the half-bin phase error of 0.5/65536 cycles.


The shared LUT is initialised lazily on the first [**dp\_lo\_create()**](lo__core_8h.md#function-dp_lo_create) call.


Lifecycle: dp\_lo\_create → (steps / steps\_ctrl / reset)\* → dp\_lo\_destroy



```C++
dp_lo_state_t *lo = dp_lo_create(0.25);
float _Complex out[4];
dp_lo_steps (lo, 4, out, 4);
// out ≈ { 1+0j, 0+1j, -1+0j, 0-1j }
dp_lo_destroy(lo);
```
 



    
## Public Attributes Documentation




### variable lo\_sin\_lut 

_Shared 2^16-entry sine LUT (read-only after init)._ 
```C++
float lo_sin_lut[LO_LUT_SIZE];
```



Filled by the first [**dp\_lo\_create()**](lo__core_8h.md#function-dp_lo_create)/lo\_init(). Indexed by the top 16 bits of the phase accumulator; the quarter-cycle offset LO\_LUT\_QTR maps sin→cos. Do not write. Exposed only so [**lo\_step()**](lo__core_8h.md#function-lo_step) can be a header inline. 


        

<hr>
## Public Functions Documentation




### function dp\_lo\_create 

_Create an LO instance. Allocates state, sets phase to 0, and derives phase\_inc from norm\_freq. Initialises the shared 65536-entry float LUT on the first call (single-threaded concern: call_ [_**dp\_lo\_create()**_](lo__core_8h.md#function-dp_lo_create) _before spawning threads that share LO instances)._
```C++
dp_lo_state_t * dp_lo_create (
    double norm_freq
) 
```





**Parameters:**


* `norm_freq` Normalised frequency in cycles per sample. Any real value; only the fractional part matters. 



**Returns:**

Heap-allocated state, or NULL on allocation failure. 
```C++
>>> from doppler.source import LO
>>> lo = LO(norm_freq=0.25)
>>> lo.phase_inc
1073741824
```
 





        

<hr>



### function dp\_lo\_destroy 

```C++
void dp_lo_destroy (
    dp_lo_state_t * state
) 
```



Free all resources. May be NULL (no-op). 


        

<hr>



### function dp\_lo\_get\_norm\_freq 

_Normalised frequency (read/write). Setting norm\_freq recomputes phase\_inc = floor(frac(v) × 2^32) and takes effect on the next dp\_lo\_steps call; phase is NOT reset._ 
```C++
double dp_lo_get_norm_freq (
    const dp_lo_state_t * state
) 
```




```C++
>>> from doppler.source import LO
>>> lo = LO(0.25)
>>> lo.norm_freq
0.25
>>> lo.norm_freq = 0.5
>>> lo.phase_inc
2147483648
```
 


        

<hr>



### function dp\_lo\_get\_phase 

_Current phase accumulator value (read/write). Returns the current integer phase in_ `[0, 2^32)` _. Writing overrides the accumulator directly for phase-coherent frequency switching._
```C++
uint32_t dp_lo_get_phase (
    const dp_lo_state_t * state
) 
```




```C++
>>> from doppler.source import LO
>>> lo = LO(0.25)
>>> lo.phase
0
>>> lo.phase = 1073741824
>>> lo.phase
1073741824
```
 


        

<hr>



### function dp\_lo\_get\_phase\_inc 

_Per-sample phase increment (read-only). Derived from norm\_freq as floor(frac(norm\_freq) × 2^32). A freq of 0.25 gives phase\_inc = 1073741824 (0x40000000)._ 
```C++
uint32_t dp_lo_get_phase_inc (
    const dp_lo_state_t * state
) 
```




```C++
>>> from doppler.source import LO
>>> lo = LO(0.25)
>>> lo.phase_inc
1073741824
```
 


        

<hr>



### function dp\_lo\_get\_state 

_Serialize_ `state's` _mutable state into_`blob` _(&gt;= dp\_lo\_state\_bytes)._
```C++
void dp_lo_get_state (
    const dp_lo_state_t * state,
    void * blob
) 
```




<hr>



### function dp\_lo\_reset 

_Zero the phase accumulator. Sets phase to 0 so the next dp\_lo\_steps call starts at angle 0 (1+0j). norm\_freq and phase\_inc are unchanged._ 
```C++
void dp_lo_reset (
    dp_lo_state_t * state
) 
```




```C++
>>> from doppler.source import LO
>>> lo = LO(0.25)
>>> _ = lo.steps(2)
>>> lo.phase
2147483648
>>> lo.reset()
>>> lo.phase
0
>>> lo.norm_freq
0.25
```
 


        

<hr>



### function dp\_lo\_set\_norm\_freq 

```C++
void dp_lo_set_norm_freq (
    dp_lo_state_t * state,
    double norm_freq
) 
```




<hr>



### function dp\_lo\_set\_phase 

```C++
void dp_lo_set_phase (
    dp_lo_state_t * state,
    uint32_t phase
) 
```




<hr>



### function dp\_lo\_set\_state 

_Restore mutable state from_ `blob` _._
```C++
int dp_lo_set_state (
    dp_lo_state_t * state,
    const void * blob
) 
```





**Returns:**

DP\_OK, or DP\_ERR\_INVALID if the blob's envelope rejects. 





        

<hr>



### function dp\_lo\_state\_bytes 

_Bytes_ [_**dp\_lo\_get\_state()**_](lo__core_8h.md#function-dp_lo_get_state) _writes for_`state` _(envelope + payload)._
```C++
size_t dp_lo_state_bytes (
    const dp_lo_state_t * state
) 
```




<hr>



### function dp\_lo\_steps 

_Generate n CF32 phasors at the current norm\_freq. Each sample is cos(θ) + j·sin(θ) where θ is the phase BEFORE the accumulator is advanced, giving a unit-magnitude complex sinusoid via the 65536-entry LUT. SFDR is ≥ 90 dBc at any frequency and ~96 dBc at a typical one — see the file header for why those are two different numbers. Returns n._ 
```C++
size_t dp_lo_steps (
    dp_lo_state_t * state,
    size_t n,
    float _Complex * out,
    size_t max_out
) 
```





**Parameters:**


* `state` LO state returned by [**dp\_lo\_create()**](lo__core_8h.md#function-dp_lo_create). 
* `n` Number of phasors to generate. 
* `out` Output buffer; must hold at least n float \_Complex values. 
* `max_out` Capacity of `out` in elements. Emission stops there, so the return value is the number actually written. 



**Returns:**

min(n, max\_out) samples. 
```C++
>>> from doppler.source import LO
>>> lo = LO(0.25)
>>> out = lo.steps(4)
>>> out.dtype
dtype('complex64')
>>> out.shape
(4,)
>>> [round(float(abs(c)), 4) for c in out]
[1.0, 1.0, 1.0, 1.0]
```
 





        

<hr>



### function dp\_lo\_steps\_ctrl 

_Generate CF32 phasors with per-sample FM deviation. For each sample i,_ `ctrl[i]` _'s fractional part is converted to a delta phase-increment (delta = floor(frac(_`ctrl[i]` _) × 2^32)) that is added on top of the base phase\_inc for that one step only. The base norm\_freq and phase\_inc are NOT modified; the deviation is transient per sample, making this the natural API for FM synthesis and frequency-hopping. Output length equals ctrl\_len. Returns ctrl\_len._
```C++
size_t dp_lo_steps_ctrl (
    dp_lo_state_t * state,
    const double * ctrl,
    size_t ctrl_len,
    float _Complex * out,
    size_t max_out
) 
```





**Parameters:**


* `state` LO state returned by [**dp\_lo\_create()**](lo__core_8h.md#function-dp_lo_create). 
* `ctrl` Per-sample normalised-frequency deviations in `double`. Only the fractional part of each element contributes. See [**dp\_nco\_steps\_u32\_ctrl()**](nco__core_8h.md#function-dp_nco_steps_u32_ctrl) on why the port is `double` and not float32. 
* `ctrl_len` Number of elements in ctrl; equals output length. 
* `out` Output buffer; must hold at least ctrl\_len float \_Complex values. 
* `max_out` Capacity of `out` in elements. Emission stops there, so the return value is the number actually written. 



**Returns:**

min(ctrl\_len, max\_out) samples. 
```C++
>>> import numpy as np
>>> from doppler.source import LO
>>> lo = LO(0.25)
>>> ctrl = np.zeros(4, dtype=np.float64)
>>> out = lo.steps_ctrl(ctrl)
>>> out.dtype
dtype('complex64')
>>> out.shape
(4,)
>>> [round(float(abs(c)), 4) for c in out]
[1.0, 1.0, 1.0, 1.0]
```
 





        

<hr>



### function dp\_lo\_steps\_ctrl\_max\_out 

```C++
size_t dp_lo_steps_ctrl_max_out (
    dp_lo_state_t * state
) 
```




<hr>



### function dp\_lo\_steps\_max\_out 

_Maximum samples per call (determines pre-allocated buffer size)._ 
```C++
size_t dp_lo_steps_max_out (
    dp_lo_state_t * state
) 
```




<hr>



### function lo\_init 

_Initialise an LO in place (no allocation)._ 
```C++
void lo_init (
    dp_lo_state_t * state,
    double norm_freq
) 
```



The by-value counterpart to [**dp\_lo\_create()**](lo__core_8h.md#function-dp_lo_create): a tracking loop that embeds an [**dp\_lo\_state\_t**](structdp__lo__state__t.md) initialises it with [**lo\_init()**](lo__core_8h.md#function-lo_init) instead of owning a heap pointer. Sets phase=0, derives phase\_inc from norm\_freq, and fills the shared LUT on first use (same single-threaded caveat as [**dp\_lo\_create()**](lo__core_8h.md#function-dp_lo_create)).




**Parameters:**


* `state` LO state to initialise in place. Must be non-NULL. 
* `norm_freq` Normalised frequency in cycles per sample (fractional part only). 
```C++
>>> from doppler.source import LO
>>> lo = LO(0.25)          # the Python type calls dp_lo_create
>>> lo.phase_inc
1073741824
```
 




        

<hr>



### function lo\_step 

_Emit the current CF32 phasor, then advance the accumulator._ 
```C++
JM_FORCEINLINE  JM_HOT float _Complex lo_step (
    dp_lo_state_t * state
) 
```



Single-sample form of [**dp\_lo\_steps()**](lo__core_8h.md#function-dp_lo_steps), same emit-before-increment convention and bit-for-bit the same LUT math, suitable for inlining into a sample-by-sample loop (e.g. carrier wipe-off ahead of a matched filter). The caller must have run [**dp\_lo\_create()**](lo__core_8h.md#function-dp_lo_create)/lo\_init() so the LUT is populated.




**Parameters:**


* `state` LO state. Must be non-NULL with phase/phase\_inc set. 



**Returns:**

cos(θ) + j·sin(θ) at the phase BEFORE the increment. 
```C++
dp_lo_state_t lo;            // embedded by value, no heap
lo_init (&lo, 0.25);
float _Complex s0 = lo_step (&lo);   // 1 + 0j
float _Complex s1 = lo_step (&lo);   // 0 + 1j
```
 





        

<hr>



### function lo\_step\_ctrl 

_Emit the current CF32 phasor, then advance by phase\_inc + control._ 
```C++
JM_FORCEINLINE  JM_HOT float _Complex lo_step_ctrl (
    dp_lo_state_t * state,
    double ctrl
) 
```



The NCO **control port** for a tracking loop: `ctrl` is a per-sample frequency control in normalized cycles/sample, added on top of the centre increment `phase_inc` for this step only (not persisted — the loop filter holds the integrator and supplies its full output as `ctrl` each sample). The LO owns the cycles→phase scaling, so the loop never touches the integer phase accumulator. Same emit-before-increment convention as [**lo\_step()**](lo__core_8h.md#function-lo_step); with `ctrl` == 0 it is bit-identical to [**lo\_step()**](lo__core_8h.md#function-lo_step).




**Parameters:**


* `state` LO state. Must be non-NULL with phase/phase\_inc set. 
* `ctrl` Frequency control, normalized cycles/sample (any sign; the fractional cycle is taken, so it wraps correctly). 



**Returns:**

cos(θ) + j·sin(θ) at the phase BEFORE the increment. 
```C++
dp_lo_state_t lo;
lo_init (&lo, 0.0);                 // centre at DC
float _Complex s = lo_step_ctrl (&lo, 0.01);  // step at +0.01 cyc/sample
```
 





        

<hr>
## Macro Definition Documentation





### define LO\_LUT\_BITS 

```C++
#define LO_LUT_BITS `16u`
```




<hr>



### define LO\_LUT\_QTR 

```C++
#define LO_LUT_QTR `( LO_LUT_SIZE >> 2u)  /* 16384  (π/2 phase shift) */`
```




<hr>



### define LO\_LUT\_SIZE 

```C++
#define LO_LUT_SIZE `(1u << LO_LUT_BITS ) /* 65536                    */`
```




<hr>



### define LO\_STATE\_MAGIC 

```C++
#define LO_STATE_MAGIC `DP_FOURCC ('L', 'O', '_', '_')`
```




<hr>



### define LO\_STATE\_VERSION 

```C++
#define LO_STATE_VERSION `1u`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/lo/lo_core.h`

