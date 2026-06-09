

# File agc\_core.h



[**FileList**](files.md) **>** [**agc**](dir_947ec4d62e9dda8dbffe026d57cfb18d.md) **>** [**agc\_core.h**](agc__core_8h.md)

[Go to the source code of this file](agc__core_8h_source.md)

_Log-domain automatic gain control (AGC)._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "jm_perf.h"`
* `#include "util/util_core.h"`
* `#include <math.h>`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**agc\_state\_t**](structagc__state__t.md) <br>_AGC state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**agc\_state\_t**](structagc__state__t.md) \* | [**agc\_create**](#function-agc_create) (double ref\_db, double loop\_bw, double alpha) <br>_Construct a log-domain feedback AGC and return its heap state. The loop integrator starts at 0 dB (unity gain) and the power detector_ `p_avg` _is pre-seeded to_`10^` _(ref\_db/10) linear, so the first block of on-target samples produces no transient. Three parameters tune the closed-loop behaviour:_`ref_db` _sets the target,_`loop_bw` _sets the convergence speed, and_`alpha` _sets the detector smoothing._ |
|  void | [**agc\_destroy**](#function-agc_destroy) ([**agc\_state\_t**](structagc__state__t.md) \* state) <br>_Destroy an AGC instance and release all memory. Frees the heap-allocated_ `agc_state_t` _. Safe to call with_`NULL` _. After this call the pointer is invalid; set it to_`NULL` _. The Python binding calls this automatically when the object is garbage- collected or when used as a context manager (_`with` _AGC() as agc:)._ |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) double | [**agc\_exp10\_**](#function-agc_exp10_) (double v) <br>_Fast 10^v approximation (~1e-3 relative)._  |
|  double | [**agc\_get\_applied\_gain\_db**](#function-agc_get_applied_gain_db) (const [**agc\_state\_t**](structagc__state__t.md) \* state) <br>_Return the gain (in dB) actually applied to the most recent sample. Computes_ `20*log10` _(g\_last), where_`g_last` _is the linear multiplier that was used on the most recently processed sample. This differs from_`gain_db` _(the loop integrator's current command) because the loop filter advances the command one step ahead after each sample: immediately after_[_**agc\_step()**_](agc__core_8h.md#function-agc_step) __`gain_db` _already reflects the updated command while_`applied_gain_db` _still reflects what the signal actually saw. At loop convergence the two values are numerically equal. At create/reset both are 0.0 dB (unity)._ |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) double | [**agc\_log10\_**](#function-agc_log10_) (double p) <br>_Fast log10(p) approximation for p &gt; 0 (~1e-3 absolute)._  |
|  void | [**agc\_reset**](#function-agc_reset) ([**agc\_state\_t**](structagc__state__t.md) \* state) <br>_Reset the AGC loop state to its post-create condition. Sets_ `gain_db` _back to 0 dB (unity), clears_`g_last` _, and re-seeds the power-detector EMA_`p_avg` _from the current_`ref_db` _so that the first post-reset block produces no transient. All configuration fields (_`ref_db` _,_`loop_bw` _,_`alpha` _,_`decim` _,_`clip_db` _) are left untouched. Use this to process a new, independent signal segment without re-allocating._ |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) float complex | [**agc\_step**](#function-agc_step) ([**agc\_state\_t**](structagc__state__t.md) \* state, float complex x) <br>_Process one complex sample through the exact per-sample AGC loop. Applies the current_ `gain_db` _, measures the output power via the EMA detector, advances the loop-filter integrator by one step, then square-clips the returned sample to_`clip_db` _. The clip is applied after the detector update, so clipping never disturbs convergence. This is the exact reference path;_[_**agc\_steps()**_](agc__core_8h.md#function-agc_steps) _is the faster block equivalent and is not bit-identical but converges to the same steady state._ |
|  void | [**agc\_steps**](#function-agc_steps) ([**agc\_state\_t**](structagc__state__t.md) \* state, const float complex \* input, float complex \* output, size\_t n) <br>_Process a block of complex samples through the decimated AGC loop. Splits the input into chunks of_ `decim` _samples. Within each chunk the gain is linearly interpolated from the previous chunk's end value to the new loop-filter output (a first-order hold) so there is no inter-chunk gain staircase. The detector and loop filter run once per chunk on the chunk's mean power — O(n/decim) control-loop work versus O(n) for_[_**agc\_step()**_](agc__core_8h.md#function-agc_step) _. The output array may alias the input (in-place)._ |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**AGC\_CLIP\_DB\_DEFAULT**](agc__core_8h.md#define-agc_clip_db_default)  `120.0`<br>_Default output clip level (_ [_**agc\_state\_t::clip\_db**_](structagc__state__t.md#variable-clip_db) _), in dB._ |
| define  | [**AGC\_DECIM\_DEFAULT**](agc__core_8h.md#define-agc_decim_default)  `8`<br>_Default envelope decimation factor (_ [_**agc\_state\_t::decim**_](structagc__state__t.md#variable-decim) _)._ |
| define  | [**AGC\_POWER\_FLOOR**](agc__core_8h.md#define-agc_power_floor)  `1e-30`<br>_Power floor for the detector, in linear units._  |

## Detailed Description


A feedback AGC that drives the average power of its output toward a fixed reference level. Three stages run per sample:



* Gain y = x \* 10^(gain\_db / 20)
* Detector p\_avg += alpha \* (\|y\|^2 - p\_avg)
* Loop filter gain\_db += (4\*loop\_bw) \* (ref\_db - 10\*log10(p\_avg))






**
**

The loop filter is a single integrator whose step size is `4*loop_bw`. `loop_bw` is the loop's normalised noise-equivalent bandwidth in cycles/sample: a 1st-order loop with integrator step `mu` has a noise bandwidth of `mu/4`, so the knob is expressed as a bandwidth rather than a bare loop gain. Smaller `loop_bw` is slower and smoother.




**
**

The control variable `gain_db` and the detector output are both in decibels, so the closed loop is a linear 1st-order recursion in the dB domain. Because output power (dB) equals input power (dB) plus `gain_db`, the loop reduces to



```C++
gain_db[n+1] = (1 - 4*loop_bw) * gain_db[n]
             + (4*loop_bw) * (ref_db - px_db[n])
```



which converges to `gain_db` = ref\_db - px\_db with a time constant of roughly `1/`(4\*loop\_bw) samples — independent of the absolute signal level. A 60 dB-loud signal and a 0 dB-quiet signal settle in the same number of samples; only a level-dependent loop would not.




**
**

`p_avg` is an exponential moving average (1-pole leaky integrator) of the instantaneous output power `|y|^2`. `alpha` in (0, 1] sets the detector bandwidth: small `alpha` smooths hard but reacts slowly.




**
**

Feedback — power is measured AFTER the gain. The gain applied to sample `n` is computed from samples up to `n-1`, so the per-sample loop is inherently sequential.




**
**

[**agc\_step()**](agc__core_8h.md#function-agc_step) advances the control loop every sample. [**agc\_steps()**](agc__core_8h.md#function-agc_steps) decimates it: the detector + loop filter run once per chunk of `decim` samples (default `AGC_DECIM_DEFAULT`; typically 8, 16 or 32). The gain the loop commands is linearly interpolated across the chunk — a first-order hold, so the applied gain has no inter-chunk staircase — while the gain-apply and the power sum vectorise. This is sound because the detector average already band-limits the envelope, but it makes [**agc\_steps()**](agc__core_8h.md#function-agc_steps) not bit-identical to a per-sample [**agc\_step()**](agc__core_8h.md#function-agc_step) loop, only equivalent at convergence. The per-block detector and loop coefficients are rescaled from `alpha` / `loop_bw` internally, so those keep their per-sample meaning; keep `loop_bw` well below `1/`(4\*decim) for loop stability.




**
**

Each output sample is square-clipped: the real and imaginary parts are independently limited to `+/-10^`(clip\_db/20) — a square region in the IQ plane, not a circular magnitude limit. The clip is the last operation applied to the output and does NOT feed the power detector: the loop always measures the true, unclipped power, so clipping never disturbs convergence. `clip_db` defaults to `AGC_CLIP_DB_DEFAULT`, which is high enough to be effectively off.


Lifecycle: `agc_create -> (step / steps / reset)* -> agc_destroy`



```C++
// Hold output power at 0 dB; slow loop, moderate detector smoothing.
agc_state_t *agc = agc_create(0.0, 0.0025, 0.05);
float complex y = agc_step(agc, 4.0f + 0.0f * I);  // loud input
// ... feed more samples; gain_db converges so 10*log10(|y|^2) -> 0 dB
agc_destroy(agc);
```
 


    
## Public Functions Documentation




### function agc\_create 

_Construct a log-domain feedback AGC and return its heap state. The loop integrator starts at 0 dB (unity gain) and the power detector_ `p_avg` _is pre-seeded to_`10^` _(ref\_db/10) linear, so the first block of on-target samples produces no transient. Three parameters tune the closed-loop behaviour:_`ref_db` _sets the target,_`loop_bw` _sets the convergence speed, and_`alpha` _sets the detector smoothing._
```C++
agc_state_t * agc_create (
    double ref_db,
    double loop_bw,
    double alpha
) 
```





**Parameters:**


* `ref_db` Target output power in dB (e.g. `0.0` for unity power). 
* `loop_bw` Loop noise bandwidth in cycles/sample; the loop settles in roughly `1/`(4\*loop\_bw) samples. Smaller values are slower and smoother; keep well below `1/`(4\*decim) when using [**agc\_steps()**](agc__core_8h.md#function-agc_steps). 
* `alpha` Power-detector EMA coefficient in (0, 1]; smaller values smooth harder but react slower to envelope changes. 



**Returns:**

Heap-allocated `agc_state_t`, or `NULL` on allocation failure. The caller must call [**agc\_destroy()**](agc__core_8h.md#function-agc_destroy) when done. 
```C++
>>> from doppler.agc import AGC
>>> agc = AGC(ref_db=0.0, loop_bw=0.0025, alpha=0.05)
>>> agc.ref_db, agc.loop_bw, agc.alpha
(0.0, 0.0025, 0.05)
>>> agc.gain_db, agc.applied_gain_db
(0.0, 0.0)
>>> agc.decim, agc.clip_db
(8, 120.0)
```
 





        

<hr>



### function agc\_destroy 

_Destroy an AGC instance and release all memory. Frees the heap-allocated_ `agc_state_t` _. Safe to call with_`NULL` _. After this call the pointer is invalid; set it to_`NULL` _. The Python binding calls this automatically when the object is garbage- collected or when used as a context manager (_`with` _AGC() as agc:)._
```C++
void agc_destroy (
    agc_state_t * state
) 
```





**Parameters:**


* `state` Pointer to the state to free; may be `NULL` (no-op). 
```C++
>>> from doppler.agc import AGC
>>> agc = AGC()
>>> agc.destroy()   # explicit release
>>> with AGC(ref_db=0.0, loop_bw=0.0025, alpha=0.05) as agc2:
...     y = agc2.step(1.0+0.0j)
...     y
(1+0j)
```
 




        

<hr>



### function agc\_exp10\_ 

_Fast 10^v approximation (~1e-3 relative)._ 
```C++
JM_FORCEINLINE double agc_exp10_ (
    double v
) 
```



Routes through 2^z = 2^floor(z) \* 2^frac with z = v\*log2(10): the integer part becomes a raw IEEE-754 exponent and the fractional part a 4th-order Taylor series. Far cheaper than libm pow(); the AGC loop tolerates orders of magnitude more error than this. 


        

<hr>



### function agc\_get\_applied\_gain\_db 

_Return the gain (in dB) actually applied to the most recent sample. Computes_ `20*log10` _(g\_last), where_`g_last` _is the linear multiplier that was used on the most recently processed sample. This differs from_`gain_db` _(the loop integrator's current command) because the loop filter advances the command one step ahead after each sample: immediately after_[_**agc\_step()**_](agc__core_8h.md#function-agc_step) __`gain_db` _already reflects the updated command while_`applied_gain_db` _still reflects what the signal actually saw. At loop convergence the two values are numerically equal. At create/reset both are 0.0 dB (unity)._
```C++
double agc_get_applied_gain_db (
    const agc_state_t * state
) 
```





**Returns:**

Applied gain in dB; 0.0 at create / reset. 
```C++
>>> from doppler.agc import AGC
>>> agc = AGC(ref_db=0.0, loop_bw=0.0025, alpha=0.05)
>>> agc.applied_gain_db   # unity before any sample
0.0
>>> _ = agc.step(4.0+0.0j)
>>> agc.applied_gain_db   # gain USED on that sample was still 0 dB
0.0
>>> round(agc.gain_db, 6)  # loop already advanced to new command
-0.024276
```
 





        

<hr>



### function agc\_log10\_ 

_Fast log10(p) approximation for p &gt; 0 (~1e-3 absolute)._ 
```C++
JM_FORCEINLINE double agc_log10_ (
    double p
) 
```



Splits p = m \* 2^e via the IEEE-754 fields, takes log2(m) from the atanh series with t = (m-1)/(m+1) in &#91;0, 1/3&#93; (two terms), and scales log2 by log10(2). Used only on the decimated control path, so even the divide is amortised across a decimation chunk. 


        

<hr>



### function agc\_reset 

_Reset the AGC loop state to its post-create condition. Sets_ `gain_db` _back to 0 dB (unity), clears_`g_last` _, and re-seeds the power-detector EMA_`p_avg` _from the current_`ref_db` _so that the first post-reset block produces no transient. All configuration fields (_`ref_db` _,_`loop_bw` _,_`alpha` _,_`decim` _,_`clip_db` _) are left untouched. Use this to process a new, independent signal segment without re-allocating._
```C++
void agc_reset (
    agc_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 
```C++
>>> from doppler.agc import AGC
>>> import numpy as np
>>> agc = AGC(ref_db=0.0, loop_bw=0.0025, alpha=0.05)
>>> _ = agc.steps(np.full(1000, 4.0+0.0j, dtype=np.complex64))
>>> round(agc.gain_db, 1)   # converged to -12 dB
-12.0
>>> agc.reset()
>>> agc.gain_db, agc.applied_gain_db
(0.0, 0.0)
```
 




        

<hr>



### function agc\_step 

_Process one complex sample through the exact per-sample AGC loop. Applies the current_ `gain_db` _, measures the output power via the EMA detector, advances the loop-filter integrator by one step, then square-clips the returned sample to_`clip_db` _. The clip is applied after the detector update, so clipping never disturbs convergence. This is the exact reference path;_[_**agc\_steps()**_](agc__core_8h.md#function-agc_steps) _is the faster block equivalent and is not bit-identical but converges to the same steady state._
```C++
JM_FORCEINLINE  JM_HOT float complex agc_step (
    agc_state_t * state,
    float complex x
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `x` Complex input sample. 



**Returns:**

Gained, clipped output sample `x` \* 10^(gain\_db/20) with each component independently clamped to `+/-10^`(clip\_db/20). 
```C++
>>> from doppler.agc import AGC
>>> agc = AGC(ref_db=0.0, loop_bw=0.0025, alpha=0.05)
>>> agc.step(1.0+0.0j)   # unity gain at start, 0 dB in = 0 dB out
(1+0j)
>>> agc.gain_db           # loop already advanced from 0 dB
0.0
>>> agc2 = AGC(ref_db=0.0, loop_bw=0.0025, alpha=0.05)
>>> agc2.step(4.0+0.0j)  # 12 dB loud; first sample passes at unity gain
(4+0j)
>>> round(agc2.gain_db, 6)  # loop starts driving gain negative
-0.024276
```
 





        

<hr>



### function agc\_steps 

_Process a block of complex samples through the decimated AGC loop. Splits the input into chunks of_ `decim` _samples. Within each chunk the gain is linearly interpolated from the previous chunk's end value to the new loop-filter output (a first-order hold) so there is no inter-chunk gain staircase. The detector and loop filter run once per chunk on the chunk's mean power — O(n/decim) control-loop work versus O(n) for_[_**agc\_step()**_](agc__core_8h.md#function-agc_step) _. The output array may alias the input (in-place)._
```C++
void agc_steps (
    agc_state_t * state,
    const float complex * input,
    float complex * output,
    size_t n
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `input` Input complex64 array of `n` samples. 
* `output` Output buffer; must hold at least `n` elements. May alias `input` for in-place operation. 
* `n` Number of samples to process. 
```C++
>>> from doppler.agc import AGC
>>> import numpy as np
>>> agc = AGC(ref_db=0.0, loop_bw=0.0025, alpha=0.05)
>>> _ = agc.steps(np.full(1000, 4.0+0.0j, dtype=np.complex64))
>>> round(agc.gain_db, 1)   # gain converged to -12 dB
-12.0
>>> x = np.full(8, 4.0+0.0j, dtype=np.complex64)
>>> y = agc.steps(x)
>>> y.shape, y.dtype
((8,), dtype('complex64'))
>>> [round(abs(v)**2, 2) for v in y.tolist()]  # output power ~1.0
[1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0]
```
 




        

<hr>
## Macro Definition Documentation





### define AGC\_CLIP\_DB\_DEFAULT 

_Default output clip level (_ [_**agc\_state\_t::clip\_db**_](structagc__state__t.md#variable-clip_db) _), in dB._
```C++
#define AGC_CLIP_DB_DEFAULT `120.0`
```



120 dB is a per-component amplitude limit of 10^6 — far above any normally scaled signal, so output clipping is effectively disabled until `clip_db` is lowered. See [**agc\_state\_t::clip\_db**](structagc__state__t.md#variable-clip_db). 


        

<hr>



### define AGC\_DECIM\_DEFAULT 

_Default envelope decimation factor (_ [_**agc\_state\_t::decim**_](structagc__state__t.md#variable-decim) _)._
```C++
#define AGC_DECIM_DEFAULT `8`
```



[**agc\_steps()**](agc__core_8h.md#function-agc_steps) runs the detector + loop filter once per chunk of `decim` samples. `decim` must stay small relative to the loop time constant ~1/(4\*loop\_bw); useful values are 8, 16 and 32. 8 keeps the gain trajectory well inside the default loop bandwidth and is one AVX-width vector for the in-chunk gain-apply. 


        

<hr>



### define AGC\_POWER\_FLOOR 

_Power floor for the detector, in linear units._ 
```C++
#define AGC_POWER_FLOOR `1e-30`
```



Substituted for `p_avg` inside `log10()` so that a long run of silence yields a large-but-finite measured level (about -300 dB) instead of `-INF` / `NaN`. Also keeps the log10() argument a normal (non-denormal) double. Never reached in normal operation — `p_avg` is seeded with the reference power at create/reset. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/agc/agc_core.h`

