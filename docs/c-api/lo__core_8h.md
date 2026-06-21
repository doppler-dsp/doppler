

# File lo\_core.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**lo**](dir_e3bbeba8c021d4d74db794db08bafd77.md) **>** [**lo\_core.h**](lo__core_8h.md)

[Go to the source code of this file](lo__core_8h_source.md)

_Local oscillator: NCO + 2^16 sin/cos LUT → CF32 phasors._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "jm_perf.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**lo\_state\_t**](structlo__state__t.md) <br>_LO state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**lo\_state\_t**](structlo__state__t.md) \* | [**lo\_create**](#function-lo_create) (double norm\_freq) <br>_Create an LO instance. Allocates state, sets phase to 0, and derives phase\_inc from norm\_freq. Initialises the shared 65536-entry float LUT on the first call (single-threaded concern: call_ [_**lo\_create()**_](lo__core_8h.md#function-lo_create) _before spawning threads that share LO instances)._ |
|  void | [**lo\_destroy**](#function-lo_destroy) ([**lo\_state\_t**](structlo__state__t.md) \* state) <br> |
|  double | [**lo\_get\_norm\_freq**](#function-lo_get_norm_freq) (const [**lo\_state\_t**](structlo__state__t.md) \* state) <br>_Normalised frequency (read/write). Setting norm\_freq recomputes phase\_inc = floor(frac(v) × 2^32) and takes effect on the next lo\_steps call; phase is NOT reset._  |
|  uint32\_t | [**lo\_get\_phase**](#function-lo_get_phase) (const [**lo\_state\_t**](structlo__state__t.md) \* state) <br>_Current phase accumulator value (read/write). Returns the current integer phase in_ `[0, 2^32)` _. Writing overrides the accumulator directly for phase-coherent frequency switching._ |
|  uint32\_t | [**lo\_get\_phase\_inc**](#function-lo_get_phase_inc) (const [**lo\_state\_t**](structlo__state__t.md) \* state) <br>_Per-sample phase increment (read-only). Derived from norm\_freq as floor(frac(norm\_freq) × 2^32). A freq of 0.25 gives phase\_inc = 1073741824 (0x40000000)._  |
|  void | [**lo\_reset**](#function-lo_reset) ([**lo\_state\_t**](structlo__state__t.md) \* state) <br>_Zero the phase accumulator. Sets phase to 0 so the next lo\_steps call starts at angle 0 (1+0j). norm\_freq and phase\_inc are unchanged._  |
|  void | [**lo\_set\_norm\_freq**](#function-lo_set_norm_freq) ([**lo\_state\_t**](structlo__state__t.md) \* state, double norm\_freq) <br> |
|  void | [**lo\_set\_phase**](#function-lo_set_phase) ([**lo\_state\_t**](structlo__state__t.md) \* state, uint32\_t phase) <br> |
|  size\_t | [**lo\_steps**](#function-lo_steps) ([**lo\_state\_t**](structlo__state__t.md) \* state, size\_t n, float complex \* out) <br>_Generate n CF32 phasors at the current norm\_freq. Each sample is cos(θ) + j·sin(θ) where θ is the phase BEFORE the accumulator is advanced, giving a unit-magnitude complex sinusoid via the 65536-entry LUT. SFDR ≈ 96 dBc. Returns n._  |
|  size\_t | [**lo\_steps\_ctrl**](#function-lo_steps_ctrl) ([**lo\_state\_t**](structlo__state__t.md) \* state, const float \* ctrl, size\_t ctrl\_len, float complex \* out) <br>_Generate CF32 phasors with per-sample FM deviation. For each sample i,_ `ctrl[i]` _'s fractional part is converted to a delta phase-increment (delta = floor(frac(_`ctrl[i]` _) × 2^32)) that is added on top of the base phase\_inc for that one step only. The base norm\_freq and phase\_inc are NOT modified; the deviation is transient per sample, making this the natural API for FM synthesis and frequency-hopping. Output length equals ctrl\_len. Returns ctrl\_len._ |
|  size\_t | [**lo\_steps\_ctrl\_max\_out**](#function-lo_steps_ctrl_max_out) ([**lo\_state\_t**](structlo__state__t.md) \* state) <br> |
|  size\_t | [**lo\_steps\_max\_out**](#function-lo_steps_max_out) ([**lo\_state\_t**](structlo__state__t.md) \* state) <br>_Maximum samples per call (determines pre-allocated buffer size)._  |




























## Detailed Description


Wraps the integer NCO in a CF32 phasor generator. The 32-bit phase accumulator drives a static 65536-entry float sine LUT; the top 16 bits of the phase select the LUT index, and a quarter-cycle offset (LUT\_QTR = 16384) converts sin to cos without extra storage:


idx = phase &gt;&gt; 16 out(i) = cos(θ) + j·sin(θ) = lut((idx + LUT\_QTR) & 0xFFFF) + j·lut(idx)


Output is emitted BEFORE the phase is incremented (same convention as NCO). The 16-bit phase truncation gives ~96 dBc SFDR.


The shared LUT is initialised lazily on the first [**lo\_create()**](lo__core_8h.md#function-lo_create) call.


Lifecycle: lo\_create → (steps / steps\_ctrl / reset)\* → lo\_destroy



```C++
lo_state_t *lo = lo_create(0.25);
float complex out[4];
lo_steps(lo, 4, out);
// out ≈ { 1+0j, 0+1j, -1+0j, 0-1j }
lo_destroy(lo);
```
 


    
## Public Functions Documentation




### function lo\_create 

_Create an LO instance. Allocates state, sets phase to 0, and derives phase\_inc from norm\_freq. Initialises the shared 65536-entry float LUT on the first call (single-threaded concern: call_ [_**lo\_create()**_](lo__core_8h.md#function-lo_create) _before spawning threads that share LO instances)._
```C++
lo_state_t * lo_create (
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



### function lo\_destroy 

```C++
void lo_destroy (
    lo_state_t * state
) 
```



Free all resources. May be NULL (no-op). 


        

<hr>



### function lo\_get\_norm\_freq 

_Normalised frequency (read/write). Setting norm\_freq recomputes phase\_inc = floor(frac(v) × 2^32) and takes effect on the next lo\_steps call; phase is NOT reset._ 
```C++
double lo_get_norm_freq (
    const lo_state_t * state
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



### function lo\_get\_phase 

_Current phase accumulator value (read/write). Returns the current integer phase in_ `[0, 2^32)` _. Writing overrides the accumulator directly for phase-coherent frequency switching._
```C++
uint32_t lo_get_phase (
    const lo_state_t * state
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



### function lo\_get\_phase\_inc 

_Per-sample phase increment (read-only). Derived from norm\_freq as floor(frac(norm\_freq) × 2^32). A freq of 0.25 gives phase\_inc = 1073741824 (0x40000000)._ 
```C++
uint32_t lo_get_phase_inc (
    const lo_state_t * state
) 
```




```C++
>>> from doppler.source import LO
>>> lo = LO(0.25)
>>> lo.phase_inc
1073741824
```
 


        

<hr>



### function lo\_reset 

_Zero the phase accumulator. Sets phase to 0 so the next lo\_steps call starts at angle 0 (1+0j). norm\_freq and phase\_inc are unchanged._ 
```C++
void lo_reset (
    lo_state_t * state
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



### function lo\_set\_norm\_freq 

```C++
void lo_set_norm_freq (
    lo_state_t * state,
    double norm_freq
) 
```




<hr>



### function lo\_set\_phase 

```C++
void lo_set_phase (
    lo_state_t * state,
    uint32_t phase
) 
```




<hr>



### function lo\_steps 

_Generate n CF32 phasors at the current norm\_freq. Each sample is cos(θ) + j·sin(θ) where θ is the phase BEFORE the accumulator is advanced, giving a unit-magnitude complex sinusoid via the 65536-entry LUT. SFDR ≈ 96 dBc. Returns n._ 
```C++
size_t lo_steps (
    lo_state_t * state,
    size_t n,
    float complex * out
) 
```





**Parameters:**


* `state` LO state returned by [**lo\_create()**](lo__core_8h.md#function-lo_create). 
* `n` Number of phasors to generate. 
* `out` Output buffer; must hold at least n float complex values. 



**Returns:**

n (always). 
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



### function lo\_steps\_ctrl 

_Generate CF32 phasors with per-sample FM deviation. For each sample i,_ `ctrl[i]` _'s fractional part is converted to a delta phase-increment (delta = floor(frac(_`ctrl[i]` _) × 2^32)) that is added on top of the base phase\_inc for that one step only. The base norm\_freq and phase\_inc are NOT modified; the deviation is transient per sample, making this the natural API for FM synthesis and frequency-hopping. Output length equals ctrl\_len. Returns ctrl\_len._
```C++
size_t lo_steps_ctrl (
    lo_state_t * state,
    const float * ctrl,
    size_t ctrl_len,
    float complex * out
) 
```





**Parameters:**


* `state` LO state returned by [**lo\_create()**](lo__core_8h.md#function-lo_create). 
* `ctrl` Float32 array of per-sample normalised-frequency deviations. Only the fractional part of each element contributes. 
* `ctrl_len` Number of elements in ctrl; equals output length. 
* `out` Output buffer; must hold at least ctrl\_len float complex values. 



**Returns:**

ctrl\_len (always). 
```C++
>>> import numpy as np
>>> from doppler.source import LO
>>> lo = LO(0.25)
>>> ctrl = np.zeros(4, dtype=np.float32)
>>> out = lo.steps_ctrl(ctrl)
>>> out.dtype
dtype('complex64')
>>> out.shape
(4,)
>>> [round(float(abs(c)), 4) for c in out]
[1.0, 1.0, 1.0, 1.0]
```
 





        

<hr>



### function lo\_steps\_ctrl\_max\_out 

```C++
size_t lo_steps_ctrl_max_out (
    lo_state_t * state
) 
```




<hr>



### function lo\_steps\_max\_out 

_Maximum samples per call (determines pre-allocated buffer size)._ 
```C++
size_t lo_steps_max_out (
    lo_state_t * state
) 
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/lo/lo_core.h`

