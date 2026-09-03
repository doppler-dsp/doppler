

# File awgn\_core.h



[**FileList**](files.md) **>** [**awgn**](dir_b535f71dd6c18f769df9e4bf89a97331.md) **>** [**awgn\_core.h**](awgn__core_8h.md)

[Go to the source code of this file](awgn__core_8h_source.md)

_Additive White Gaussian Noise generator._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "dp_state.h"`
* `#include "jm_perf.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**awgn\_state\_t**](structawgn__state__t.md) <br> |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  int | [**awgn**](#function-awgn) (uint64\_t seed, float amplitude, size\_t n, float \_Complex \* out) <br>_One-shot AWGN generation — no persistent state required._  |
|  float | [**awgn\_amplitude\_for\_snr**](#function-awgn_amplitude_for_snr) (float snr\_db, float signal\_power) <br>_The_ `amplitude` _that puts a signal at a target SNR._ |
|  [**awgn\_state\_t**](structawgn__state__t.md) \* | [**awgn\_create**](#function-awgn_create) (uint64\_t seed, float amplitude) <br>_Create an AWGN generator. Allocates state, seeds the xoshiro256++ RNG via SplitMix64, and sets up both the scalar and the AVX2 parallel streams. The initial seed is stored so_ [_**awgn\_reset()**_](awgn__core_8h.md#function-awgn_reset) _can reproduce the exact same stream._ |
|  void | [**awgn\_destroy**](#function-awgn_destroy) ([**awgn\_state\_t**](structawgn__state__t.md) \* state) <br> |
|  size\_t | [**awgn\_generate**](#function-awgn_generate) ([**awgn\_state\_t**](structawgn__state__t.md) \* state, size\_t n, float \_Complex \* out, size\_t max\_out) <br>_Generate n complex CF32 AWGN samples. Uses Box-Muller with xoshiro256++ to fill_ `out` _with independent complex Gaussians: Re and Im each have zero mean and standard deviation_`amplitude` _. Total complex power = 2 × amplitude². The AVX2 path processes 8 samples in parallel when available._ |
|  size\_t | [**awgn\_generate\_max\_out**](#function-awgn_generate_max_out) ([**awgn\_state\_t**](structawgn__state__t.md) \* state) <br>_Conservative upper bound on generate() output size._  |
|  float | [**awgn\_get\_amplitude**](#function-awgn_get_amplitude) (const [**awgn\_state\_t**](structawgn__state__t.md) \* state) <br>_Return the current amplitude (per-component std dev)._  |
|  void | [**awgn\_get\_state**](#function-awgn_get_state) (const [**awgn\_state\_t**](structawgn__state__t.md) \* state, void \* blob) <br>_Serialize the RNG state (scalar + AVX2 streams) into_ `blob` _._ |
|  void | [**awgn\_reseed**](#function-awgn_reseed) ([**awgn\_state\_t**](structawgn__state__t.md) \* state, uint64\_t seed) <br>_Reseed the RNG and reset all xoshiro256++ state. Equivalent to calling_ [_**awgn\_destroy()**_](awgn__core_8h.md#function-awgn_destroy) _and awgn\_create(seed, amplitude) but reuses the existing allocation. amplitude is unchanged._ |
|  void | [**awgn\_reset**](#function-awgn_reset) ([**awgn\_state\_t**](structawgn__state__t.md) \* state) <br>_Reset RNG to the seed supplied at create time. Re-runs the SplitMix64 seeding procedure with the original seed so the next_ [_**awgn\_generate()**_](awgn__core_8h.md#function-awgn_generate) _call produces exactly the same samples as the first call after_[_**awgn\_create()**_](awgn__core_8h.md#function-awgn_create) _. amplitude is not changed._ |
|  void | [**awgn\_set\_amplitude**](#function-awgn_set_amplitude) ([**awgn\_state\_t**](structawgn__state__t.md) \* state, float val) <br> |
|  int | [**awgn\_set\_state**](#function-awgn_set_state) ([**awgn\_state\_t**](structawgn__state__t.md) \* state, const void \* blob) <br>_Restore RNG state; DP\_OK, or DP\_ERR\_INVALID if rejected._  |
|  size\_t | [**awgn\_state\_bytes**](#function-awgn_state_bytes) (const [**awgn\_state\_t**](structawgn__state__t.md) \* state) <br>_Serialized-state byte size._  |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**AWGN\_STATE\_MAGIC**](awgn__core_8h.md#define-awgn_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('A', 'W', 'G', 'N')`<br> |
| define  | [**AWGN\_STATE\_VERSION**](awgn__core_8h.md#define-awgn_state_version)  `1u`<br> |

## Detailed Description


Generates complex CF32 samples where real and imaginary parts are independent zero-mean Gaussians, each with standard deviation `amplitude`. Total complex power = 2 \* amplitude².


#### Algorithm



RNG: xoshiro256++ — four 64-bit state words, seeded via SplitMix64 from the user-supplied uint64 seed. Period 2^256 − 1.


Transform: Box-Muller. Each call to awgn\_generate () consumes two 64-bit RNG outputs per complex output sample:


u1 ∈ (0, 1] (top 24 bits of first 64-bit word, +1 offset, /2^24) u2 ∈ [0, 1) (top 24 bits of second 64-bit word, /2^24) r = amplitude \* sqrt(−2 · ln u1) θ = 2π · u2 out = r·cos θ + j·r·sin θ



#### Usage




```C++
awgn_state_t *g = awgn_create(42, 1.0f);
float _Complex out[1024];
awgn_generate(g, 1024, out, 1024);
awgn_destroy(g);
```
 



    
## Public Functions Documentation




### function awgn 

_One-shot AWGN generation — no persistent state required._ 
```C++
int awgn (
    uint64_t seed,
    float amplitude,
    size_t n,
    float _Complex * out
) 
```



Creates a temporary generator, fills `out`, then frees it. Equivalent to: 
```C++
awgn_state_t *g = awgn_create(seed, amplitude);
awgn_generate (g, n, out, n);
awgn_destroy(g);
```





**Parameters:**


* `seed` RNG seed. 
* `amplitude` Per-component (Re, Im) standard deviation. 
* `n` Number of samples to generate. 
* `out` Output buffer, capacity ≥ n. 



**Returns:**

DP\_OK on success, DP\_ERR\_MEMORY on allocation failure. 





        

<hr>



### function awgn\_amplitude\_for\_snr 

_The_ `amplitude` _that puts a signal at a target SNR._
```C++
float awgn_amplitude_for_snr (
    float snr_db,
    float signal_power
) 
```



The inverse of this generator's own convention, and the reason it lives here: [**awgn\_create**](awgn__core_8h.md#function-awgn_create) takes a PER-COMPONENT sigma, so the complex noise power it produces is `2 * amplitude^2`. For a signal of power `signal_power` at `snr_db` (referenced to the full sample rate),


`amplitude = sqrt(signal_power / (2 * 10^(snr_db/10)))`


Pass the result straight to [**awgn\_create()**](awgn__core_8h.md#function-awgn_create). "Is the amplitude per rail or
total power?" has two defensible answers and this function is the one place that answers it  a caller deriving its own sigma is one factor of two away from a 3 dB error that nothing will fail on.




**Parameters:**


* `snr_db` Target SNR in dB, over the full sample rate. 
* `signal_power` Signal power (1.0 for unit-power tones or unit-energy BPSK/QPSK symbols). 



**Returns:**

Per-component sigma for one I or Q rail. 





        

<hr>



### function awgn\_create 

_Create an AWGN generator. Allocates state, seeds the xoshiro256++ RNG via SplitMix64, and sets up both the scalar and the AVX2 parallel streams. The initial seed is stored so_ [_**awgn\_reset()**_](awgn__core_8h.md#function-awgn_reset) _can reproduce the exact same stream._
```C++
awgn_state_t * awgn_create (
    uint64_t seed,
    float amplitude
) 
```





**Parameters:**


* `seed` 64-bit RNG seed. Two generators with different seeds produce statistically independent noise streams. 
* `amplitude` Per-component (Re, Im) standard deviation. Must be ≥ 0; total complex power = 2 × amplitude². 



**Returns:**

Heap-allocated state, or NULL on allocation failure. 
```C++
>>> from doppler.source import AWGN
>>> gen = AWGN(seed=0, amplitude=1.0)
>>> gen.amplitude
1.0
```
 





        

<hr>



### function awgn\_destroy 

```C++
void awgn_destroy (
    awgn_state_t * state
) 
```



Free all resources. NULL is a no-op. 


        

<hr>



### function awgn\_generate 

_Generate n complex CF32 AWGN samples. Uses Box-Muller with xoshiro256++ to fill_ `out` _with independent complex Gaussians: Re and Im each have zero mean and standard deviation_`amplitude` _. Total complex power = 2 × amplitude². The AVX2 path processes 8 samples in parallel when available._
```C++
size_t awgn_generate (
    awgn_state_t * state,
    size_t n,
    float _Complex * out,
    size_t max_out
) 
```





**Parameters:**


* `state` Generator state returned by [**awgn\_create()**](awgn__core_8h.md#function-awgn_create). 
* `n` Number of samples to generate. 
* `out` Output buffer; must hold at least n float \_Complex values. 
* `max_out` Capacity of `out` in elements. Emission stops there, so the return value is the number actually written. 



**Returns:**

min(n, max\_out) samples. 
```C++
>>> import numpy as np
>>> from doppler.source import AWGN
>>> gen = AWGN(seed=0, amplitude=1.0)
>>> out = gen.generate(1024)
>>> out.dtype
dtype('complex64')
>>> out.shape
(1024,)
>>> round(float(np.var(out.real)), 1)
1.0
>>> round(float(np.var(out.imag)), 1)
1.0
```
 





        

<hr>



### function awgn\_generate\_max\_out 

_Conservative upper bound on generate() output size._ 
```C++
size_t awgn_generate_max_out (
    awgn_state_t * state
) 
```



Returns 65536. The Python extension uses this for the initial buffer allocation; the buffer grows on demand if n &gt; 65536. 


        

<hr>



### function awgn\_get\_amplitude 

_Return the current amplitude (per-component std dev)._ 
```C++
float awgn_get_amplitude (
    const awgn_state_t * state
) 
```




```C++
>>> from doppler.source import AWGN
>>> gen = AWGN(seed=0, amplitude=1.0)
>>> gen.amplitude
1.0
>>> gen.amplitude = 2.0
>>> gen.amplitude
2.0
```
 


        

<hr>



### function awgn\_get\_state 

_Serialize the RNG state (scalar + AVX2 streams) into_ `blob` _._
```C++
void awgn_get_state (
    const awgn_state_t * state,
    void * blob
) 
```




<hr>



### function awgn\_reseed 

_Reseed the RNG and reset all xoshiro256++ state. Equivalent to calling_ [_**awgn\_destroy()**_](awgn__core_8h.md#function-awgn_destroy) _and awgn\_create(seed, amplitude) but reuses the existing allocation. amplitude is unchanged._
```C++
void awgn_reseed (
    awgn_state_t * state,
    uint64_t seed
) 
```





**Parameters:**


* `state` Generator state returned by [**awgn\_create()**](awgn__core_8h.md#function-awgn_create). 
* `seed` New 64-bit RNG seed. 
```C++
>>> import numpy as np
>>> from doppler.source import AWGN
>>> gen = AWGN(seed=0, amplitude=1.0)
>>> gen.reseed(42)
>>> out1 = gen.generate(4)
>>> gen2 = AWGN(seed=42, amplitude=1.0)
>>> out2 = gen2.generate(4)
>>> bool(np.all(out1 == out2))
True
```
 




        

<hr>



### function awgn\_reset 

_Reset RNG to the seed supplied at create time. Re-runs the SplitMix64 seeding procedure with the original seed so the next_ [_**awgn\_generate()**_](awgn__core_8h.md#function-awgn_generate) _call produces exactly the same samples as the first call after_[_**awgn\_create()**_](awgn__core_8h.md#function-awgn_create) _. amplitude is not changed._
```C++
void awgn_reset (
    awgn_state_t * state
) 
```




```C++
>>> import numpy as np
>>> from doppler.source import AWGN
>>> gen = AWGN(seed=0, amplitude=1.0)
>>> first = gen.generate(4)
>>> gen.reset()
>>> second = gen.generate(4)
>>> bool(np.all(first == second))
True
```
 


        

<hr>



### function awgn\_set\_amplitude 

```C++
void awgn_set_amplitude (
    awgn_state_t * state,
    float val
) 
```



Set amplitude without disturbing RNG state. 


        

<hr>



### function awgn\_set\_state 

_Restore RNG state; DP\_OK, or DP\_ERR\_INVALID if rejected._ 
```C++
int awgn_set_state (
    awgn_state_t * state,
    const void * blob
) 
```




<hr>



### function awgn\_state\_bytes 

_Serialized-state byte size._ 
```C++
size_t awgn_state_bytes (
    const awgn_state_t * state
) 
```




<hr>
## Macro Definition Documentation





### define AWGN\_STATE\_MAGIC 

```C++
#define AWGN_STATE_MAGIC `DP_FOURCC ('A', 'W', 'G', 'N')`
```




<hr>



### define AWGN\_STATE\_VERSION 

```C++
#define AWGN_STATE_VERSION `1u`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/awgn/awgn_core.h`

