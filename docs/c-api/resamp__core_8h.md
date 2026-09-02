

# File resamp\_core.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**resamp**](dir_289a9297ce406b952fab973539197d1c.md) **>** [**resamp\_core.h**](resamp__core_8h.md)

[Go to the source code of this file](resamp__core_8h_source.md)

_Continuously-variable polyphase resampler for CF32 IQ._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "dp_state.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**resamp\_state\_t**](structresamp__state__t.md) <br> |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**resamp\_state\_t**](structresamp__state__t.md) \* | [**resamp\_create**](#function-resamp_create) (double rate) <br> |
|  [**resamp\_state\_t**](structresamp__state__t.md) \* | [**resamp\_create\_custom**](#function-resamp_create_custom) (size\_t num\_phases, size\_t num\_taps, const float \* bank, double rate) <br> |
|  double | [**resamp\_dc\_gain**](#function-resamp_dc_gain) (const [**resamp\_state\_t**](structresamp__state__t.md) \* state) <br>_The resampler's response to a constant input, from its own bank._  |
|  void | [**resamp\_destroy**](#function-resamp_destroy) ([**resamp\_state\_t**](structresamp__state__t.md) \* state) <br> |
|  size\_t | [**resamp\_execute**](#function-resamp_execute) ([**resamp\_state\_t**](structresamp__state__t.md) \* state, const float \_Complex \* in, size\_t num\_in, float \_Complex \* out, size\_t max\_out) <br>_Resample a block of CF32 samples (fixed rate)._  |
|  size\_t | [**resamp\_execute\_ctrl**](#function-resamp_execute_ctrl) ([**resamp\_state\_t**](structresamp__state__t.md) \* state, const float \_Complex \* in, const double \* ctrl, size\_t num\_in, float \_Complex \* out, size\_t max\_out) <br>_Resample with per-sample additive rate deviation._  |
|  size\_t | [**resamp\_execute\_ctrl\_push**](#function-resamp_execute_ctrl_push) ([**resamp\_state\_t**](structresamp__state__t.md) \* state, float \_Complex x, double ctrl, float \_Complex \* out, size\_t max\_out) <br>_Push one input at an instantaneous rate deviation; emit any outputs._  |
|  double | [**resamp\_get\_ctrl\_acc**](#function-resamp_get_ctrl_acc) (const [**resamp\_state\_t**](structresamp__state__t.md) \* state) <br>_The control accumulator's fractional phase, in [0, 1)._  |
|  double | [**resamp\_get\_delay**](#function-resamp_get_delay) (const [**resamp\_state\_t**](structresamp__state__t.md) \* state) <br>_Group delay of the interpolator, in INPUT samples._  |
|  size\_t | [**resamp\_get\_num\_phases**](#function-resamp_get_num_phases) (const [**resamp\_state\_t**](structresamp__state__t.md) \* state) <br> |
|  size\_t | [**resamp\_get\_num\_taps**](#function-resamp_get_num_taps) (const [**resamp\_state\_t**](structresamp__state__t.md) \* state) <br> |
|  double | [**resamp\_get\_rate**](#function-resamp_get_rate) (const [**resamp\_state\_t**](structresamp__state__t.md) \* state) <br> |
|  void | [**resamp\_get\_state**](#function-resamp_get_state) (const [**resamp\_state\_t**](structresamp__state__t.md) \* state, void \* blob) <br>_Serialize_ `state's` _mutable state into_`blob` _._ |
|  size\_t | [**resamp\_interp\_fill**](#function-resamp_interp_fill) ([**resamp\_state\_t**](structresamp__state__t.md) \* state, const float \_Complex \* in, float \_Complex \* out, size\_t max\_out) <br>_Emit exactly_ `max_out` _interpolated outputs, pulling inputs on overflow._ |
|  size\_t | [**resamp\_interp\_inputs\_needed**](#function-resamp_interp_inputs_needed) (const [**resamp\_state\_t**](structresamp__state__t.md) \* state, size\_t max\_out) <br>_Input samples an interpolating fill of_ `max_out` _outputs consumes._ |
|  void | [**resamp\_reset**](#function-resamp_reset) ([**resamp\_state\_t**](structresamp__state__t.md) \* state) <br> |
|  void | [**resamp\_set\_rate**](#function-resamp_set_rate) ([**resamp\_state\_t**](structresamp__state__t.md) \* state, double rate) <br> |
|  int | [**resamp\_set\_state**](#function-resamp_set_state) ([**resamp\_state\_t**](structresamp__state__t.md) \* state, const void \* blob) <br>_Restore mutable state from_ `blob` _(same rate)._ |
|  size\_t | [**resamp\_state\_bytes**](#function-resamp_state_bytes) (const [**resamp\_state\_t**](structresamp__state__t.md) \* state) <br>_Bytes_ [_**resamp\_get\_state()**_](resamp__core_8h.md#function-resamp_get_state) _writes for_`state` _(envelope + payload)._ |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**RESAMP\_CTRL\_RATE\_MIN**](resamp__core_8h.md#define-resamp_ctrl_rate_min)  `1e-6`<br> |
| define  | [**RESAMP\_STATE\_MAGIC**](resamp__core_8h.md#define-resamp_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('R', 'S', 'M', 'P')`<br> |
| define  | [**RESAMP\_STATE\_VERSION**](resamp__core_8h.md#define-resamp_state_version)  `2u`<br> |

## Detailed Description


Two execute paths:


resamp\_execute — dual-mode:
* Interpolation (rate &gt;= 1): output-driven, one NCO tick per output sample, overflow pushes the next input into the delay line.
* Decimation (rate &lt; 1): input-driven transposed-form polyphase. Each input is multiplied by the current polyphase arm and accumulated into N integrate-and-dump registers; on NCO overflow the I&D dump through a transposed tapped delay line to produce one output. Bank coefficients are pre-scaled by rate so the passband gain is unity.




resamp\_execute\_ctrl — unified, and it rides the INTERPOLATOR at every rate: emit at every tick, and load an input when the accumulator fails to advance (`u(k) <= u(k-1)`). The steered rate `rate + ctrl(i)` sets the step; whole input intervals per output are owed as `ctrl_debt` and the fractional remainder selects the arm.


This paragraph used to describe the other structure — "each input
    advances the accumulator by (rate + ctrl(i)); every time the
    accumulator crosses 1.0 an output is emitted" — which is the DECIMATOR's recurrence, exact only at rate 1 where the two coincide. Running it here cost 55-60 dB of tone purity at every other rate. The description is kept accurate rather than deleted because it is the mental model under which someone writes a private `(uint32_t) (frac * 2^32 + 0.5)` and believes it correct; one did, and it stalled the interpolator for composite rates just above unity.


Phase accumulator (execute): upper log2(num\_phases) bits of the 32-bit NCO word index the polyphase bank — nearest-neighbor, no interpolation between branches.


Default constructor builds a 4096-phase × 19-tap Kaiser bank (60 dB rejection, 0.4/0.6 pass/stop) at first call. Use [**resamp\_create\_custom()**](resamp__core_8h.md#function-resamp_create_custom) to supply your own bank.


Lifecycle: 
```C++
resamp_state_t *r = resamp_create(0.5);
float _Complex out[64];
size_t n = resamp_execute(r, in, 128, out, 64);
resamp_destroy(r);
```
 


    
## Public Functions Documentation




### function resamp\_create 

```C++
resamp_state_t * resamp_create (
    double rate
) 
```



Built-in 4096×19 Kaiser bank (60 dB, 0.4/0.6 pass/stop). 


        

<hr>



### function resamp\_create\_custom 

```C++
resamp_state_t * resamp_create_custom (
    size_t num_phases,
    size_t num_taps,
    const float * bank,
    double rate
) 
```



User-supplied bank, shape num\_phases × num\_taps, row-major. num\_phases must be a power of two. 


        

<hr>



### function resamp\_dc\_gain 

_The resampler's response to a constant input, from its own bank._ 
```C++
double resamp_dc_gain (
    const resamp_state_t * state
) 
```



Every arm of a polyphase bank is the same filter at a different fractional delay, so they share one DC gain and arm 0 answers for all of them: the sum of its taps. Computed, not measured — a caller (or a gate) can ask what gain this stage contributes without running a signal through it.


The decimating path pre-scales by `rate` and integrates over the whole bank between outputs, which cancels: `rate` inputs' worth of taps per output, so the tap sum is the answer on both paths.


ARM 0's gain, which is the realised gain only where arm 0 is the arm being used — at rate 1, where the fraction is zero and one arm is selected forever. A non-unity rate visits every arm, so what a caller measures is the arm AVERAGE: 1.000586 computed against 1.000293 at rate 0.5 and 2, and 1.000249 at 0.25 and 4. The 3.4e-4 spread is the bank's arm-to-arm ripple and of no practical consequence, but this returns the computed number, not the one a measurement will find. Pinned by test\_resamp\_core.c §13, which checks both.




**Parameters:**


* `state` State. Must be non-NULL. 



**Returns:**

The DC gain. 1.0 for the default Kaiser bank; a matched pulse bank is a matched filter, not a flat one, so its DC gain is the pulse's own `sum(h)/sum(h^2)` and is not expected to be 1.



```C++
resamp_state_t *r = resamp_create (0.5);
printf ("%.3f\n", resamp_dc_gain (r));   // 1.000
resamp_destroy (r);
```
 


        

<hr>



### function resamp\_destroy 

```C++
void resamp_destroy (
    resamp_state_t * state
) 
```



Free all resources. NULL is a no-op. 


        

<hr>



### function resamp\_execute 

_Resample a block of CF32 samples (fixed rate)._ 
```C++
size_t resamp_execute (
    resamp_state_t * state,
    const float _Complex * in,
    size_t num_in,
    float _Complex * out,
    size_t max_out
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `in` Input samples. 
* `num_in` Number of input samples. 
* `out` Output buffer. 
* `max_out` Capacity of out in samples. 



**Returns:**

Number of output samples written. 





        

<hr>



### function resamp\_execute\_ctrl 

_Resample with per-sample additive rate deviation._ 
```C++
size_t resamp_execute_ctrl (
    resamp_state_t * state,
    const float _Complex * in,
    const double * ctrl,
    size_t num_in,
    float _Complex * out,
    size_t max_out
) 
```



rate\_i = base\_rate + `ctrl[i]`. The control is real-valued and double-precision, matching [**resamp\_execute\_ctrl\_push()**](resamp__core_8h.md#function-resamp_execute_ctrl_push)'s scalar `ctrl` and the `double` the base rate itself is configured in.


Output buffer: allocate ceil(num\_in × (rate + max\_ctrl)) samples.




**Parameters:**


* `state` Must be non-NULL. 
* `in` Input CF32 samples (length num\_in). 
* `ctrl` Rate deviations, parallel to in (length num\_in). 
* `num_in` Number of input samples (= length of ctrl). 
* `out` Output buffer. 
* `max_out` Capacity of out in samples. 



**Returns:**

Number of output samples written. 





        

<hr>



### function resamp\_execute\_ctrl\_push 

_Push one input at an instantaneous rate deviation; emit any outputs._ 
```C++
size_t resamp_execute_ctrl_push (
    resamp_state_t * state,
    float _Complex x,
    double ctrl,
    float _Complex * out,
    size_t max_out
) 
```



The single-input streaming form of [**resamp\_execute\_ctrl()**](resamp__core_8h.md#function-resamp_execute_ctrl): OFFERS `x` to the delay line, advances the accumulator by `rate + ctrl`, and emits every output whose period completes (0 for a decimator between strobes, 1 typically, or several for an interpolator) at the polyphase arm the fractional remainder selects.


**The scalar and block forms are INDISTINGUISHABLE.** Feeding a stream one sample at a time through here yields the same outputs, in the same number, bit-for-bit, as one [**resamp\_execute\_ctrl()**](resamp__core_8h.md#function-resamp_execute_ctrl) over the same `(in, ctrl[])`. Not "close" and not "one sample of delay apart": the same. A caller chooses between them for control flow — the block form when `ctrl[]` is known in advance, this one when each correction depends on the outputs already emitted — never for a difference in what comes out.


That is what "offers" buys, and why `x` is NOT pushed on entry: nothing enters an interpolator's delay line without a load REQUEST — a tick emits, the accumulator fails to advance, and only then is an input consumed. `x` is held until a tick asks. Pushing on entry is an unrequested load, and it is precisely what broke the invariant, costing exactly one sample of group delay against the block form. Fixed, and gated: `eq_ctrl_push` in test\_resamp\_core.c asserts bit-exact equality across decimating, unity-neighbourhood and interpolating rates, at zero and at both signs of steer.


`ctrl_ahead` covers the one case the API cannot decline — `max_out` ending the call before any tick could ask for the offered sample. Feeding a stream of `(x, ctrl)` through this one input at a time reproduces [**resamp\_execute\_ctrl()**](resamp__core_8h.md#function-resamp_execute_ctrl) on the same `(in, ctrl[])` bit-for-bit — but, unlike the block form's precomputed `ctrl[]`, `ctrl` here can depend on the outputs already emitted. That closes the loop: a timing-recovery or rate-tracking loop reads each emitted output, computes its correction, and feeds it back as the next call's `ctrl` to steer the strobe. This is the per-output feedback a matched-filter timing loop (track.RateSync) needs and the block `execute_ctrl` cannot provide.




**Parameters:**


* `state` Must be non-NULL. 
* `x` One input sample. 
* `ctrl` Rate deviation added to the base rate for this input (real-valued; the effective rate is `rate + ctrl`). 
* `out` Output buffer for any emitted samples. 
* `max_out` Capacity of `out` (emission stops at this bound). 



**Returns:**

Number of outputs emitted into `out` (0, 1, or more). 





        

<hr>



### function resamp\_get\_ctrl\_acc 

_The control accumulator's fractional phase, in [0, 1)._ 
```C++
double resamp_get_ctrl_acc (
    const resamp_state_t * state
) 
```



This is the timing NCO's state, and observing it is the only way to see what a closed timing loop is actually doing to the sampling instant. `mu` IS the fractional delay applied to the stream, and `floor(mu * num_phases)` is the polyphase arm the **next** output will read — the accumulator advances after the emit, so on return it already describes the output still to come. That holds at EVERY rate, because the control port rides the interpolating structure at every rate (see resamp\_execute\_ctrl); it is not a peculiarity of a decimating stage.


A steady `mu` means the loop has settled on a sampling phase. A `mu` that slews and wraps means a residual RATE error the loop has not absorbed, and one cycle of wrap is one INPUT interval of slip — an output period only at rate 1, where the two coincide.


Reports the CONTROL accumulator, so it stays 0.0 for a caller driving this object through [**resamp\_execute()**](resamp__core_8h.md#function-resamp_execute): the free-running phase is a separate accumulator with no accessor.


Pinned by test\_resamp\_core.c §10 (the arm, read off the output at four rates), §11 (the wrap's unit, against the counting law) and §12. 


        

<hr>



### function resamp\_get\_delay 

_Group delay of the interpolator, in INPUT samples._ 
```C++
double resamp_get_delay (
    const resamp_state_t * state
) 
```



The prototype is a symmetric (linear-phase) Kaiser lowpass, so its delay is its centre: `floor((num_phases * num_taps) | 1) / 2` prototype taps, i.e. that over `num_phases` input samples  9.5 for the built-in bank  plus ONE input sample of pipeline: an output is formed from the delay line as it stands, and only then is the next input loaded, so the newest sample under the taps is the one before the sample being offered. 10.5 for the built-in bank, on every entry point (the free, ctrl and push forms agree; test\_resamp\_core.c §3 measures it by phase differencing on each and holds it to this value).


What it is for: output `k` carries the input at index `k * (1/rate) + (the accumulated ctrl) - delay`. A caller comparing the output timeline with the input's  a code loop fed through doppler\_channel, a test asserting where a chip lands  subtracts this, and a loop started at the input's phase without it is this far from the peak (doppler-dsp/doppler#1189: 5 chips at 2 samples per chip, onto a Gold sidelobe). Closed-form from the bank's geometry, so it is exact for the built-in bank; a [**resamp\_create\_custom()**](resamp__core_8h.md#function-resamp_create_custom) bank is assumed symmetric about the same centre.




**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

The delay in input samples (`>= 1`). 





        

<hr>



### function resamp\_get\_num\_phases 

```C++
size_t resamp_get_num_phases (
    const resamp_state_t * state
) 
```




<hr>



### function resamp\_get\_num\_taps 

```C++
size_t resamp_get_num_taps (
    const resamp_state_t * state
) 
```




<hr>



### function resamp\_get\_rate 

```C++
double resamp_get_rate (
    const resamp_state_t * state
) 
```




<hr>



### function resamp\_get\_state 

_Serialize_ `state's` _mutable state into_`blob` _._
```C++
void resamp_get_state (
    const resamp_state_t * state,
    void * blob
) 
```




<hr>



### function resamp\_interp\_fill 

_Emit exactly_ `max_out` _interpolated outputs, pulling inputs on overflow._
```C++
size_t resamp_interp_fill (
    resamp_state_t * state,
    const float _Complex * in,
    float _Complex * out,
    size_t max_out
) 
```



The output-count-driven twin of the interpolation branch of [**resamp\_execute()**](resamp__core_8h.md#function-resamp_execute): it emits one output per phase tick and pushes the next input on each NCO overflow, but — unlike [**resamp\_execute()**](resamp__core_8h.md#function-resamp_execute), whose loop halts as soon as the input is exhausted even with output capacity left — it always writes `max_out` outputs. The caller must therefore supply at least resamp\_interp\_inputs\_needed(state, max\_out) inputs in `in`; supplying exactly that many (the common case) consumes them all. This is what lets a streaming producer (e.g. a pulse-shaping synth) feed symbols on demand and get a bit-exact match between a single call for `max_out` outputs and `max_out` single-output calls (the resampler is block-boundary invariant).




**Parameters:**


* `state` Must be non-NULL, upsampling. 
* `in` Inputs to push on overflow (&gt;= inputs\_needed available). 
* `out` Output buffer, capacity &gt;= `max_out`. 
* `max_out` Number of outputs to emit. 



**Returns:**

Inputs consumed (== resamp\_interp\_inputs\_needed(state, max\_out)). 





        

<hr>



### function resamp\_interp\_inputs\_needed 

_Input samples an interpolating fill of_ `max_out` _outputs consumes._
```C++
size_t resamp_interp_inputs_needed (
    const resamp_state_t * state,
    size_t max_out
) 
```



The exact number of delay-line pushes producing `max_out` outputs from the current phase: `((uint64_t)phase + max_out * phase_inc) >> 32`.


Exact at EVERY rate, not only at an integer interpolation factor, and a caller may rely on it: generate precisely this many inputs, hand them to [**resamp\_interp\_fill()**](resamp__core_8h.md#function-resamp_interp_fill), and there is no over- or under-production. The guarantee is structural rather than numeric — this closed form and the fill loop advance the same accumulator by the same `phase_inc`, so they cannot disagree whatever the rate, and mid-stream phase is carried in the formula. Measured across 1800 mid-stream calls at nine rates with randomised `max_out`, worst deviation zero (test\_resamp\_core.c §20).


(This paragraph used to scope the promise to "an integer interpolation
factor". That is where phase\_inc divides evenly and therefore represents the rate exactly — a real property, but a different one: the prediction matches the fill because both USE phase\_inc, not because it is exact.)


Meaningful only for an upsampling resampler (rate &gt;= 1) — the prediction still matches what [**resamp\_interp\_fill()**](resamp__core_8h.md#function-resamp_interp_fill) consumes below unity, but that entry point interpolates, so a decimating caller wants [**resamp\_execute()**](resamp__core_8h.md#function-resamp_execute).




**Parameters:**


* `state` Must be non-NULL, upsampling. 
* `max_out` Number of outputs the following [**resamp\_interp\_fill()**](resamp__core_8h.md#function-resamp_interp_fill) call will request. 



**Returns:**

Inputs that call will consume. 





        

<hr>



### function resamp\_reset 

```C++
void resamp_reset (
    resamp_state_t * state
) 
```



Zero phase accumulator, ctrl accumulator, and delay line. Rate and bank are preserved. 


        

<hr>



### function resamp\_set\_rate 

```C++
void resamp_set_rate (
    resamp_state_t * state,
    double rate
) 
```



Update rate and recompute phase\_inc. Accumulator phase and delay line are preserved. Switching between interp and decim modes requires a new create() + destroy() pair. 


        

<hr>



### function resamp\_set\_state 

_Restore mutable state from_ `blob` _(same rate)._
```C++
int resamp_set_state (
    resamp_state_t * state,
    const void * blob
) 
```





**Returns:**

DP\_OK, or DP\_ERR\_INVALID if the blob's envelope rejects. 





        

<hr>



### function resamp\_state\_bytes 

_Bytes_ [_**resamp\_get\_state()**_](resamp__core_8h.md#function-resamp_get_state) _writes for_`state` _(envelope + payload)._
```C++
size_t resamp_state_bytes (
    const resamp_state_t * state
) 
```




<hr>
## Macro Definition Documentation





### define RESAMP\_CTRL\_RATE\_MIN 

```C++
#define RESAMP_CTRL_RATE_MIN `1e-6`
```




<hr>



### define RESAMP\_STATE\_MAGIC 

```C++
#define RESAMP_STATE_MAGIC `DP_FOURCC ('R', 'S', 'M', 'P')`
```




<hr>



### define RESAMP\_STATE\_VERSION 

```C++
#define RESAMP_STATE_VERSION `2u`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/resamp/resamp_core.h`

