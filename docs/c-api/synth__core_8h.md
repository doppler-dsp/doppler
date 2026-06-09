

# File synth\_core.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**synth**](dir_135e4b6b03fee6eda2308471f560474b.md) **>** [**synth\_core.h**](synth__core_8h.md)

[Go to the source code of this file](synth__core_8h_source.md)

_Synth component API._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "jm_perf.h"`
* `#include "lo/lo_core.h"`
* `#include "awgn/awgn_core.h"`
* `#include "pn/pn_core.h"`
* `#include <math.h>`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**synth\_state\_t**](structsynth__state__t.md) <br>_Synth state._  |


## Public Types

| Type | Name |
| ---: | :--- |
| enum  | [**@013030212227253331271157327264131156300177213217**](#enum-@013030212227253331271157327264131156300177213217)  <br> |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**synth\_state\_t**](structsynth__state__t.md) \* | [**synth\_create**](#function-synth_create) (int type, double fs, double freq, double snr, int snr\_mode, uint32\_t seed, int sps, int pn\_length, uint64\_t pn\_poly, int lfsr) <br>_Allocate and configure a waveform synthesiser. The synthesiser combines a local oscillator (LO), optional AWGN, and an optional PN LFSR into a single streaming source. One call to_ [_**synth\_step()**_](synth__core_8h.md#function-synth_step) _or_[_**synth\_steps()**_](synth__core_8h.md#function-synth_steps) _advances all sub-components in lock-step. SNR &gt;= SYNTH\_SNR\_CLEAN (100 dB) skips AWGN entirely — clean waveforms pay no noise overhead. When_`snr_mode` _is "auto" the library picks the natural reference: Es/No for modulated types (BPSK, QPSK), fs-band SNR for tone/noise/PN._ |
|  void | [**synth\_destroy**](#function-synth_destroy) ([**synth\_state\_t**](structsynth__state__t.md) \* state) <br>_Destroy a synth instance and release all memory. Recursively frees the LO, AWGN, and PN sub-objects, then the struct itself. Safe to call with NULL (no-op)._  |
|  float | [**synth\_get\_cur\_im**](#function-synth_get_cur_im) (const [**synth\_state\_t**](structsynth__state__t.md) \* state) <br>_Return the imaginary part of the current held symbol. For QPSK this is the Q component (±1/√2); for BPSK/PN it is always 0; for tone/noise it is 0._  |
|  float | [**synth\_get\_cur\_re**](#function-synth_get_cur_re) (const [**synth\_state\_t**](structsynth__state__t.md) \* state) <br>_Return the real part of the current held symbol. For modulated types this is the I component latched at the last symbol boundary (±1 for BPSK/PN, ±1/√2 for QPSK). For tone the synthesiser initialises cur\_re to 1.0 so that the held symbol is a clean unit-power carrier; for noise it is 0.0 (noise has no held symbol)._  |
|  int | [**synth\_get\_nsps**](#function-synth_get_nsps) (const [**synth\_state\_t**](structsynth__state__t.md) \* state) <br>_Return the samples-per-symbol count. For modulated types (BPSK, QPSK, PN) each symbol is held for nsps consecutive output samples. For tone/noise this field is present but unused by the synthesis path._  |
|  int | [**synth\_get\_sym\_pos**](#function-synth_get_sym_pos) (const [**synth\_state\_t**](structsynth__state__t.md) \* state) <br>_Return the current position within the current symbol (0..nsps-1). Reaches nsps and wraps to 0 each time a new symbol is consumed from the PN LFSR. Useful for frame alignment: sym\_pos==0 on a step boundary means the very next sample begins a fresh symbol._  |
|  int | [**synth\_get\_wtype**](#function-synth_get_wtype) (const [**synth\_state\_t**](structsynth__state__t.md) \* state) <br>_Return the active waveform type discriminant. Maps to the SYNTH\_\* enum: 0=tone, 1=noise, 2=pn, 3=bpsk, 4=qpsk. Use this to inspect which synthesis path is active at runtime._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) uint64\_t | [**synth\_mls\_poly**](#function-synth_mls_poly) (uint32\_t n) <br>_Maximal-length-sequence (MLS) primitive polynomial for an LFSR of the given register length n, in pn\_core's right-shift Galois convention. Returns 0 for lengths outside 2..64 (caller errors). Generated from verified primitive polynomials (period 2^n-1); the n=2..16 values are unchanged._  |
|  void | [**synth\_reset**](#function-synth_reset) ([**synth\_state\_t**](structsynth__state__t.md) \* state) <br>_Reset Synth to its post-create state. Resets the LO phase accumulator, AWGN internal state, and PN LFSR register to their initial values so the output sequence is perfectly reproducible from sample 0._  |
|  void | [**synth\_set\_cur\_im**](#function-synth_set_cur_im) ([**synth\_state\_t**](structsynth__state__t.md) \* state, float val) <br>_Override the held-symbol imaginary (Q) component in-place. Takes effect on the next_ [_**synth\_step()**_](synth__core_8h.md#function-synth_step) _within the current symbol hold._ |
|  void | [**synth\_set\_cur\_re**](#function-synth_set_cur_re) ([**synth\_state\_t**](structsynth__state__t.md) \* state, float val) <br>_Override the held-symbol real (I) component in-place. Takes effect on the next_ [_**synth\_step()**_](synth__core_8h.md#function-synth_step) _within the current symbol hold._ |
|  void | [**synth\_set\_nsps**](#function-synth_set_nsps) ([**synth\_state\_t**](structsynth__state__t.md) \* state, int val) <br>_Override the samples-per-symbol count in-place. Does not flush the symbol-position counter (sym\_pos); set sym\_pos=0 as well when changing sps mid-stream._  |
|  void | [**synth\_set\_sym\_pos**](#function-synth_set_sym_pos) ([**synth\_state\_t**](structsynth__state__t.md) \* state, int val) <br>_Override the symbol-position counter in-place. Injecting 0 forces the next_ [_**synth\_step()**_](synth__core_8h.md#function-synth_step) _to latch a new PN chip; any other value fast-forwards into the middle of the current symbol hold._ |
|  void | [**synth\_set\_wtype**](#function-synth_set_wtype) ([**synth\_state\_t**](structsynth__state__t.md) \* state, int val) <br>_Override the waveform type discriminant in-place. Changing wtype does not reinitialise sub-objects; use with care._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) float complex | [**synth\_step**](#function-synth_step) ([**synth\_state\_t**](structsynth__state__t.md) \* state) <br>_Generate one output sample from internal state. Advances the PN LFSR (modulated types only, on symbol boundaries), the LO phase accumulator, and the AWGN engine, then returns the mixed result:_ `sym * carrier + noise` _. Inlined and hot-path annotated so tight per-sample loops pay no call overhead._ |
|  void | [**synth\_steps**](#function-synth_steps) ([**synth\_state\_t**](structsynth__state__t.md) \* state, float complex \* output, size\_t n) <br>_Generate a block of output samples. Calls_ [_**synth\_step()**_](synth__core_8h.md#function-synth_step) _in a tight loop, writing each cf32 sample into_`output` _. The Python binding returns a freshly allocated NumPy complex64 array; ownership is transferred to the caller._ |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**SYNTH\_SNR\_CLEAN**](synth__core_8h.md#define-synth_snr_clean)  `100.0`<br> |

## Detailed Description


Lifecycle: create -&gt; [step / steps / reset]\* -&gt; destroy


Example: 
```C++
synth_state_t *obj = synth_create(0, 1000000.0, 0.0, 100.0, 0, 1, 8, 7, 0);
float complex y = synth_step(obj);
synth_destroy(obj);
```
 


    
## Public Types Documentation




### enum @013030212227253331271157327264131156300177213217 

```C++
enum @013030212227253331271157327264131156300177213217 {
    SYNTH_TONE = 0,
    SYNTH_NOISE = 1,
    SYNTH_PN = 2,
    SYNTH_BPSK = 3,
    SYNTH_QPSK = 4
};
```



Waveform type discriminant (the `type` create argument / type choice). 


        

<hr>
## Public Functions Documentation




### function synth\_create 

_Allocate and configure a waveform synthesiser. The synthesiser combines a local oscillator (LO), optional AWGN, and an optional PN LFSR into a single streaming source. One call to_ [_**synth\_step()**_](synth__core_8h.md#function-synth_step) _or_[_**synth\_steps()**_](synth__core_8h.md#function-synth_steps) _advances all sub-components in lock-step. SNR &gt;= SYNTH\_SNR\_CLEAN (100 dB) skips AWGN entirely — clean waveforms pay no noise overhead. When_`snr_mode` _is "auto" the library picks the natural reference: Es/No for modulated types (BPSK, QPSK), fs-band SNR for tone/noise/PN._
```C++
synth_state_t * synth_create (
    int type,
    double fs,
    double freq,
    double snr,
    int snr_mode,
    uint32_t seed,
    int sps,
    int pn_length,
    uint64_t pn_poly,
    int lfsr
) 
```





**Parameters:**


* `type` Waveform type: 0=tone, 1=noise, 2=pn, 3=bpsk, 4=qpsk. The Python binding accepts strings "tone"\|"noise"\|"pn"\| "bpsk"\|"qpsk". 
* `fs` Sample rate in Hz. Sets the carrier frequency normalisation and the noise bandwidth. Default 1 000 000.0. 
* `freq` Carrier frequency offset in Hz (−fs/2 … fs/2). A complex LO is created only when freq != 0. Default 0.0. 
* `snr` Target SNR in dB, interpreted per `snr_mode`. Values &gt;= SYNTH\_SNR\_CLEAN (100) disable AWGN. Default 100.0. 
* `snr_mode` SNR reference: 0=auto, 1=fs (full-band), 2=ebno, 3=esno. The Python binding accepts strings "auto"\|"fs"\|"ebno"\|"esno". Default 0. 
* `seed` PRNG seed shared by AWGN and the PN LFSR. Default 1. 
* `sps` Samples per symbol for modulated types (BPSK, QPSK, PN). Ignored for tone/noise. Default 8. 
* `pn_length` LFSR register length (1..64); period = 2^pn\_length - 1. Default 7 (period 127). 
* `pn_poly` Galois tap polynomial for the LFSR. 0 means "look up
             the canonical MLS polynomial for pn\_length" from the synth\_mls\_poly table. Default 0. 
* `lfsr` LFSR realization: PN\_GALOIS (0) or PN\_FIBONACCI (1). 



**Returns:**

Heap-allocated state, or NULL on allocation failure. 




**Note:**

Caller must call [**synth\_destroy()**](synth__core_8h.md#function-synth_destroy) when done. 
```C++
>>> from doppler.wfmgen import Synth
>>> import numpy as np
>>> s = Synth(type="tone", fs=1.0, freq=0.0, snr=100.0)
>>> x = s.steps(4)
>>> x.dtype
dtype('complex64')
>>> x.tolist()
[(1+0j), (1+0j), (1+0j), (1+0j)]
```
 





        

<hr>



### function synth\_destroy 

_Destroy a synth instance and release all memory. Recursively frees the LO, AWGN, and PN sub-objects, then the struct itself. Safe to call with NULL (no-op)._ 
```C++
void synth_destroy (
    synth_state_t * state
) 
```





**Parameters:**


* `state` Pointer to heap-allocated state; may be NULL. 
```C++
>>> from doppler.wfmgen import Synth
>>> s = Synth(type="tone", fs=1.0, freq=0.0, snr=100.0)
>>> s.destroy()   # explicit teardown; no exception
```
 




        

<hr>



### function synth\_get\_cur\_im 

_Return the imaginary part of the current held symbol. For QPSK this is the Q component (±1/√2); for BPSK/PN it is always 0; for tone/noise it is 0._ 
```C++
float synth_get_cur_im (
    const synth_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Current symbol imaginary (Q) component. 
```C++
>>> from doppler.wfmgen import Synth
>>> s = Synth(type="tone", fs=1.0, freq=0.0, snr=100.0)
>>> s.get_cur_im()
0.0
```
 





        

<hr>



### function synth\_get\_cur\_re 

_Return the real part of the current held symbol. For modulated types this is the I component latched at the last symbol boundary (±1 for BPSK/PN, ±1/√2 for QPSK). For tone the synthesiser initialises cur\_re to 1.0 so that the held symbol is a clean unit-power carrier; for noise it is 0.0 (noise has no held symbol)._ 
```C++
float synth_get_cur_re (
    const synth_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Current symbol real (I) component. 
```C++
>>> from doppler.wfmgen import Synth
>>> s = Synth(type="tone", fs=1.0, freq=0.0, snr=100.0)
>>> s.get_cur_re()  # tone initialises to 1.0
1.0
```
 





        

<hr>



### function synth\_get\_nsps 

_Return the samples-per-symbol count. For modulated types (BPSK, QPSK, PN) each symbol is held for nsps consecutive output samples. For tone/noise this field is present but unused by the synthesis path._ 
```C++
int synth_get_nsps (
    const synth_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Samples per symbol (nsps &gt;= 1). 
```C++
>>> from doppler.wfmgen import Synth
>>> s = Synth(type="bpsk", sps=4, fs=1.0, snr=100.0)
>>> s.get_nsps()
4
```
 





        

<hr>



### function synth\_get\_sym\_pos 

_Return the current position within the current symbol (0..nsps-1). Reaches nsps and wraps to 0 each time a new symbol is consumed from the PN LFSR. Useful for frame alignment: sym\_pos==0 on a step boundary means the very next sample begins a fresh symbol._ 
```C++
int synth_get_sym_pos (
    const synth_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Symbol position counter (0 &lt;= sym\_pos &lt; nsps). 
```C++
>>> from doppler.wfmgen import Synth
>>> s = Synth(type="bpsk", sps=4, fs=1.0, snr=100.0)
>>> s.get_sym_pos()
0
```
 





        

<hr>



### function synth\_get\_wtype 

_Return the active waveform type discriminant. Maps to the SYNTH\_\* enum: 0=tone, 1=noise, 2=pn, 3=bpsk, 4=qpsk. Use this to inspect which synthesis path is active at runtime._ 
```C++
int synth_get_wtype (
    const synth_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Integer waveform type index (SYNTH\_TONE .. SYNTH\_QPSK). 
```C++
>>> from doppler.wfmgen import Synth
>>> s = Synth(type="tone", fs=1.0, freq=0.0, snr=100.0)
>>> s.get_wtype()
0
```
 





        

<hr>



### function synth\_mls\_poly 

_Maximal-length-sequence (MLS) primitive polynomial for an LFSR of the given register length n, in pn\_core's right-shift Galois convention. Returns 0 for lengths outside 2..64 (caller errors). Generated from verified primitive polynomials (period 2^n-1); the n=2..16 values are unchanged._ 
```C++
JM_FORCEINLINE uint64_t synth_mls_poly (
    uint32_t n
) 
```




<hr>



### function synth\_reset 

_Reset Synth to its post-create state. Resets the LO phase accumulator, AWGN internal state, and PN LFSR register to their initial values so the output sequence is perfectly reproducible from sample 0._ 
```C++
void synth_reset (
    synth_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 
```C++
>>> from doppler.wfmgen import Synth
>>> import numpy as np
>>> s = Synth(type="qpsk", sps=4, seed=1, snr=100.0)
>>> a = s.steps(16).copy()
>>> s.reset()
>>> np.array_equal(a, s.steps(16))
True
```
 




        

<hr>



### function synth\_set\_cur\_im 

_Override the held-symbol imaginary (Q) component in-place. Takes effect on the next_ [_**synth\_step()**_](synth__core_8h.md#function-synth_step) _within the current symbol hold._
```C++
void synth_set_cur_im (
    synth_state_t * state,
    float val
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `val` New cur\_im value. 
```C++
>>> from doppler.wfmgen import Synth
>>> s = Synth(type="tone", fs=1.0, freq=0.0, snr=100.0)
>>> s.set_cur_im(0.5)
>>> s.get_cur_im()
0.5
```
 




        

<hr>



### function synth\_set\_cur\_re 

_Override the held-symbol real (I) component in-place. Takes effect on the next_ [_**synth\_step()**_](synth__core_8h.md#function-synth_step) _within the current symbol hold._
```C++
void synth_set_cur_re (
    synth_state_t * state,
    float val
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `val` New cur\_re value. 
```C++
>>> from doppler.wfmgen import Synth
>>> s = Synth(type="tone", fs=1.0, freq=0.0, snr=100.0)
>>> s.set_cur_re(1.0)
>>> s.get_cur_re()
1.0
```
 




        

<hr>



### function synth\_set\_nsps 

_Override the samples-per-symbol count in-place. Does not flush the symbol-position counter (sym\_pos); set sym\_pos=0 as well when changing sps mid-stream._ 
```C++
void synth_set_nsps (
    synth_state_t * state,
    int val
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `val` New nsps value (&gt;= 1). 
```C++
>>> from doppler.wfmgen import Synth
>>> s = Synth(type="bpsk", sps=4, fs=1.0, snr=100.0)
>>> s.set_nsps(8)
>>> s.get_nsps()
8
```
 




        

<hr>



### function synth\_set\_sym\_pos 

_Override the symbol-position counter in-place. Injecting 0 forces the next_ [_**synth\_step()**_](synth__core_8h.md#function-synth_step) _to latch a new PN chip; any other value fast-forwards into the middle of the current symbol hold._
```C++
void synth_set_sym_pos (
    synth_state_t * state,
    int val
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `val` New sym\_pos value (0 &lt;= val &lt; nsps). 
```C++
>>> from doppler.wfmgen import Synth
>>> s = Synth(type="bpsk", sps=4, fs=1.0, snr=100.0)
>>> s.set_sym_pos(0)
>>> s.get_sym_pos()
0
```
 




        

<hr>



### function synth\_set\_wtype 

_Override the waveform type discriminant in-place. Changing wtype does not reinitialise sub-objects; use with care._ 
```C++
void synth_set_wtype (
    synth_state_t * state,
    int val
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `val` New wtype value (SYNTH\_TONE .. SYNTH\_QPSK). 
```C++
>>> from doppler.wfmgen import Synth
>>> s = Synth(type="tone", fs=1.0, freq=0.0, snr=100.0)
>>> s.set_wtype(1)
>>> s.get_wtype()
1
```
 




        

<hr>



### function synth\_step 

_Generate one output sample from internal state. Advances the PN LFSR (modulated types only, on symbol boundaries), the LO phase accumulator, and the AWGN engine, then returns the mixed result:_ `sym * carrier + noise` _. Inlined and hot-path annotated so tight per-sample loops pay no call overhead._
```C++
JM_FORCEINLINE  JM_HOT float complex synth_step (
    synth_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Next output sample (float complex). 
```C++
>>> from doppler.wfmgen import Synth
>>> s = Synth(type="tone", fs=1.0, freq=0.0, snr=100.0)
>>> s.step()
(1+0j)
```
 





        

<hr>



### function synth\_steps 

_Generate a block of output samples. Calls_ [_**synth\_step()**_](synth__core_8h.md#function-synth_step) _in a tight loop, writing each cf32 sample into_`output` _. The Python binding returns a freshly allocated NumPy complex64 array; ownership is transferred to the caller._
```C++
void synth_steps (
    synth_state_t * state,
    float complex * output,
    size_t n
) 
```





**Parameters:**


* `state` Initialised Synth state returned by `synth_create`. 
* `output` Output buffer of at least `n` cf32 elements. 
* `n` Number of samples to generate. 
```C++
>>> from doppler.wfmgen import Synth
>>> import numpy as np
>>> s = Synth(type="tone", fs=1.0, freq=0.0, snr=100.0)
>>> x = s.steps(4)
>>> x.shape, x.dtype
((4,), dtype('complex64'))
>>> x.tolist()
[(1+0j), (1+0j), (1+0j), (1+0j)]
```
 




        

<hr>
## Macro Definition Documentation





### define SYNTH\_SNR\_CLEAN 

```C++
#define SYNTH_SNR_CLEAN `100.0`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/synth/synth_core.h`

