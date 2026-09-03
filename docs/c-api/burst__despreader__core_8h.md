

# File burst\_despreader\_core.h



[**FileList**](files.md) **>** [**burst\_despreader**](dir_311cad0a77759dd1ff95e00f622e2f49.md) **>** [**burst\_despreader\_core.h**](burst__despreader__core_8h.md)

[Go to the source code of this file](burst__despreader__core_8h_source.md)

_BurstDespreader component API._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "dp_state.h"`
* `#include "jm_perf.h"`
* `#include "loop_filter/loop_filter_core.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**burst\_despreader\_state\_t**](structburst__despreader__state__t.md) <br>_BurstDespreader state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  size\_t | [**burst\_despreader\_bits**](#function-burst_despreader_bits) ([**burst\_despreader\_state\_t**](structburst__despreader__state__t.md) \* state, const float \_Complex \* x, size\_t x\_len, uint8\_t \* out, size\_t max\_out) <br>_Despread a CF32 block; emit one hard BPSK bit (0/1) per code period._  |
|  size\_t | [**burst\_despreader\_bits\_max\_out**](#function-burst_despreader_bits_max_out) ([**burst\_despreader\_state\_t**](structburst__despreader__state__t.md) \* state) <br>_Upper bound on bits_ `burst_despreader_bits` _can emit (0; see burst\_despreader\_steps\_max\_out)._ |
|  [**burst\_despreader\_state\_t**](structburst__despreader__state__t.md) \* | [**burst\_despreader\_create**](#function-burst_despreader_create) (const uint8\_t \* code, size\_t code\_len, size\_t sf, size\_t sps, double init\_norm\_freq, double init\_chip\_phase, double bn\_carrier, double bn\_code) <br>_Create a burst despreader instance._  |
|  void | [**burst\_despreader\_destroy**](#function-burst_despreader_destroy) ([**burst\_despreader\_state\_t**](structburst__despreader__state__t.md) \* state) <br>_Destroy a burst despreader instance and release all memory._  |
|  double | [**burst\_despreader\_get\_bn\_carrier**](#function-burst_despreader_get_bn_carrier) (const [**burst\_despreader\_state\_t**](structburst__despreader__state__t.md) \* state) <br>_Carrier (Costas) loop noise bandwidth, normalized to the symbol rate._  |
|  double | [**burst\_despreader\_get\_bn\_code**](#function-burst_despreader_get_bn_code) (const [**burst\_despreader\_state\_t**](structburst__despreader__state__t.md) \* state) <br>_Code (DLL) loop noise bandwidth, normalized to the symbol rate._  |
|  double | [**burst\_despreader\_get\_code\_phase**](#function-burst_despreader_get_code_phase) (const [**burst\_despreader\_state\_t**](structburst__despreader__state__t.md) \* state) <br>_Current tracked code phase within the symbol, chips._  |
|  double | [**burst\_despreader\_get\_lock\_metric**](#function-burst_despreader_get_lock_metric) (const [**burst\_despreader\_state\_t**](structburst__despreader__state__t.md) \* state) <br>_Lock indicator in_ `[0,1]` _: the mean of \|Re prompt\|/\|prompt\| over every prompt of the burst (cumulative, not EMA — a one-shot burst gives each prompt equal weight instead of spending the whole burst warming a smoother up). ~1 when phase-locked; ~2/pi (0.637) with no carrier (\|cos theta\|, uniform theta)._ |
|  double | [**burst\_despreader\_get\_lock\_stat**](#function-burst_despreader_get_lock_stat) (const [**burst\_despreader\_state\_t**](structburst__despreader__state__t.md) \* state) <br>_Calibrated whole-burst lock statistic (the one-shot analog of the tracking loops' verify-counted detectors)._  |
|  double | [**burst\_despreader\_get\_norm\_freq**](#function-burst_despreader_get_norm_freq) (const [**burst\_despreader\_state\_t**](structburst__despreader__state__t.md) \* state) <br>_Current carrier frequency estimate, cycles/sample._  |
|  double | [**burst\_despreader\_get\_snr\_est**](#function-burst_despreader_get_snr_est) (const [**burst\_despreader\_state\_t**](structburst__despreader__state__t.md) \* state) <br>_Post-despread SNR estimate over the burst, accumulate-then-ratio: (sum Re^2 - sum Im^2) / sum Im^2, clamped &gt;= 0. For BPSK the signal lives in Re and the noise splits evenly, so this estimates A^2/sigma^2 (per-component) directly — unlike a per-symbol Re^2/Im^2 ratio, whose heavy-tailed reciprocal chi-square makes the estimate biased high with enormous variance. This is the EFFECTIVE post-loop SNR: residual tracking-loop phase jitter rotates signal energy into Im, so the estimate sits below the AWGN-only value by the jitter term (converging as bn -&gt; 0) — the quantity that actually predicts demodulation performance._  |
|  size\_t | [**burst\_despreader\_get\_stat\_n**](#function-burst_despreader_get_stat_n) (const [**burst\_despreader\_state\_t**](structburst__despreader__state__t.md) \* state) <br>_Number of prompts folded into the burst statistics so far._  |
|  void | [**burst\_despreader\_get\_state**](#function-burst_despreader_get_state) (const [**burst\_despreader\_state\_t**](structburst__despreader__state__t.md) \* state, void \* blob) <br> |
|  void | [**burst\_despreader\_reset**](#function-burst_despreader_reset) ([**burst\_despreader\_state\_t**](structburst__despreader__state__t.md) \* state) <br>_Re-seed the loops to the create-time phase/frequency and re-arm the burst statistics; preserve config._  |
|  void | [**burst\_despreader\_set\_acq**](#function-burst_despreader_set_acq) ([**burst\_despreader\_state\_t**](structburst__despreader__state__t.md) \* state, const uint8\_t \* acq\_code, size\_t acq\_code\_len, size\_t acq\_reps) <br>_Enable preamble-aided pull-in with a distinct acquisition code._  |
|  void | [**burst\_despreader\_set\_bn\_carrier**](#function-burst_despreader_set_bn_carrier) ([**burst\_despreader\_state\_t**](structburst__despreader__state__t.md) \* state, double val) <br>_Set the carrier loop bandwidth (recomputes the loop gains)._  |
|  void | [**burst\_despreader\_set\_bn\_code**](#function-burst_despreader_set_bn_code) ([**burst\_despreader\_state\_t**](structburst__despreader__state__t.md) \* state, double val) <br>_Set the code loop bandwidth (recomputes the loop gains)._  |
|  void | [**burst\_despreader\_set\_norm\_freq**](#function-burst_despreader_set_norm_freq) ([**burst\_despreader\_state\_t**](structburst__despreader__state__t.md) \* state, double val) <br>_Override the carrier frequency estimate, cycles/sample (re-seed)._  |
|  int | [**burst\_despreader\_set\_state**](#function-burst_despreader_set_state) ([**burst\_despreader\_state\_t**](structburst__despreader__state__t.md) \* state, const void \* blob) <br> |
|  size\_t | [**burst\_despreader\_state\_bytes**](#function-burst_despreader_state_bytes) (const [**burst\_despreader\_state\_t**](structburst__despreader__state__t.md) \* state) <br> |
|  size\_t | [**burst\_despreader\_steps**](#function-burst_despreader_steps) ([**burst\_despreader\_state\_t**](structburst__despreader__state__t.md) \* state, const float \_Complex \* x, size\_t x\_len, float \_Complex \* out, size\_t max\_out) <br>_Despread a CF32 block; emit one complex prompt symbol per code period._  |
|  size\_t | [**burst\_despreader\_steps\_max\_out**](#function-burst_despreader_steps_max_out) ([**burst\_despreader\_state\_t**](structburst__despreader__state__t.md) \* state) <br>_Upper bound on symbols_ `burst_despreader_steps` _can emit (0; the caller sizes the output buffer to the input length, which always suffices)._ |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**BURST\_DESPREADER\_STATE\_MAGIC**](burst__despreader__core_8h.md#define-burst_despreader_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('B','D','S','P')`<br> |
| define  | [**BURST\_DESPREADER\_STATE\_VERSION**](burst__despreader__core_8h.md#define-burst_despreader_state_version)  `2u /\* v2: cumulative burst statistics \*/`<br> |

## Detailed Description


Lifecycle: `create -> (steps / bits / reset)* -> destroy`


This object despreads a BLOCK: one prompt sample per spread symbol, so there is no scalar `step`  a single input chip is not a symbol.


Example: 
```C++
static const uint8_t code[4] = { 1, 0, 1, 1 };
burst_despreader_state_t *obj
    = burst_despreader_create (code, 4, 4, 2, 0.0, 0.0, 0.05, 0.01);
float _Complex in[8]  = { 0 };
float _Complex out[8] = { 0 };
size_t n = burst_despreader_steps (obj, in, 8, out,
                                   burst_despreader_steps_max_out (obj));
burst_despreader_destroy (obj);
```
 


    
## Public Functions Documentation




### function burst\_despreader\_bits 

_Despread a CF32 block; emit one hard BPSK bit (0/1) per code period._ 
```C++
size_t burst_despreader_bits (
    burst_despreader_state_t * state,
    const float _Complex * x,
    size_t x_len,
    uint8_t * out,
    size_t max_out
) 
```



Same streaming kernel as [**burst\_despreader\_steps()**](burst__despreader__core_8h.md#function-burst_despreader_steps), but emits the hard decision `crealf(prompt) >= 0` instead of the complex symbol.




**Parameters:**


* `state` Must be non-NULL. 
* `x` Input CF32 samples, length `x_len`. 
* `x_len` Number of input samples. 
* `out` Output buffer for bits (&gt;= max\_out). 
* `max_out` Capacity of `out` in bits. 



**Returns:**

Number of hard bits written into `out`. 
```C++
>>> import numpy as np
>>> from doppler.dsss import BurstDespreader
>>> rng = np.random.default_rng(1)
>>> code = rng.integers(0, 2, 31).astype(np.uint8)
>>> bits = rng.integers(0, 2, 30).astype(np.uint8)
>>> chips = np.where(code & 1, -1.0, 1.0)
>>> syms = np.where(bits == 1, -1.0, 1.0)
>>> tx = np.concatenate(
...     [np.repeat(s * chips, 4) for s in syms]).astype(np.complex64)
>>> d = BurstDespreader(code, sf=31, sps=4)
>>> rec = d.bits(tx)                             # hard 0/1 per symbol
>>> rec.shape
(30,)
>>> e = np.mean(rec != bits)             # up to a BPSK sign flip
>>> round(float(min(e, 1.0 - e)), 4)
0.0
>>> round(d.lock_metric, 3)
1.0
```
 





        

<hr>



### function burst\_despreader\_bits\_max\_out 

_Upper bound on bits_ `burst_despreader_bits` _can emit (0; see burst\_despreader\_steps\_max\_out)._
```C++
size_t burst_despreader_bits_max_out (
    burst_despreader_state_t * state
) 
```




<hr>



### function burst\_despreader\_create 

_Create a burst despreader instance._ 
```C++
burst_despreader_state_t * burst_despreader_create (
    const uint8_t * code,
    size_t code_len,
    size_t sf,
    size_t sps,
    double init_norm_freq,
    double init_chip_phase,
    double bn_carrier,
    double bn_code
) 
```





**Parameters:**


* `code` Data spreading code (0/1 chips), length `code_len`; copied. 
* `code_len` Length of `code` in chips (&gt;= sf). 
* `sf` Spreading factor: chips integrated per prompt symbol (default: 1). 
* `sps` Samples per chip (default: 2). 
* `init_norm_freq` Seed carrier frequency, cycles/sample — the acquisition estimate (default: 0.0). 
* `init_chip_phase` Seed code phase, chips (default: 0.0). 
* `bn_carrier` Carrier (Costas) loop noise bandwidth, normalized to the symbol rate (default: 0.05). 
* `bn_code` Code (DLL) loop noise bandwidth, normalized to the symbol rate (default: 0.01). 



**Returns:**

Heap-allocated state, or NULL on allocation failure. 




**Note:**

Caller must call [**burst\_despreader\_destroy()**](burst__despreader__core_8h.md#function-burst_despreader_destroy) when done. 
```C++
>>> import numpy as np
>>> from doppler.dsss import BurstDespreader
>>> rng = np.random.default_rng(1)
>>> code = rng.integers(0, 2, 31).astype(np.uint8)  # length-31 code
>>> chips = np.where(code & 1, -1.0, 1.0)    # 0 -> +1, 1 -> -1
>>> bits = rng.integers(0, 2, 30).astype(np.uint8)    # payload bits
>>> syms = np.where(bits == 1, -1.0, 1.0)             # BPSK symbols
>>> tx = np.concatenate(
...     [np.repeat(s * chips, 4) for s in syms]).astype(np.complex64)
>>> b = BurstDespreader(code, sf=31, sps=4)           # 31 chips/symbol
>>> sym = b.steps(tx)                        # one prompt/symbol
>>> sym.shape
(30,)
>>> hard = (sym.real < 0).astype(np.uint8)            # BPSK decision
>>> float(np.mean(hard != bits))             # payload recovered
0.0
```
 





        

<hr>



### function burst\_despreader\_destroy 

_Destroy a burst despreader instance and release all memory._ 
```C++
void burst_despreader_destroy (
    burst_despreader_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function burst\_despreader\_get\_bn\_carrier 

_Carrier (Costas) loop noise bandwidth, normalized to the symbol rate._ 
```C++
double burst_despreader_get_bn_carrier (
    const burst_despreader_state_t * state
) 
```




<hr>



### function burst\_despreader\_get\_bn\_code 

_Code (DLL) loop noise bandwidth, normalized to the symbol rate._ 
```C++
double burst_despreader_get_bn_code (
    const burst_despreader_state_t * state
) 
```




<hr>



### function burst\_despreader\_get\_code\_phase 

_Current tracked code phase within the symbol, chips._ 
```C++
double burst_despreader_get_code_phase (
    const burst_despreader_state_t * state
) 
```




<hr>



### function burst\_despreader\_get\_lock\_metric 

_Lock indicator in_ `[0,1]` _: the mean of \|Re prompt\|/\|prompt\| over every prompt of the burst (cumulative, not EMA — a one-shot burst gives each prompt equal weight instead of spending the whole burst warming a smoother up). ~1 when phase-locked; ~2/pi (0.637) with no carrier (\|cos theta\|, uniform theta)._
```C++
double burst_despreader_get_lock_metric (
    const burst_despreader_state_t * state
) 
```




<hr>



### function burst\_despreader\_get\_lock\_stat 

_Calibrated whole-burst lock statistic (the one-shot analog of the tracking loops' verify-counted detectors)._ 
```C++
double burst_despreader_get_lock_stat (
    const burst_despreader_state_t * state
) 
```



R = sqrt(stat\_n \* sum Re^2 / sum Im^2): the burst's coherent (in-phase) energy normalised by the noise power estimated from the quadrature arm. Because the noise reference is estimated from the SAME number of samples as the signal sum, the exact H0 law is R^2 = stat\_n \* F(stat\_n, stat\_n) — NOT chi-square (a chi-square gate assumes a known noise power and realizes tens of times the priced pfa here). The closed-form gate is


locked\_burst = R &gt; sqrt(stat\_n \* det\_threshold\_f(pfa, stat\_n))


exact for every stat\_n (odd included). Only payload prompts fold into the statistics — preamble prompts (different code length, pull-in transients) are excluded so the H0 law and the SNR calibration hold.


RETURNS 0 IN TWO CASES, and a caller must separate them with stat\_n: before any payload prompt has been folded (stat\_n == 0), and when the quadrature sum is exactly zero (stat\_n &gt; 0), where the ratio is undefined and there is no statistic to report. The second only happens on a perfectly noiseless input — which never occurs on the air and occurs constantly in tests, so it is worth knowing that the WORST possible reading of this statistic is what a synthetic clean burst produces. The example below therefore carries a noise floor, which is also the only condition under which a lock statistic means anything.



```C++
>>> import numpy as np
>>> from doppler.dsss import BurstDespreader
>>> from doppler.detection import det_threshold_f
>>> code = (np.arange(31) % 2).astype(np.uint8)
>>> chips = 1.0 - 2.0 * (code % 2)
>>> clean = np.tile(np.repeat(chips, 2), 64)
>>> rng = np.random.default_rng(7)
>>> n = (rng.standard_normal(clean.size)
...      + 1j * rng.standard_normal(clean.size)) / np.sqrt(2)
>>> b = BurstDespreader(code=code, sf=31, sps=2)
>>> _ = b.steps((clean + 0.3 * n).astype(np.complex64))
>>> eta = np.sqrt(b.stat_n * det_threshold_f(1e-3, b.stat_n))
>>> bool(b.lock_stat > eta)   # this burst passes the pfa=1e-3 gate
True
```
 


        

<hr>



### function burst\_despreader\_get\_norm\_freq 

_Current carrier frequency estimate, cycles/sample._ 
```C++
double burst_despreader_get_norm_freq (
    const burst_despreader_state_t * state
) 
```




<hr>



### function burst\_despreader\_get\_snr\_est 

_Post-despread SNR estimate over the burst, accumulate-then-ratio: (sum Re^2 - sum Im^2) / sum Im^2, clamped &gt;= 0. For BPSK the signal lives in Re and the noise splits evenly, so this estimates A^2/sigma^2 (per-component) directly — unlike a per-symbol Re^2/Im^2 ratio, whose heavy-tailed reciprocal chi-square makes the estimate biased high with enormous variance. This is the EFFECTIVE post-loop SNR: residual tracking-loop phase jitter rotates signal energy into Im, so the estimate sits below the AWGN-only value by the jitter term (converging as bn -&gt; 0) — the quantity that actually predicts demodulation performance._ 
```C++
double burst_despreader_get_snr_est (
    const burst_despreader_state_t * state
) 
```




<hr>



### function burst\_despreader\_get\_stat\_n 

_Number of prompts folded into the burst statistics so far._ 
```C++
size_t burst_despreader_get_stat_n (
    const burst_despreader_state_t * state
) 
```




<hr>



### function burst\_despreader\_get\_state 

```C++
void burst_despreader_get_state (
    const burst_despreader_state_t * state,
    void * blob
) 
```




<hr>



### function burst\_despreader\_reset 

_Re-seed the loops to the create-time phase/frequency and re-arm the burst statistics; preserve config._ 
```C++
void burst_despreader_reset (
    burst_despreader_state_t * state
) 
```



Restores the carrier NCO to the seed frequency and the code phase to the seed chip, zeroes the loop accumulators, and clears the cumulative burst read-backs (lock\_metric / snr\_est / lock\_stat / stat\_n) — the spreading code and bandwidths are kept. Call it between bursts so each burst's statistics start clean; a prior [**burst\_despreader\_set\_acq()**](burst__despreader__core_8h.md#function-burst_despreader_set_acq) preamble is also re-armed.




**Parameters:**


* `state` Must be non-NULL. 
```C++
>>> import numpy as np
>>> from doppler.dsss import BurstDespreader
>>> rng = np.random.default_rng(1)
>>> code = rng.integers(0, 2, 31).astype(np.uint8)
>>> chips = np.where(code & 1, -1.0, 1.0)
>>> syms = np.where(rng.integers(0, 2, 30) == 1, -1.0, 1.0)
>>> tx = np.concatenate(
...     [np.repeat(s * chips, 4) for s in syms]).astype(np.complex64)
>>> d = BurstDespreader(code, sf=31, sps=4)
>>> first = d.bits(tx)
>>> d.reset()                          # re-arm for a new burst
>>> np.array_equal(first, d.bits(tx))  # same as a fresh object
True
```
 




        

<hr>



### function burst\_despreader\_set\_acq 

_Enable preamble-aided pull-in with a distinct acquisition code._ 
```C++
void burst_despreader_set_acq (
    burst_despreader_state_t * state,
    const uint8_t * acq_code,
    size_t acq_code_len,
    size_t acq_reps
) 
```



Track `acq_reps` periods of `acq_code` coherently (the unmodulated, repeated acquisition preamble — a full ±pi phase discriminator, so the loops pull in even a wide residual) before switching to the data code for the payload. Call before feeding the burst; the acq mode clears automatically once the preamble is consumed, and re-arms on [**burst\_despreader\_reset()**](burst__despreader__core_8h.md#function-burst_despreader_reset). NB: set\_acq re-arms the PREAMBLE only — the cumulative burst statistics (lock\_metric / snr\_est / lock\_stat / stat\_n) are re-armed by [**burst\_despreader\_reset()**](burst__despreader__core_8h.md#function-burst_despreader_reset); call it between bursts.




**Parameters:**


* `state` Must be non-NULL. 
* `acq_code` Acquisition code (0/1), length acq\_code\_len; copied. 
* `acq_code_len` Acquisition code length in chips. 
* `acq_reps` Number of acq-code periods in the preamble. 
```C++
>>> import numpy as np
>>> from doppler.dsss import BurstDespreader
>>> rng = np.random.default_rng(5)
>>> acq = rng.integers(0, 2, 128).astype(np.uint8)    # long acq code
>>> data_code = rng.integers(0, 2, 32).astype(np.uint8)
>>> pbits = rng.integers(0, 2, 40).astype(np.uint8)
>>> asig = np.where(acq & 1, -1.0, 1.0)
>>> dch = np.where(data_code & 1, -1.0, 1.0)
>>> psyms = np.where(pbits == 1, -1.0, 1.0)
>>> pre = np.concatenate([np.repeat(asig, 4) for _ in range(4)])
>>> pay = np.concatenate([np.repeat(s * dch, 4) for s in psyms])
>>> burst = np.concatenate([pre, pay]).astype(np.complex64)
>>> d = BurstDespreader(data_code, sf=32, sps=4)
>>> d.set_acq(acq, 4)            # 4 preamble reps, pulls loops in
>>> out = d.bits(burst)          # preamble emits nothing
>>> out.shape                    # only the payload symbols come out
(40,)
>>> e = np.mean(out != pbits)
>>> round(float(min(e, 1.0 - e)), 4)
0.0
```
 




        

<hr>



### function burst\_despreader\_set\_bn\_carrier 

_Set the carrier loop bandwidth (recomputes the loop gains)._ 
```C++
void burst_despreader_set_bn_carrier (
    burst_despreader_state_t * state,
    double val
) 
```




<hr>



### function burst\_despreader\_set\_bn\_code 

_Set the code loop bandwidth (recomputes the loop gains)._ 
```C++
void burst_despreader_set_bn_code (
    burst_despreader_state_t * state,
    double val
) 
```




<hr>



### function burst\_despreader\_set\_norm\_freq 

_Override the carrier frequency estimate, cycles/sample (re-seed)._ 
```C++
void burst_despreader_set_norm_freq (
    burst_despreader_state_t * state,
    double val
) 
```




<hr>



### function burst\_despreader\_set\_state 

```C++
int burst_despreader_set_state (
    burst_despreader_state_t * state,
    const void * blob
) 
```




<hr>



### function burst\_despreader\_state\_bytes 

```C++
size_t burst_despreader_state_bytes (
    const burst_despreader_state_t * state
) 
```




<hr>



### function burst\_despreader\_steps 

_Despread a CF32 block; emit one complex prompt symbol per code period._ 
```C++
size_t burst_despreader_steps (
    burst_despreader_state_t * state,
    const float _Complex * x,
    size_t x_len,
    float _Complex * out,
    size_t max_out
) 
```



Streams: a partial symbol is carried in state across calls. Each emitted symbol is the complex prompt integrate-and-dump (carrier-wiped, code-stripped) — its sign is the BPSK decision, its phase/magnitude the soft information. During a `burst_despreader_set_acq` preamble no symbols are emitted (the loops are pulling in); payload symbols follow.




**Parameters:**


* `state` Must be non-NULL. 
* `x` Input CF32 samples, length `x_len`. 
* `x_len` Number of input samples. 
* `out` Output buffer for prompt symbols (&gt;= max\_out). 
* `max_out` Capacity of `out` in symbols. 



**Returns:**

Number of prompt symbols written into `out`.



```C++
>>> import numpy as np
>>> from doppler.dsss import BurstDespreader
>>> rng = np.random.default_rng(1)
>>> code = rng.integers(0, 2, 31).astype(np.uint8)  # length-31 code
>>> bits = rng.integers(0, 2, 30).astype(np.uint8)   # payload bits
>>> chips = np.where(code & 1, -1.0, 1.0)    # 0 -> +1, 1 -> -1
>>> syms = np.where(bits == 1, -1.0, 1.0)             # BPSK symbols
>>> tx = np.concatenate(
...     [np.repeat(s * chips, 4) for s in syms]).astype(np.complex64)
>>> d = BurstDespreader(code, sf=31, sps=4)
>>> sym = d.steps(tx)                        # one prompt/symbol
>>> sym.shape
(30,)
>>> hard = (sym.real < 0).astype(np.uint8)            # BPSK decision
>>> float(np.mean(hard != bits))             # payload recovered
0.0
```
 


        

<hr>



### function burst\_despreader\_steps\_max\_out 

_Upper bound on symbols_ `burst_despreader_steps` _can emit (0; the caller sizes the output buffer to the input length, which always suffices)._
```C++
size_t burst_despreader_steps_max_out (
    burst_despreader_state_t * state
) 
```




<hr>
## Macro Definition Documentation





### define BURST\_DESPREADER\_STATE\_MAGIC 

```C++
#define BURST_DESPREADER_STATE_MAGIC `DP_FOURCC ('B','D','S','P')`
```




<hr>



### define BURST\_DESPREADER\_STATE\_VERSION 

```C++
#define BURST_DESPREADER_STATE_VERSION `2u /* v2: cumulative burst statistics */`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/burst_despreader/burst_despreader_core.h`

