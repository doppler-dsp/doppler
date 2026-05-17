

# File nco.h



[**FileList**](files.md) **>** [**ddc**](dir_b33dc116452ac5c7d7799725e78b6bdc.md) **>** [**nco.h**](nco_8h.md)

[Go to the source code of this file](nco_8h_source.md)

_Numerically Controlled Oscillator (NCO)._ [More...](#detailed-description)

* `#include <complex.h>`
* `#include <stddef.h>`
* `#include <stdint.h>`

















## Public Types

| Type | Name |
| ---: | :--- |
| typedef struct dp\_nco | [**dp\_nco\_t**](#typedef-dp_nco_t)  <br>_Opaque NCO state._  |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**dp\_nco\_t**](nco_8h.md#typedef-dp_nco_t) \* | [**dp\_nco\_create**](#function-dp_nco_create) (float norm\_freq) <br>_Create an NCO at the given normalised frequency._  |
|  void | [**dp\_nco\_destroy**](#function-dp_nco_destroy) ([**dp\_nco\_t**](nco_8h.md#typedef-dp_nco_t) \* nco) <br>_Destroy the NCO and release all memory._  |
|  void | [**dp\_nco\_execute\_cf32**](#function-dp_nco_execute_cf32) ([**dp\_nco\_t**](nco_8h.md#typedef-dp_nco_t) \* nco, float \_Complex \* out, size\_t n) <br>_Generate_ `n` _complex samples from the free-running NCO._ |
|  void | [**dp\_nco\_execute\_cf32\_ctrl**](#function-dp_nco_execute_cf32_ctrl) ([**dp\_nco\_t**](nco_8h.md#typedef-dp_nco_t) \* nco, const float \* ctrl, float \_Complex \* out, size\_t n) <br>_Generate_ `n` _complex samples with a per-sample frequency deviation input (FM-modulator / phase-increment port)._ |
|  void | [**dp\_nco\_execute\_u32**](#function-dp_nco_execute_u32) ([**dp\_nco\_t**](nco_8h.md#typedef-dp_nco_t) \* nco, uint32\_t \* out, size\_t n) <br>_Output raw uint32 phase values, free-running._  |
|  void | [**dp\_nco\_execute\_u32\_ctrl**](#function-dp_nco_execute_u32_ctrl) ([**dp\_nco\_t**](nco_8h.md#typedef-dp_nco_t) \* nco, const float \* ctrl, uint32\_t \* out, size\_t n) <br>_Output raw uint32 phase values with per-sample ctrl port._  |
|  void | [**dp\_nco\_execute\_u32\_ovf**](#function-dp_nco_execute_u32_ovf) ([**dp\_nco\_t**](nco_8h.md#typedef-dp_nco_t) \* nco, uint32\_t \* out, uint8\_t \* carry, size\_t n) <br>_Output raw uint32 phase + carry bit, free-running._  |
|  void | [**dp\_nco\_execute\_u32\_ovf\_ctrl**](#function-dp_nco_execute_u32_ovf_ctrl) ([**dp\_nco\_t**](nco_8h.md#typedef-dp_nco_t) \* nco, const float \* ctrl, uint32\_t \* out, uint8\_t \* carry, size\_t n) <br>_Output raw uint32 phase + carry bit with ctrl port._  |
|  float | [**dp\_nco\_get\_freq**](#function-dp_nco_get_freq) (const [**dp\_nco\_t**](nco_8h.md#typedef-dp_nco_t) \* nco) <br>_Return the current normalised frequency._  |
|  uint32\_t | [**dp\_nco\_get\_phase**](#function-dp_nco_get_phase) (const [**dp\_nco\_t**](nco_8h.md#typedef-dp_nco_t) \* nco) <br>_Return the current raw 32-bit phase accumulator value._  |
|  uint32\_t | [**dp\_nco\_get\_phase\_inc**](#function-dp_nco_get_phase_inc) (const [**dp\_nco\_t**](nco_8h.md#typedef-dp_nco_t) \* nco) <br>_Return the phase increment (fixed-point frequency)._  |
|  void | [**dp\_nco\_reset**](#function-dp_nco_reset) ([**dp\_nco\_t**](nco_8h.md#typedef-dp_nco_t) \* nco) <br>_Reset the phase accumulator to zero._  |
|  void | [**dp\_nco\_set\_freq**](#function-dp_nco_set_freq) ([**dp\_nco\_t**](nco_8h.md#typedef-dp_nco_t) \* nco, float norm\_freq) <br>_Change the centre frequency without resetting the phase._  |




























## Detailed Description


Implements a phase-accurate NCO using a 32-bit unsigned overflowing phase accumulator and a 2^16-entry single-precision sine LUT.


The normalised frequency `f_n` maps to the accumulator as:
```C++
phase_inc = (uint32_t)(f_n * 2^32)
```
 where `f_n` = f / fs (cycles per sample). `f_n` = 0.5 produces the Nyquist tone; negative values produce conjugate-sense rotation. Values outside [−0.5, 0.5) are folded by unsigned wrap-around so the full ±Nyquist range is always reachable.


The control port (dp\_nco\_execute\_cf32\_ctrl) accepts a per-sample normalised-frequency deviation that is added to the base phase increment before each sample — standard FM-modulator topology:
```C++
inc_i     = phase_inc + (uint32_t)(ctrl[i] * 2^32)
phase    += inc_i
out[i]    = { cos(2π·phase/2^32), sin(2π·phase/2^32) }
```



Phase precision: 16 bits (top 16 bits of the 32-bit accumulator index the LUT; the lower 16 bits are truncated, giving a worst-case spurious level of ~−96 dBc). Amplitude accuracy is determined solely by float32 precision of sinf() at LUT construction time.


**Example — free-running quarter-rate tone:**
```C++
#include <dp/nco.h>

dp_nco_t      *nco = dp_nco_create(0.25f);  // f = fs/4
float _Complex out[256];
dp_nco_execute_cf32(nco, out, 256);
dp_nco_destroy(nco);
```



**Example — FM modulation:**
```C++
float mod[256] = { ... };           // normalised freq deviations
dp_nco_t      *nco = dp_nco_create(0.1f);
float _Complex out[256];
dp_nco_execute_cf32_ctrl(nco, mod, out, 256);
dp_nco_destroy(nco);
```




## Public Types Documentation




### typedef dp\_nco\_t

_Opaque NCO state._
```C++
typedef struct dp_nco dp_nco_t;
```




<hr>
## Public Functions Documentation




### function dp\_nco\_create

_Create an NCO at the given normalised frequency._
```C++
dp_nco_t * dp_nco_create (
    float norm_freq
)
```



Initialises the global 2^16-entry sine LUT on the first call (one-time cost; the table is never freed). The phase accumulator starts at zero.




**Parameters:**


* `norm_freq` Normalised frequency f/fs (cycles per sample). Typical range [−0.5, 0.5); values outside this range are folded via unsigned 32-bit arithmetic.



**Returns:**

Heap-allocated NCO state, or NULL on failure.







<hr>



### function dp\_nco\_destroy

_Destroy the NCO and release all memory._
```C++
void dp_nco_destroy (
    dp_nco_t * nco
)
```





**Parameters:**


* `nco` NCO state (may be NULL).






<hr>



### function dp\_nco\_execute\_cf32

_Generate_ `n` _complex samples from the free-running NCO._
```C++
void dp_nco_execute_cf32 (
    dp_nco_t * nco,
    float _Complex * out,
    size_t n
)
```



For each sample `i` the phase accumulator advances by `phase_inc` (set from the normalised frequency), then the output is read from the sine LUT:
```C++
out[i].i = cos(2π × phase / 2^32)   (in-phase)
out[i].q = sin(2π × phase / 2^32)   (quadrature)
```
 The accumulator wraps silently via uint32 overflow.




**Parameters:**


* `nco` NCO state.
* `out` Output array of `n` CF32 samples.
* `n` Number of samples to generate.






<hr>



### function dp\_nco\_execute\_cf32\_ctrl

_Generate_ `n` _complex samples with a per-sample frequency deviation input (FM-modulator / phase-increment port)._
```C++
void dp_nco_execute_cf32_ctrl (
    dp_nco_t * nco,
    const float * ctrl,
    float _Complex * out,
    size_t n
)
```



For each sample `i` the effective phase increment is:
```C++
inc_i = phase_inc + (uint32_t)(ctrl[i] × 2^32)
```
 where `ctrl`[i] is a normalised frequency deviation (same units as the `norm_freq` argument to dp\_nco\_create). The base phase increment stored in the NCO state is not modified; only the instantaneous frequency is perturbed.




**Parameters:**


* `nco` NCO state.
* `ctrl` Per-sample normalised-frequency deviations (length ≥ `n`).
* `out` Output array of `n` CF32 samples.
* `n` Number of samples to generate.






<hr>



### function dp\_nco\_execute\_u32

_Output raw uint32 phase values, free-running._
```C++
void dp_nco_execute_u32 (
    dp_nco_t * nco,
    uint32_t * out,
    size_t n
)
```





**Parameters:**


* `nco` NCO state.
* `out` Output array of `n` uint32\_t phase values.
* `n` Number of samples.






<hr>



### function dp\_nco\_execute\_u32\_ctrl

_Output raw uint32 phase values with per-sample ctrl port._
```C++
void dp_nco_execute_u32_ctrl (
    dp_nco_t * nco,
    const float * ctrl,
    uint32_t * out,
    size_t n
)
```





**Parameters:**


* `nco` NCO state.
* `ctrl` Per-sample normalised-frequency deviations.
* `out` Output array of `n` uint32\_t phase values.
* `n` Number of samples.






<hr>



### function dp\_nco\_execute\_u32\_ovf

_Output raw uint32 phase + carry bit, free-running._
```C++
void dp_nco_execute_u32_ovf (
    dp_nco_t * nco,
    uint32_t * out,
    uint8_t * carry,
    size_t n
)
```





**Parameters:**


* `nco` NCO state.
* `out` Output array of `n` uint32\_t phase values.
* `carry` Output array of `n` uint8\_t carry flags.
* `n` Number of samples.






<hr>



### function dp\_nco\_execute\_u32\_ovf\_ctrl

_Output raw uint32 phase + carry bit with ctrl port._
```C++
void dp_nco_execute_u32_ovf_ctrl (
    dp_nco_t * nco,
    const float * ctrl,
    uint32_t * out,
    uint8_t * carry,
    size_t n
)
```





**Parameters:**


* `nco` NCO state.
* `ctrl` Per-sample normalised-frequency deviations.
* `out` Output array of `n` uint32\_t phase values.
* `carry` Output array of `n` uint8\_t carry flags.
* `n` Number of samples.






<hr>



### function dp\_nco\_get\_freq

_Return the current normalised frequency._
```C++
float dp_nco_get_freq (
    const dp_nco_t * nco
)
```



Returns the value last passed to [**dp\_nco\_create()**](nco_8h.md#function-dp_nco_create) or [**dp\_nco\_set\_freq()**](nco_8h.md#function-dp_nco_set_freq) — no conversion from the phase accumulator.




**Parameters:**


* `nco` Must be non-NULL.






<hr>



### function dp\_nco\_get\_phase

_Return the current raw 32-bit phase accumulator value._
```C++
uint32_t dp_nco_get_phase (
    const dp_nco_t * nco
)
```



The phase advances by `phase_inc` on every execute tick. Convert to normalised units: `phase` / 2^32.




**Parameters:**


* `nco` Must be non-NULL.






<hr>



### function dp\_nco\_get\_phase\_inc

_Return the phase increment (fixed-point frequency)._
```C++
uint32_t dp_nco_get_phase_inc (
    const dp_nco_t * nco
)
```



This is the value added to the phase accumulator on every execute tick: `phase_inc` = round(norm\_freq × 2^32). Directly usable as the NCO step in a polyphase branch selector:
```C++
branch = (phase + phase_inc) >> (32 - log2(num_phases));
```





**Parameters:**


* `nco` Must be non-NULL.






<hr>



### function dp\_nco\_reset

_Reset the phase accumulator to zero._
```C++
void dp_nco_reset (
    dp_nco_t * nco
)
```



Does not change the centre frequency. Use after a stream discontinuity to restart from a known phase reference.




**Parameters:**


* `nco` NCO state (must be non-NULL).






<hr>



### function dp\_nco\_set\_freq

_Change the centre frequency without resetting the phase._
```C++
void dp_nco_set_freq (
    dp_nco_t * nco,
    float norm_freq
)
```



Takes effect on the next call to an execute function.




**Parameters:**


* `nco` NCO state (must be non-NULL).
* `norm_freq` New normalised frequency (same convention as dp\_nco\_create).






<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/ddc/nco.h`
