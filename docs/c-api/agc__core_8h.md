

# File agc\_core.h



[**FileList**](files.md) **>** [**agc**](dir_947ec4d62e9dda8dbffe026d57cfb18d.md) **>** [**agc\_core.h**](agc__core_8h.md)

[Go to the source code of this file](agc__core_8h_source.md)

_Log-domain automatic gain control (AGC)._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "jm_perf.h"`
* `#include "dp_state.h"`
* `#include "dp_tlm/dp_tlm_core.h"`
* `#include "util/util_core.h"`
* `#include <math.h>`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**agc\_state\_t**](structagc__state__t.md) <br>_AGC state._  |
| struct | [**agc\_tlm\_t**](structagc__tlm__t.md) <br>_Telemetry attachment: a borrowed context + this object's probe ids. NULL ctx (the default) means detached — every probe site is then a single predicted-not-taken branch. Zeroed in state blobs and preserved across set\_state (DP\_DEFINE\_POD\_STATE\_TLM); telemetry is observation, not DSP state that migrates._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**agc\_state\_t**](structagc__state__t.md) \* | [**agc\_create**](#function-agc_create) (double ref\_db, double loop\_bw, double alpha) <br>_Construct a log-domain feedback AGC and return its heap state. The loop integrator starts at 0 dB (unity gain) and the power detector_ `p_avg` _is pre-seeded to_`10^` _(ref\_db/10) linear, so the first block of on-target samples produces no transient. Three parameters tune the closed-loop behaviour:_`ref_db` _sets the target,_`loop_bw` _sets the convergence speed, and_`alpha` _sets the detector smoothing._ |
|  void | [**agc\_destroy**](#function-agc_destroy) ([**agc\_state\_t**](structagc__state__t.md) \* state) <br>_Destroy an AGC instance and release all memory. Frees the heap-allocated_ `agc_state_t` _. Safe to call with_`NULL` _. After this call the pointer is invalid; set it to_`NULL` _. The Python binding calls this automatically when the object is garbage- collected or when used as a context manager (_`with` _AGC() as agc:)._ |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) double | [**agc\_exp10\_**](#function-agc_exp10_) (double v) <br>_Fast 10^v approximation (~1e-3 relative)._  |
|  double | [**agc\_get\_applied\_gain\_db**](#function-agc_get_applied_gain_db) (const [**agc\_state\_t**](structagc__state__t.md) \* state) <br>_Return the gain (in dB) actually applied to the most recent sample. Computes_ `20*log10` _(g\_last), where_`g_last` _is the linear multiplier that was used on the most recently processed sample. This differs from_`gain_db` _(the loop integrator's current command) because the loop filter advances the command one step ahead after each sample: immediately after_[_**agc\_step()**_](agc__core_8h.md#function-agc_step) __`gain_db` _already reflects the updated command while_`applied_gain_db` _still reflects what the signal actually saw. At loop convergence the two values are numerically equal. At create/reset both are 0.0 dB (unity)._ |
|  void | [**agc\_get\_state**](#function-agc_get_state) (const [**agc\_state\_t**](structagc__state__t.md) \* state, void \* blob) <br> |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) double | [**agc\_log10\_**](#function-agc_log10_) (double p) <br>_Fast log10(p) approximation for p &gt; 0 (~1e-3 absolute)._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) double | [**agc\_power\_**](#function-agc_power_) (float complex y) <br>_Power \|y\|^2 in the detector's working precision (double)._  |
|  void | [**agc\_reset**](#function-agc_reset) ([**agc\_state\_t**](structagc__state__t.md) \* state) <br>_Reset the AGC loop state to its post-create condition. Sets_ `gain_db` _back to 0 dB (unity), clears_`g_last` _, and re-seeds the power-detector EMA_`p_avg` _from the current_`ref_db` _so that the first post-reset block produces no transient. All configuration fields (_`ref_db` _,_`loop_bw` _,_`alpha` _,_`decim` _,_`clip_db` _) are left untouched. Use this to process a new, independent signal segment without re-allocating._ |
|  int | [**agc\_set\_state**](#function-agc_set_state) ([**agc\_state\_t**](structagc__state__t.md) \* state, const void \* blob) <br> |
|  int | [**agc\_set\_telemetry**](#function-agc_set_telemetry) ([**agc\_state\_t**](structagc__state__t.md) \* state, [**dp\_tlm\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_t) \* tlm, const char \* prefix, uint32\_t decim) <br>_Attach (or detach) a telemetry context and register the AGC's probes on it. Registers two probes, both recorded once per gain-update event and further thinned by decim:_  |
|  size\_t | [**agc\_settling\_samples**](#function-agc_settling_samples) (double loop\_bw, double alpha, double gain\_err\_db, double tol\_db) <br>_How many samples this loop needs to settle — the design query._  |
|  size\_t | [**agc\_state\_bytes**](#function-agc_state_bytes) (const [**agc\_state\_t**](structagc__state__t.md) \* state) <br> |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) float complex | [**agc\_step**](#function-agc_step) ([**agc\_state\_t**](structagc__state__t.md) \* state, float complex x) <br>_Process one complex sample through the per-sample AGC loop. Applies the current gain, measures the output power via the EMA detector, advances the loop-filter integrator, then square-clips the returned sample to_ `clip_db` _. The clip is applied after the detector update, so clipping never disturbs convergence. With the default_`gain_update_period` _== 1 this is the exact per-sample reference path; with_`gain_update_period` _P &gt; 1 the detector and gain-apply still run every sample but the loop-filter command (and the exp10/log10 it needs) refreshes once per P samples — a zero-order hold on the gain that amortises the transcendentals on a sample-rate hot loop, the streaming analogue of_[_**agc\_steps()**_](agc__core_8h.md#function-agc_steps) _' decimation._[_**agc\_steps()**_](agc__core_8h.md#function-agc_steps) _is the faster block equivalent; neither is bit-identical to the P == 1 loop once decimated, but both converge to the same steady state._ |
|  void | [**agc\_steps**](#function-agc_steps) ([**agc\_state\_t**](structagc__state__t.md) \* state, const float complex \* input, float complex \* output, size\_t n) <br>_Process a block of complex samples through the decimated AGC loop. Splits the input into chunks of_ `decim` _samples. Within each chunk the gain is linearly interpolated from the previous chunk's end value to the new loop-filter output (a first-order hold) so there is no inter-chunk gain staircase. The detector and loop filter run once per chunk on the chunk's mean power — O(n/decim) control-loop work versus O(n) for_[_**agc\_step()**_](agc__core_8h.md#function-agc_step) _. The output array may alias the input (in-place)._ |
|  size\_t | [**settling\_samples**](#function-settling_samples) (double loop\_bw, double alpha, double gain\_err\_db, double tol\_db) <br> |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**AGC\_CLIP\_DB\_DEFAULT**](agc__core_8h.md#define-agc_clip_db_default)  `120.0`<br>_Default output clip level (_ [_**agc\_state\_t::clip\_db**_](structagc__state__t.md#variable-clip_db) _), in dB._ |
| define  | [**AGC\_DECIM\_DEFAULT**](agc__core_8h.md#define-agc_decim_default)  `8`<br>_Default envelope decimation factor (_ [_**agc\_state\_t::decim**_](structagc__state__t.md#variable-decim) _)._ |
| define  | [**AGC\_POWER\_CEIL**](agc__core_8h.md#define-agc_power_ceil)  `2.3158417847463238e77`<br>_Power ceiling for the detector, in linear units._  |
| define  | [**AGC\_POWER\_FLOOR**](agc__core_8h.md#define-agc_power_floor)  `1e-30`<br>_Power floor for the detector, in linear units._  |
| define  | [**AGC\_STATE\_MAGIC**](agc__core_8h.md#define-agc_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('A', 'G', 'C', ' ')`<br> |
| define  | [**AGC\_STATE\_VERSION**](agc__core_8h.md#define-agc_state_version)  `3u /\* v3: telemetry attachment (zeroed in blob) \*/`<br> |

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



which converges to `gain_db` = ref\_db - px\_db with a time constant of roughly `1/`(4\*loop\_bw) samples, independent of the absolute signal level.




**
**

The reduction above treats `px_db` as given. It is not: the detector is inside this loop and it measures in POWER, so a quiet input's dB reading approaches from the wrong side of a concave log and crawls. The level-independence therefore belongs to the loop filter alone, and the OBJECT's settling is level-dependent. Measured, 1/e settling at `loop_bw` 0.005 against a predicted 50 samples:


+40 dB in: 41 +20 dB: 45 -20 dB: 84 -40 dB: 109


The asymmetry scales with the detector's own bandwidth — the spread between a +40 dB and a -40 dB input is 0.21 at `alpha` 0.2, 1.01 at 0.05, and 2.98 at 0.01. A composing receiver sizing a warm-up budget from `1/`(4\*loop\_bw) alone will be optimistic by up to 3x on a weak signal. See docs/design/agc.md section 6.




**
**

`p_avg` is an exponential moving average (1-pole leaky integrator) of the instantaneous output power `|y|^2`. `alpha` in (0, 1] sets the detector bandwidth: small `alpha` smooths hard but reacts slowly.


Its input is this object's ONE safety boundary — see [**AGC\_POWER\_CEIL**](agc__core_8h.md#define-agc_power_ceil). The EMA is where an input sample first becomes persistent state, so it is the only place a malformed input can do lasting damage, and the only place guarded.




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
* `loop_bw` Loop noise bandwidth in cycles/sample. The FILTER's time constant is `1/`(4\*loop\_bw) samples; the object settles more slowly than that on a quiet input, because the detector is inside the loop and measures in power (see the Linear-in-dB note above — measured 1.7x to 2.2x at -40 dB in, worse at small `alpha`). Treat `1/`(4\*loop\_bw) as a floor on settling, not an estimate of it. Smaller values are slower and smoother; keep well below `1/`(4\*decim) when using [**agc\_steps()**](agc__core_8h.md#function-agc_steps). 
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




**
**

`z` is saturated into the range the exponent field can hold before it is used. Without that, assembling ``((int64\_t)zi + 1023) &lt;&lt; 52 overflows into the SIGN bit for `|v|` past ~308, and the function returns a **negative** result where the true answer is `+inf` or 0 — measured, `agc_exp10_(309)` gave `-3.09e-308` and `agc_exp10_`(-320) gave `-3.23e+296`. A gain function that returns a negative gain does not merely lose precision, it inverts the signal. Past the rails this now saturates at `2^±1023` instead.


NaN takes the LOW rail, and the direction is the same one [**AGC\_POWER\_CEIL**](agc__core_8h.md#define-agc_power_ceil) uses: when the input is unknown, attenuate. A gain saturated low is silence; a gain saturated high rails everything downstream of it. 


        

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



### function agc\_get\_state 

```C++
void agc_get_state (
    const agc_state_t * state,
    void * blob
) 
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




**
**

`p` is saturated into `[AGC_POWER_FLOOR, AGC_POWER_CEIL]` first. The bit-field split has no notion of a special value: handed a NaN it reads the exponent as an ordinary 1024 and returns a perfectly plausible `+308`, where libm's `log10` returns NaN. Measured on the unguarded version, that fabricated level was what turned a stalled AGC into a runaway one — the loop believed it was seeing `+3084` dB and drove the gain the other way, forever. A wrong answer that looks like a right one is worse than an infinity.


The floor is why silence reads as about `-300` dB rather than `-INF`, so that promise is now structural rather than something each caller has to remember to add. 


        

<hr>



### function agc\_power\_ 

_Power \|y\|^2 in the detector's working precision (double)._ 
```C++
JM_FORCEINLINE double agc_power_ (
    float complex y
) 
```



The power detector EMA, the dB loop filter and `agc_log10_` all work in double across the AGC's full (dB) dynamic range, so the squaring promotes the float components once. Defined here so [**agc\_step()**](agc__core_8h.md#function-agc_step) — and any composing sample loop that accumulates AGC input power — measures power identically. 


        

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



### function agc\_set\_state 

```C++
int agc_set_state (
    agc_state_t * state,
    const void * blob
) 
```




<hr>



### function agc\_set\_telemetry 

_Attach (or detach) a telemetry context and register the AGC's probes on it. Registers two probes, both recorded once per gain-update event and further thinned by decim:_ 
```C++
int agc_set_telemetry (
    agc_state_t * state,
    dp_tlm_t * tlm,
    const char * prefix,
    uint32_t decim
) 
```




* "&lt;prefix&gt;.gain\_db" — the loop-filter integrator, i.e. the gain the loop is commanding, in dB.
* "&lt;prefix&gt;.level\_db" — the level the power detector measures, `10*log10(p_avg)`, in dB. This is the loop's _input_: the integrator drives `ref_db - level_db` to zero, so level\_db is the zero-referenced settling indicator. Reading it says whether the loop has converged without knowing the true input level, which gain\_db alone cannot — gain\_db settles to an offset that depends on how loud the signal happens to be.




The pair is emitted from one update, with level\_db being the belief that update was answering (measured before the correction is applied). Passing NULL detaches (probe sites revert to their single-branch disabled cost); re-attaching after a reset is idempotent (same name -&gt; same probe id). Setup path, never hot: call before the producer thread starts stepping, and keep every object attached to one context on that one thread (the ring is SPSC — see [**dp\_tlm/dp\_tlm\_core.h**](dp__tlm__core_8h.md)). The context is borrowed, not owned: it must outlive the attachment. 

**Parameters:**


* `state` Must be non-NULL. 
* `tlm` Telemetry context to attach, or NULL to detach. 
* `prefix` Probe-name prefix, e.g. "agc" or "rx.agc". 
* `decim` Emit every decim-th gain update; &gt;= 1. 



**Returns:**

DP\_OK, or DP\_ERR\_INVALID when the probe table cannot take both probes or a prefixed name is invalid (the attach fails whole; the object stays detached). 
```C++
>>> import numpy as np
>>> from doppler.agc import AGC
>>> from doppler.telemetry import Telemetry
>>> tlm = Telemetry(1 << 12)
>>> agc = AGC(ref_db=0.0, loop_bw=0.0025, alpha=0.05)
>>> agc.set_telemetry(tlm, "agc")
>>> sorted(tlm.probe_names)
['agc.gain_db', 'agc.level_db']
>>> x = (0.5 + 0j) * np.ones(4096, dtype=np.complex64)
>>> _ = agc.steps(x)
>>> recs = tlm.read()      # both probes, per decim-chunk update
>>> gain = recs[recs["probe"] == tlm.probe_id("agc.gain_db")]["value"]
>>> lvl = recs[recs["probe"] == tlm.probe_id("agc.level_db")]["value"]
>>> len(gain) == len(lvl) == 4096 // agc.decim
True
>>> round(float(gain[-1]), 1)   # -6 dB input, 0 dB ref -> +6 dB gain
6.0
>>> round(float(lvl[-1]), 1)    # settled: measured level == ref
0.0
```
 





        

<hr>



### function agc\_settling\_samples 

_How many samples this loop needs to settle — the design query._ 
```C++
size_t agc_settling_samples (
    double loop_bw,
    double alpha,
    double gain_err_db,
    double tol_db
) 
```



Answers "how long must I wait before the output level can be trusted", which a caller sizing a warm-up budget, a burst preamble or an acquisition guard has to answer and could not.




**
**

`1/`(4\*loop\_bw) is the loop FILTER's time constant, and the object is not the filter — the detector sits inside the loop and measures in power, so a quiet input settles more slowly (see the Linear-in-dB note above). The real settling is `M/`(4\*loop\_bw) where `M` depends on the starting error and on how fast the detector is relative to the filter, `alpha/`(4\*loop\_bw). Measured, `M` runs from about 0.8 on a loud start to nearly 5 on a quiet one with a slow detector.




**
**

This runs the real [**agc\_step**](agc__core_8h.md#function-agc_step) loop against a constant input and counts, so there is no fitted curve to go stale: the answer is whatever the shipped loop does, and it cannot disagree with the object it describes. Design-time only — it allocates and iterates, so call it while planning a pipeline, never inside one.




**Parameters:**


* `loop_bw` Loop noise bandwidth, as passed to [**agc\_create()**](agc__core_8h.md#function-agc_create). 
* `alpha` Detector EMA coefficient, as passed to [**agc\_create()**](agc__core_8h.md#function-agc_create). 
* `gain_err_db` How far from settled the loop starts, in dB of gain it must apply. POSITIVE for a quiet input (the loop must add gain) — the slow direction, and the one to budget for. For a cold receiver this is the whole input dynamic range it must cover, not the steady-state variation. 
* `tol_db` Settled means within this many dB of the target. 



**Returns:**

Samples to settle (&gt;= 1), or 0 if the arguments are invalid or the loop does not settle within a bounded search. 
```C++
>>> from doppler.agc import settling_samples
>>> settling_samples(0.0025, 0.05, 40.0, 0.5)   # cold, 40 dB quiet
430
>>> settling_samples(0.0025, 0.05, 40.0, 3.0)   # a looser bar is cheaper
294
>>> settling_samples(0.0025, 0.05, -40.0, 0.5)  # loud: the fast direction
175
>>> settling_samples(0.01, 0.05, 40.0, 0.5)     # 4x the bandwidth, ~1/4
112
>>> settling_samples(0.0025, 0.05, 0.1, 0.5)    # already inside tol_db
1
>>> settling_samples(0.0, 0.05, 40.0, 0.5)      # refused, not guessed
0
```
 





        

<hr>



### function agc\_state\_bytes 

```C++
size_t agc_state_bytes (
    const agc_state_t * state
) 
```




<hr>



### function agc\_step 

_Process one complex sample through the per-sample AGC loop. Applies the current gain, measures the output power via the EMA detector, advances the loop-filter integrator, then square-clips the returned sample to_ `clip_db` _. The clip is applied after the detector update, so clipping never disturbs convergence. With the default_`gain_update_period` _== 1 this is the exact per-sample reference path; with_`gain_update_period` _P &gt; 1 the detector and gain-apply still run every sample but the loop-filter command (and the exp10/log10 it needs) refreshes once per P samples — a zero-order hold on the gain that amortises the transcendentals on a sample-rate hot loop, the streaming analogue of_[_**agc\_steps()**_](agc__core_8h.md#function-agc_steps) _' decimation._[_**agc\_steps()**_](agc__core_8h.md#function-agc_steps) _is the faster block equivalent; neither is bit-identical to the P == 1 loop once decimated, but both converge to the same steady state._
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
>>> agc2.step(4.0+0.0j)  # 12 dB loud; first sample at unity gain
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



### function settling\_samples 

```C++
size_t settling_samples (
    double loop_bw,
    double alpha,
    double gain_err_db,
    double tol_db
) 
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



### define AGC\_POWER\_CEIL 

_Power ceiling for the detector, in linear units._ 
```C++
#define AGC_POWER_CEIL `2.3158417847463238e77`
```



The largest `|y|^2` a pair of finite `float` components can produce: `2*FLT_MAX^2`. Any measured power above this came from a non-finite output, which in turn came from a non-finite input or an overflowed gain — never from a signal.




**
**

Every power reaching the EMA is put through [**saturate**](util__core_8h.md#function-saturate) into `[0, AGC_POWER_CEIL]`, with NaN sent to the **ceiling** — an unknown level must drive the gain DOWN, since too little gain loses a signal while too much rails everything downstream.


That boundary, and not the stages around it, because the EMA is the first place an input sample becomes _persistent_ state. The gain multiply ahead of it is transient: a bad sample makes one bad output sample and is gone. Once it folds into `p_avg` it is remembered, and the measured level, the loop integrator and the applied gain are all functions of `p_avg`. One guard here makes the whole chain total, where a clamp at each stage would be several chances to miss one.


It is sufficient because a guarded `p_avg` is a convex combination of a finite `p_avg` and a saturated `p`, so it cannot leave the interval once it starts inside — which `agc_create()` and `agc_reset()` guarantee by seeding it with the reference power. Measured on the unguarded loop, a _single_ non-finite input sample drove `p_avg` to NaN permanently, and a following normal sample did not recover it. 


        

<hr>



### define AGC\_POWER\_FLOOR 

_Power floor for the detector, in linear units._ 
```C++
#define AGC_POWER_FLOOR `1e-30`
```



The low end of [**agc\_log10\_**](agc__core_8h.md#function-agc_log10_)'s saturation range, so a long run of silence yields a large-but-finite measured level — exactly -300 dB — instead of `-INF` / `NaN`, and the `log10()` argument stays a normal (non-denormal) double. The floor lives inside the primitive rather than at each call site, so that promise is structural and not something a caller has to remember to add.




**
**

This said "never reached in normal operation — @c p\_avg is seeded with
the reference power at create/reset", which is true of the seed and says nothing about the steady state. Any gap in the signal reaches it: a muted source, a stream discontinuity, a receiver started on a zero-filled buffer. Measured on the unguarded object, ~800 silent samples — 100 symbols at 8 samples per symbol — left the loop permanently dead, because reaching the floor gave the filter a constant +300 dB error to integrate. [**AGC\_POWER\_CEIL**](agc__core_8h.md#define-agc_power_ceil) is the guard that makes reaching it survivable. 





        

<hr>



### define AGC\_STATE\_MAGIC 

```C++
#define AGC_STATE_MAGIC `DP_FOURCC ('A', 'G', 'C', ' ')`
```




<hr>



### define AGC\_STATE\_VERSION 

```C++
#define AGC_STATE_VERSION `3u /* v3: telemetry attachment (zeroed in blob) */`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/agc/agc_core.h`

