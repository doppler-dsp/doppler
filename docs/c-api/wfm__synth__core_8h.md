

# File wfm\_synth\_core.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**wfm\_synth**](dir_0493917d169dff974fa9eaf690c8d4c9.md) **>** [**wfm\_synth\_core.h**](wfm__synth__core_8h.md)

[Go to the source code of this file](wfm__synth__core_8h_source.md)

_Synth component API._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "jm_perf.h"`
* `#include "fir/fir_core.h"`
* `#include "lo/lo_core.h"`
* `#include "awgn/awgn_core.h"`
* `#include "pn/pn_core.h"`
* `#include <math.h>`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**wfm\_synth\_state\_t**](structwfm__synth__state__t.md) <br>_Synth state._  |


## Public Types

| Type | Name |
| ---: | :--- |
| enum  | [**wfm\_\_synth\_\_core\_8h\_1abd477555e01841805289c5cf8e4e76fb**](#enum-wfm__synth__core_8h_1abd477555e01841805289c5cf8e4e76fb)  <br> |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**wfm\_synth\_state\_t**](structwfm__synth__state__t.md) \* | [**wfm\_synth\_create**](#function-wfm_synth_create) (int type, double fs, double freq, double snr, int snr\_mode, uint32\_t seed, int sps, int pn\_length, uint64\_t pn\_poly, int lfsr, double f\_end) <br>_Allocate and configure a waveform synthesiser. The synthesiser combines a local oscillator (LO), optional AWGN, and an optional PN LFSR into a single streaming source. One call to_ [_**wfm\_synth\_step()**_](wfm__synth__core_8h.md#function-wfm_synth_step) _or_[_**wfm\_synth\_steps()**_](wfm__synth__core_8h.md#function-wfm_synth_steps) _advances all sub-components in lock-step. SNR &gt;= WFM\_SYNTH\_SNR\_CLEAN (100 dB) skips AWGN entirely — clean waveforms pay no noise overhead. When_`snr_mode` _is "auto" the library picks the natural reference: Es/No for modulated types (BPSK, QPSK), fs-band SNR for tone/noise/PN._ |
|  void | [**wfm\_synth\_destroy**](#function-wfm_synth_destroy) ([**wfm\_synth\_state\_t**](structwfm__synth__state__t.md) \* state) <br>_Destroy a synth instance and release all memory. Recursively frees the LO, AWGN, and PN sub-objects, then the struct itself. Safe to call with NULL (no-op)._  |
|  float | [**wfm\_synth\_get\_cur\_im**](#function-wfm_synth_get_cur_im) (const [**wfm\_synth\_state\_t**](structwfm__synth__state__t.md) \* state) <br>_Return the imaginary part of the current held symbol. For QPSK this is the Q component (±1/√2); for BPSK/PN it is always 0; for tone/noise it is 0._  |
|  float | [**wfm\_synth\_get\_cur\_re**](#function-wfm_synth_get_cur_re) (const [**wfm\_synth\_state\_t**](structwfm__synth__state__t.md) \* state) <br>_Return the real part of the current held symbol. For modulated types this is the I component latched at the last symbol boundary (±1 for BPSK/PN, ±1/√2 for QPSK). For tone the synthesiser initialises cur\_re to 1.0 so that the held symbol is a clean unit-power carrier; for noise it is 0.0 (noise has no held symbol)._  |
|  int | [**wfm\_synth\_get\_nsps**](#function-wfm_synth_get_nsps) (const [**wfm\_synth\_state\_t**](structwfm__synth__state__t.md) \* state) <br>_Return the samples-per-symbol count. For modulated types (BPSK, QPSK, PN) each symbol is held for nsps consecutive output samples. For tone/noise this field is present but unused by the synthesis path._  |
|  int | [**wfm\_synth\_get\_sym\_pos**](#function-wfm_synth_get_sym_pos) (const [**wfm\_synth\_state\_t**](structwfm__synth__state__t.md) \* state) <br>_Return the current position within the current symbol (0..nsps-1). Reaches nsps and wraps to 0 each time a new symbol is consumed from the PN LFSR. Useful for frame alignment: sym\_pos==0 on a step boundary means the very next sample begins a fresh symbol._  |
|  int | [**wfm\_synth\_get\_wtype**](#function-wfm_synth_get_wtype) (const [**wfm\_synth\_state\_t**](structwfm__synth__state__t.md) \* state) <br>_Return the active waveform type discriminant. Maps to the WFM\_SYNTH\_\* enum: 0=tone, 1=noise, 2=pn, 3=bpsk, 4=qpsk. Use this to inspect which synthesis path is active at runtime._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) uint64\_t | [**wfm\_synth\_mls\_poly**](#function-wfm_synth_mls_poly) (uint32\_t n) <br>_Maximal-length-sequence (MLS) primitive polynomial for an LFSR of the given register length n, in pn\_core's right-shift Galois convention. Returns 0 for lengths outside 2..64 (caller errors). Generated from verified primitive polynomials (period 2^n-1); the n=2..16 values are unchanged._  |
|  void | [**wfm\_synth\_reset**](#function-wfm_synth_reset) ([**wfm\_synth\_state\_t**](structwfm__synth__state__t.md) \* state) <br>_Reset Synth to its post-create state. Resets the LO phase accumulator, AWGN internal state, and PN LFSR register to their initial values so the output sequence is perfectly reproducible from sample 0._  |
|  int | [**wfm\_synth\_set\_bits**](#function-wfm_synth_set_bits) ([**wfm\_synth\_state\_t**](structwfm__synth__state__t.md) \* state, const uint8\_t \* bits, size\_t n, int modulation) <br>_Attach a user bit pattern to a type=bits synth (no-op otherwise)._  |
|  void | [**wfm\_synth\_set\_chirp\_span**](#function-wfm_synth_set_chirp_span) ([**wfm\_synth\_state\_t**](structwfm__synth__state__t.md) \* state, size\_t span) <br>_Pin a chirp's sweep span to_ `span` _samples (no-op for non-chirp)._ |
|  void | [**wfm\_synth\_set\_cur\_im**](#function-wfm_synth_set_cur_im) ([**wfm\_synth\_state\_t**](structwfm__synth__state__t.md) \* state, float val) <br>_Override the held-symbol imaginary (Q) component in-place. Takes effect on the next_ [_**wfm\_synth\_step()**_](wfm__synth__core_8h.md#function-wfm_synth_step) _within the current symbol hold._ |
|  void | [**wfm\_synth\_set\_cur\_re**](#function-wfm_synth_set_cur_re) ([**wfm\_synth\_state\_t**](structwfm__synth__state__t.md) \* state, float val) <br>_Override the held-symbol real (I) component in-place. Takes effect on the next_ [_**wfm\_synth\_step()**_](wfm__synth__core_8h.md#function-wfm_synth_step) _within the current symbol hold._ |
|  void | [**wfm\_synth\_set\_nsps**](#function-wfm_synth_set_nsps) ([**wfm\_synth\_state\_t**](structwfm__synth__state__t.md) \* state, int val) <br>_Override the samples-per-symbol count in-place. Does not flush the symbol-position counter (sym\_pos); set sym\_pos=0 as well when changing sps mid-stream._  |
|  int | [**wfm\_synth\_set\_rrc**](#function-wfm_synth_set_rrc) ([**wfm\_synth\_state\_t**](structwfm__synth__state__t.md) \* state, const float \* taps, size\_t ntaps) <br>_Enable RRC pulse shaping on a modulated synth (pn/bpsk/qpsk)._  |
|  void | [**wfm\_synth\_set\_sym\_pos**](#function-wfm_synth_set_sym_pos) ([**wfm\_synth\_state\_t**](structwfm__synth__state__t.md) \* state, int val) <br>_Override the symbol-position counter in-place. Injecting 0 forces the next_ [_**wfm\_synth\_step()**_](wfm__synth__core_8h.md#function-wfm_synth_step) _to latch a new PN chip; any other value fast-forwards into the middle of the current symbol hold._ |
|  void | [**wfm\_synth\_set\_wtype**](#function-wfm_synth_set_wtype) ([**wfm\_synth\_state\_t**](structwfm__synth__state__t.md) \* state, int val) <br>_Override the waveform type discriminant in-place. Changing wtype does not reinitialise sub-objects; use with care._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) float complex | [**wfm\_synth\_step**](#function-wfm_synth_step) ([**wfm\_synth\_state\_t**](structwfm__synth__state__t.md) \* state) <br>_Generate one output sample from internal state. Advances the PN LFSR (modulated types only, on symbol boundaries), the LO phase accumulator, and the AWGN engine, then returns the mixed result:_ `sym * carrier + noise` _. Inlined and hot-path annotated so tight per-sample loops pay no call overhead._ |
|  void | [**wfm\_synth\_steps**](#function-wfm_synth_steps) ([**wfm\_synth\_state\_t**](structwfm__synth__state__t.md) \* state, float complex \* output, size\_t n) <br>_Generate a block of output samples. Calls_ [_**wfm\_synth\_step()**_](wfm__synth__core_8h.md#function-wfm_synth_step) _in a tight loop, writing each cf32 sample into_`output` _. The Python binding returns a freshly allocated NumPy complex64 array; ownership is transferred to the caller._ |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**WFM\_SYNTH\_SNR\_CLEAN**](wfm__synth__core_8h.md#define-wfm_synth_snr_clean)  `100.0`<br> |

## Detailed Description


Lifecycle: create -&gt; `[step / steps / reset]*` -&gt; destroy


Example: 
```C++
wfm_synth_state_t *obj = wfm_synth_create(0, 1000000.0, 0.0, 100.0, 0, 1, 8, 7, 0);
float complex y = wfm_synth_step(obj);
wfm_synth_destroy(obj);
```
 


    
## Public Types Documentation




### enum wfm\_\_synth\_\_core\_8h\_1abd477555e01841805289c5cf8e4e76fb 

```C++
enum wfm__synth__core_8h_1abd477555e01841805289c5cf8e4e76fb {
    WFM_SYNTH_TONE = 0,
    WFM_SYNTH_NOISE = 1,
    WFM_SYNTH_PN = 2,
    WFM_SYNTH_BPSK = 3,
    WFM_SYNTH_QPSK = 4,
    WFM_SYNTH_CHIRP = 5,
    WFM_SYNTH_BITS = 6
};
```



Waveform type discriminant (the `type` create argument / type choice). 


        

<hr>
## Public Functions Documentation




### function wfm\_synth\_create 

_Allocate and configure a waveform synthesiser. The synthesiser combines a local oscillator (LO), optional AWGN, and an optional PN LFSR into a single streaming source. One call to_ [_**wfm\_synth\_step()**_](wfm__synth__core_8h.md#function-wfm_synth_step) _or_[_**wfm\_synth\_steps()**_](wfm__synth__core_8h.md#function-wfm_synth_steps) _advances all sub-components in lock-step. SNR &gt;= WFM\_SYNTH\_SNR\_CLEAN (100 dB) skips AWGN entirely — clean waveforms pay no noise overhead. When_`snr_mode` _is "auto" the library picks the natural reference: Es/No for modulated types (BPSK, QPSK), fs-band SNR for tone/noise/PN._
```C++
wfm_synth_state_t * wfm_synth_create (
    int type,
    double fs,
    double freq,
    double snr,
    int snr_mode,
    uint32_t seed,
    int sps,
    int pn_length,
    uint64_t pn_poly,
    int lfsr,
    double f_end
) 
```





**Parameters:**


* `type` Waveform type: 0=tone, 1=noise, 2=pn, 3=bpsk, 4=qpsk, 5=chirp, 6=bits. The Python binding accepts strings "tone"\| "noise"\|"pn"\|"bpsk"\|"qpsk"\|"chirp"\|"bits". For "bits" attach the pattern with [**wfm\_synth\_set\_bits()**](wfm__synth__core_8h.md#function-wfm_synth_set_bits) after create(). 
* `fs` Sample rate in Hz. Sets the carrier frequency normalisation and the noise bandwidth. Default 1 000 000.0. 
* `freq` Carrier frequency offset in Hz (−fs/2 … fs/2). A complex LO is created only when freq != 0. For a chirp this is the start frequency f\_start (the instantaneous frequency at t=0). Default 0.0. 
* `snr` Target SNR in dB, interpreted per `snr_mode`. Values &gt;= WFM\_SYNTH\_SNR\_CLEAN (100) disable AWGN. Default 100.0. 
* `snr_mode` SNR reference: 0=auto, 1=fs (full-band), 2=ebno, 3=esno. The Python binding accepts strings "auto"\|"fs"\|"ebno"\|"esno". Default 0. 
* `seed` PRNG seed shared by AWGN and the PN LFSR. Default 1. 
* `sps` Samples per symbol for modulated types (BPSK, QPSK, PN). Ignored for tone/noise. Default 8. 
* `pn_length` LFSR register length (1..64); period = 2^pn\_length - 1. Default 7 (period 127). 
* `pn_poly` Galois tap polynomial for the LFSR. 0 means "look up
             the canonical MLS polynomial for pn\_length" from the wfm\_synth\_mls\_poly table. Default 0. 
* `lfsr` LFSR realization: PN\_GALOIS (0) or PN\_FIBONACCI (1). 
* `f_end` Chirp end frequency in Hz (type=chirp only; ignored otherwise). With `freq` as the start, the instantaneous frequency sweeps linearly from `freq` to `f_end` over the span (set by [**wfm\_synth\_set\_chirp\_span()**](wfm__synth__core_8h.md#function-wfm_synth_set_chirp_span) or the first [**wfm\_synth\_steps()**](wfm__synth__core_8h.md#function-wfm_synth_steps) call), then holds at `f_end`. `f_end < freq` is a down-chirp. Default 0.0. 



**Returns:**

Heap-allocated state, or NULL on allocation failure. 




**Note:**

Caller must call [**wfm\_synth\_destroy()**](wfm__synth__core_8h.md#function-wfm_synth_destroy) when done. 
```C++
>>> from doppler.wfm import _SynthEngine
>>> import numpy as np
>>> s = _SynthEngine(type="tone", fs=1.0, freq=0.0, snr=100.0)
>>> x = s.steps(4)
>>> x.dtype
dtype('complex64')
>>> x.tolist()
[(1+0j), (1+0j), (1+0j), (1+0j)]
```
 





        

<hr>



### function wfm\_synth\_destroy 

_Destroy a synth instance and release all memory. Recursively frees the LO, AWGN, and PN sub-objects, then the struct itself. Safe to call with NULL (no-op)._ 
```C++
void wfm_synth_destroy (
    wfm_synth_state_t * state
) 
```





**Parameters:**


* `state` Pointer to heap-allocated state; may be NULL. 
```C++
>>> from doppler.wfm import _SynthEngine
>>> s = _SynthEngine(type="tone", fs=1.0, freq=0.0, snr=100.0)
>>> s.destroy()   # explicit teardown; no exception
```
 




        

<hr>



### function wfm\_synth\_get\_cur\_im 

_Return the imaginary part of the current held symbol. For QPSK this is the Q component (±1/√2); for BPSK/PN it is always 0; for tone/noise it is 0._ 
```C++
float wfm_synth_get_cur_im (
    const wfm_synth_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Current symbol imaginary (Q) component. 





        

<hr>



### function wfm\_synth\_get\_cur\_re 

_Return the real part of the current held symbol. For modulated types this is the I component latched at the last symbol boundary (±1 for BPSK/PN, ±1/√2 for QPSK). For tone the synthesiser initialises cur\_re to 1.0 so that the held symbol is a clean unit-power carrier; for noise it is 0.0 (noise has no held symbol)._ 
```C++
float wfm_synth_get_cur_re (
    const wfm_synth_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Current symbol real (I) component. 





        

<hr>



### function wfm\_synth\_get\_nsps 

_Return the samples-per-symbol count. For modulated types (BPSK, QPSK, PN) each symbol is held for nsps consecutive output samples. For tone/noise this field is present but unused by the synthesis path._ 
```C++
int wfm_synth_get_nsps (
    const wfm_synth_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Samples per symbol (nsps &gt;= 1). 





        

<hr>



### function wfm\_synth\_get\_sym\_pos 

_Return the current position within the current symbol (0..nsps-1). Reaches nsps and wraps to 0 each time a new symbol is consumed from the PN LFSR. Useful for frame alignment: sym\_pos==0 on a step boundary means the very next sample begins a fresh symbol._ 
```C++
int wfm_synth_get_sym_pos (
    const wfm_synth_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Symbol position counter (0 &lt;= sym\_pos &lt; nsps). 





        

<hr>



### function wfm\_synth\_get\_wtype 

_Return the active waveform type discriminant. Maps to the WFM\_SYNTH\_\* enum: 0=tone, 1=noise, 2=pn, 3=bpsk, 4=qpsk. Use this to inspect which synthesis path is active at runtime._ 
```C++
int wfm_synth_get_wtype (
    const wfm_synth_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Integer waveform type index (WFM\_SYNTH\_TONE .. WFM\_SYNTH\_QPSK). 





        

<hr>



### function wfm\_synth\_mls\_poly 

_Maximal-length-sequence (MLS) primitive polynomial for an LFSR of the given register length n, in pn\_core's right-shift Galois convention. Returns 0 for lengths outside 2..64 (caller errors). Generated from verified primitive polynomials (period 2^n-1); the n=2..16 values are unchanged._ 
```C++
JM_FORCEINLINE uint64_t wfm_synth_mls_poly (
    uint32_t n
) 
```




<hr>



### function wfm\_synth\_reset 

_Reset Synth to its post-create state. Resets the LO phase accumulator, AWGN internal state, and PN LFSR register to their initial values so the output sequence is perfectly reproducible from sample 0._ 
```C++
void wfm_synth_reset (
    wfm_synth_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 
```C++
>>> from doppler.wfm import _SynthEngine
>>> import numpy as np
>>> s = _SynthEngine(type="qpsk", sps=4, seed=1, snr=100.0)
>>> a = s.steps(16).copy()
>>> s.reset()
>>> np.array_equal(a, s.steps(16))
True
```
 




        

<hr>



### function wfm\_synth\_set\_bits 

_Attach a user bit pattern to a type=bits synth (no-op otherwise)._ 
```C++
int wfm_synth_set_bits (
    wfm_synth_state_t * state,
    const uint8_t * bits,
    size_t n,
    int modulation
) 
```



Copies `n` bits (each 0/1) into the synth; `modulation` maps them to symbols (0=none → 0/1 amplitude, 1=bpsk → ±1, 2=qpsk → Gray-coded ±1/√2, two bits per symbol). The pattern is oversampled by the create-time `sps` and **cycled** to fill whatever length `wfm_synth_steps()` requests, so one pass is `n * sps` samples (`2*ceil... ` — `n/2 * sps` for qpsk). Replaces any previous pattern; resets the read position. Safe to call repeatedly.




**Parameters:**


* `state` Must be non-NULL. 
* `bits` Array of `n` bytes, each 0 or 1. 
* `n` Number of bits (&gt; 0). 
* `modulation` 0=none, 1=bpsk, 2=qpsk. 



**Returns:**

0 on success; -1 on bad args or allocation failure. 





        

<hr>



### function wfm\_synth\_set\_chirp\_span 

_Pin a chirp's sweep span to_ `span` _samples (no-op for non-chirp)._
```C++
void wfm_synth_set_chirp_span (
    wfm_synth_state_t * state,
    size_t span
) 
```



A linear chirp's slope is `(f_end − f_start) / span`, so the span — the number of samples the sweep occupies — must be known before generation. The composer/CLI call this with the segment length; a standalone synth that is never pinned locks its span to the first [**wfm\_synth\_steps()**](wfm__synth__core_8h.md#function-wfm_synth_steps) call instead. Only the first pin (while the span is still 0) takes effect, so it is safe to call unconditionally after [**wfm\_synth\_create()**](wfm__synth__core_8h.md#function-wfm_synth_create).




**Parameters:**


* `state` Must be non-NULL. 
* `span` Sweep length in samples (&gt; 0). 




        

<hr>



### function wfm\_synth\_set\_cur\_im 

_Override the held-symbol imaginary (Q) component in-place. Takes effect on the next_ [_**wfm\_synth\_step()**_](wfm__synth__core_8h.md#function-wfm_synth_step) _within the current symbol hold._
```C++
void wfm_synth_set_cur_im (
    wfm_synth_state_t * state,
    float val
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `val` New cur\_im value. 




        

<hr>



### function wfm\_synth\_set\_cur\_re 

_Override the held-symbol real (I) component in-place. Takes effect on the next_ [_**wfm\_synth\_step()**_](wfm__synth__core_8h.md#function-wfm_synth_step) _within the current symbol hold._
```C++
void wfm_synth_set_cur_re (
    wfm_synth_state_t * state,
    float val
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `val` New cur\_re value. 




        

<hr>



### function wfm\_synth\_set\_nsps 

_Override the samples-per-symbol count in-place. Does not flush the symbol-position counter (sym\_pos); set sym\_pos=0 as well when changing sps mid-stream._ 
```C++
void wfm_synth_set_nsps (
    wfm_synth_state_t * state,
    int val
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `val` New nsps value (&gt;= 1). 




        

<hr>



### function wfm\_synth\_set\_rrc 

_Enable RRC pulse shaping on a modulated synth (pn/bpsk/qpsk)._ 
```C++
int wfm_synth_set_rrc (
    wfm_synth_state_t * state,
    const float * taps,
    size_t ntaps
) 
```



Replaces the default rectangular sample-and-hold with a root-raised-cosine pulse: the symbol-rate impulse train is filtered by `taps` (a real FIR of `ntaps` coefficients, typically `wfm_rrc_taps(beta, sps, span)`). The taps are scaled by sqrt(sps) internally for unit transmit power, so every caller passes the raw taps and gets byte-identical shaping. No-op for non-modulated types. Replaces any existing shaper and clears its delay line.




**Parameters:**


* `state` Must be non-NULL. 
* `taps` Real FIR taps (copied). 
* `ntaps` Number of taps (&gt; 0). 



**Returns:**

0 on success; -1 on bad args / allocation failure. 





        

<hr>



### function wfm\_synth\_set\_sym\_pos 

_Override the symbol-position counter in-place. Injecting 0 forces the next_ [_**wfm\_synth\_step()**_](wfm__synth__core_8h.md#function-wfm_synth_step) _to latch a new PN chip; any other value fast-forwards into the middle of the current symbol hold._
```C++
void wfm_synth_set_sym_pos (
    wfm_synth_state_t * state,
    int val
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `val` New sym\_pos value (0 &lt;= val &lt; nsps). 




        

<hr>



### function wfm\_synth\_set\_wtype 

_Override the waveform type discriminant in-place. Changing wtype does not reinitialise sub-objects; use with care._ 
```C++
void wfm_synth_set_wtype (
    wfm_synth_state_t * state,
    int val
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `val` New wtype value (WFM\_SYNTH\_TONE .. WFM\_SYNTH\_QPSK). 




        

<hr>



### function wfm\_synth\_step 

_Generate one output sample from internal state. Advances the PN LFSR (modulated types only, on symbol boundaries), the LO phase accumulator, and the AWGN engine, then returns the mixed result:_ `sym * carrier + noise` _. Inlined and hot-path annotated so tight per-sample loops pay no call overhead._
```C++
JM_FORCEINLINE  JM_HOT float complex wfm_synth_step (
    wfm_synth_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Next output sample (float complex). 
```C++
>>> from doppler.wfm import _SynthEngine
>>> s = _SynthEngine(type="tone", fs=1.0, freq=0.0, snr=100.0)
>>> s.step()
(1+0j)
```
 





        

<hr>



### function wfm\_synth\_steps 

_Generate a block of output samples. Calls_ [_**wfm\_synth\_step()**_](wfm__synth__core_8h.md#function-wfm_synth_step) _in a tight loop, writing each cf32 sample into_`output` _. The Python binding returns a freshly allocated NumPy complex64 array; ownership is transferred to the caller._
```C++
void wfm_synth_steps (
    wfm_synth_state_t * state,
    float complex * output,
    size_t n
) 
```





**Parameters:**


* `state` Initialised Synth state returned by `wfm_synth_create`. 
* `output` Output buffer of at least `n` cf32 elements. 
* `n` Number of samples to generate. 
```C++
>>> from doppler.wfm import _SynthEngine
>>> import numpy as np
>>> s = _SynthEngine(type="tone", fs=1.0, freq=0.0, snr=100.0)
>>> x = s.steps(4)
>>> x.shape, x.dtype
((4,), dtype('complex64'))
>>> x.tolist()
[(1+0j), (1+0j), (1+0j), (1+0j)]
```
 




        

<hr>
## Macro Definition Documentation





### define WFM\_SYNTH\_SNR\_CLEAN 

```C++
#define WFM_SYNTH_SNR_CLEAN `100.0`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/wfm_synth/wfm_synth_core.h`

