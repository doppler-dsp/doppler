

# File ddc\_core.h



[**FileList**](files.md) **>** [**ddc**](dir_b33dc116452ac5c7d7799725e78b6bdc.md) **>** [**ddc\_core.h**](ddc__core_8h.md)

[Go to the source code of this file](ddc__core_8h_source.md)

_Digital Down-Converter — composes LO + RateConverter cascade._ [More...](#detailed-description)

* `#include <complex.h>`
* `#include <stdbool.h>`
* `#include <stddef.h>`
* `#include "lo/lo_core.h"`
* `#include "RateConverter/RateConverter_core.h"`
* `#include "resamp/resamp_core.h"`
* `#include "hbdecim/hbdecim_core.h"`
* `#include "cic/cic_core.h"`
* `#include "fir/fir_core.h"`
* `#include "resample/resample_core.h"`
* `#include "agc/agc_core.h"`
* `#include "dp_tlm/dp_tlm_core.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**ddc\_extra\_t**](structddc__extra__t.md) <br> |
| struct | [**ddc\_state**](structddc__state.md) <br>_Ddc state — an LO and the cascade it feeds._  |


## Public Types

| Type | Name |
| ---: | :--- |
| typedef struct [**ddc\_state**](structddc__state.md) | [**ddc\_state\_t**](#typedef-ddc_state_t)  <br>_Ddc state — an LO and the cascade it feeds._  |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**ddc\_state\_t**](ddc__core_8h.md#typedef-ddc_state_t) \* | [**ddc\_create**](#function-ddc_create) (double norm\_freq, double rate) <br>_Create a complex-input Digital Down-Converter. Allocates internal state for the LO and RateConverter cascade. The RateConverter selects the cheapest multi-stage decimation chain (CIC + optional halfband + polyphase resampler) for the given rate._  |
|  [**ddc\_state\_t**](ddc__core_8h.md#typedef-ddc_state_t) \* | [**ddc\_create\_matched**](#function-ddc_create_matched) (double norm\_freq, double rate, int pulse, double beta, size\_t span, double pulse\_sps, size\_t num\_phases) <br>_Create a DDC whose cascade's terminal stage IS a matched filter._  |
|  void | [**ddc\_destroy**](#function-ddc_destroy) ([**ddc\_state\_t**](ddc__core_8h.md#typedef-ddc_state_t) \* state) <br>_Free all resources held by a DDC instance. Releases the RateConverter and LO substructures, then the struct itself. Passing NULL is a no-op._  |
|  size\_t | [**ddc\_execute**](#function-ddc_execute) ([**ddc\_state\_t**](ddc__core_8h.md#typedef-ddc_state_t) \* state, const float complex \* x, size\_t x\_len, float complex \* out, size\_t max\_out) <br>_Mix and resample a block of CF32 samples. Multiplies each input sample by the current LO phasor (advancing the NCO phase per sample), then feeds the mixed block into the RateConverter. The resampler maintains history across calls, so arbitrary block sizes produce contiguous output with no edge artefacts. Output length ≈ x\_len \* rate (varies by ±1 due to polyphase indexing)._  |
|  size\_t | [**ddc\_execute\_ctrl**](#function-ddc_execute_ctrl) ([**ddc\_state\_t**](ddc__core_8h.md#typedef-ddc_state_t) \* state, const float complex \* x, size\_t x\_len, double rate\_ctrl, double freq\_ctrl, float complex \* out, size\_t max\_out) <br>_Mix and resample a block, steering both control ports._  |
|  size\_t | [**ddc\_execute\_ctrl\_max\_out**](#function-ddc_execute_ctrl_max_out) ([**ddc\_state\_t**](ddc__core_8h.md#typedef-ddc_state_t) \* state, size\_t x\_len) <br> |
|  size\_t | [**ddc\_execute\_ctrl\_push**](#function-ddc_execute_ctrl_push) ([**ddc\_state\_t**](ddc__core_8h.md#typedef-ddc_state_t) \* state, float complex x, double rate\_ctrl, double freq\_ctrl, float complex \* out, size\_t max\_out) <br>_Push ONE input sample; emit whatever outputs it completes._  |
|  size\_t | [**ddc\_execute\_ctrl\_push\_max\_out**](#function-ddc_execute_ctrl_push_max_out) ([**ddc\_state\_t**](ddc__core_8h.md#typedef-ddc_state_t) \* state) <br> |
|  size\_t | [**ddc\_execute\_ctrl\_push\_tap**](#function-ddc_execute_ctrl_push_tap) ([**ddc\_state\_t**](ddc__core_8h.md#typedef-ddc_state_t) \* state, float complex x, double rate\_ctrl, double freq\_ctrl, float complex \* out, size\_t max\_out, float complex \* lo\_out, int \* n\_lo) <br>[_**ddc\_execute\_ctrl\_push()**_](ddc__core_8h.md#function-ddc_execute_ctrl_push) _that also hands back the post-LO sample._ |
|  size\_t | [**ddc\_execute\_ctrl\_push\_tap2**](#function-ddc_execute_ctrl_push_tap2) ([**ddc\_state\_t**](ddc__core_8h.md#typedef-ddc_state_t) \* state, float complex x, double rate\_ctrl, double freq\_ctrl, float complex \* out, size\_t max\_out, float complex \* lo\_out, int \* n\_lo, float complex \* pre\_out, int \* n\_pre) <br>[_**ddc\_execute\_ctrl\_push\_tap()**_](ddc__core_8h.md#function-ddc_execute_ctrl_push_tap) _, plus the PRE-TERMINAL tap._ |
|  size\_t | [**ddc\_execute\_max\_out**](#function-ddc_execute_max_out) ([**ddc\_state\_t**](ddc__core_8h.md#typedef-ddc_state_t) \* state, size\_t x\_len) <br>_Maximum output samples one execute() of x\_len inputs can produce._  |
|  double | [**ddc\_get\_bank\_sps**](#function-ddc_get_bank_sps) (const [**ddc\_state\_t**](ddc__core_8h.md#typedef-ddc_state_t) \* state) <br>_Samples per symbol of the pre-terminal tap; a planner outcome._  |
|  bool | [**ddc\_get\_clipped**](#function-ddc_get_clipped) (const [**ddc\_state\_t**](ddc__core_8h.md#typedef-ddc_state_t) \* state) <br>_Has the cascade's CIC clipped its input since the last reset?_  |
|  bool | [**ddc\_get\_narrow\_pulse**](#function-ddc_get_narrow_pulse) (const [**ddc\_state\_t**](ddc__core_8h.md#typedef-ddc_state_t) \* state) <br>_Is this object's rectangular matched filter degenerately narrow?_  |
|  double | [**ddc\_get\_norm\_freq**](#function-ddc_get_norm_freq) (const [**ddc\_state\_t**](ddc__core_8h.md#typedef-ddc_state_t) \* state) <br>_Return the current LO normalised frequency (cycles/sample)._  |
|  double | [**ddc\_get\_rate**](#function-ddc_get_rate) (const [**ddc\_state\_t**](ddc__core_8h.md#typedef-ddc_state_t) \* state) <br>_Return the configured output/input rate ratio (read-only). The rate is fixed at create time; change it by destroying and recreating the DDC with the new value._  |
|  void | [**ddc\_get\_state**](#function-ddc_get_state) (const [**ddc\_state\_t**](ddc__core_8h.md#typedef-ddc_state_t) \* state, void \* blob) <br>_Serialize_ `state's` _LO + RateConverter state into_`blob` _._ |
|  void | [**ddc\_reset**](#function-ddc_reset) ([**ddc\_state\_t**](ddc__core_8h.md#typedef-ddc_state_t) \* state) <br>_Zero LO phase and resampler history. After reset, the next execute call produces the same output as the first execute after create — useful for reproducible block-by-block processing or looped test fixtures._  |
|  size\_t | [**ddc\_run**](#function-ddc_run) ([**ddc\_state\_t**](ddc__core_8h.md#typedef-ddc_state_t) \* state, const void \* state\_in, void \* state\_out, const float complex \* in, size\_t n\_in, float complex \* out, size\_t max\_out) <br>_Pure run:_ `(state_in, input) -> (state_out, output)` _; either blob may be NULL (NULL in = current; NULL out = discard)._ |
|  void | [**ddc\_set\_norm\_freq**](#function-ddc_set_norm_freq) ([**ddc\_state\_t**](ddc__core_8h.md#typedef-ddc_state_t) \* state, double val) <br>_Retune the LO without resetting phase or resampler history. Updates the NCO phase increment atomically so the carrier shift changes seamlessly across block boundaries. The resampler history and LO phase accumulator are left intact, avoiding the transient that a full reset would cause._  |
|  int | [**ddc\_set\_state**](#function-ddc_set_state) ([**ddc\_state\_t**](ddc__core_8h.md#typedef-ddc_state_t) \* state, const void \* blob) <br>_Restore LO + RateConverter state from_ `blob` _._ |
|  int | [**ddc\_set\_telemetry**](#function-ddc_set_telemetry) ([**ddc\_state\_t**](ddc__core_8h.md#typedef-ddc_state_t) \* state, [**dp\_tlm\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_t) \* tlm, const char \* prefix, uint32\_t decim) <br>_Attach (or detach) a telemetry context on the cascade's AGC._  |
|  size\_t | [**ddc\_state\_bytes**](#function-ddc_state_bytes) (const [**ddc\_state\_t**](ddc__core_8h.md#typedef-ddc_state_t) \* state) <br>_Byte size of_ `state's` _blob (envelope + extra + lo + rc)._ |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**DDC\_STATE\_MAGIC**](ddc__core_8h.md#define-ddc_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('D', 'D', 'C', '\_')`<br> |
| define  | [**DDC\_STATE\_VERSION**](ddc__core_8h.md#define-ddc_state_version)  `1u`<br> |

## Detailed Description


Two types:


Ddc — LO mix → RateConverter (the plain flavor) MatchedDDC — the same, with the pulse on the cascade's terminal stage


Streaming: any block size per execute call. The real-input twin lives in [**ddcr/ddcr\_core.h**](ddcr__core_8h.md) (halfband R2C → LO mix → RateConverter); it is the same chain behind a real-to-complex front end.


RateConverter selects the cheapest cascade (CIC + optional halfband + polyphase resampler) for the requested rate at create time. This makes large-ratio decimation (e.g., 100:1) significantly cheaper than a single polyphase stage.


#### Ddc signal chain




```C++
CF32 in (fs_in)  →  LO mix  →  RateConverter  →  CF32 out (fs_out)
```



norm\_freq: NCO normalised frequency (cycles/sample at fs\_in). Set to -f\_carrier to shift a carrier at f\_carrier to DC.



#### Pulse and the two control ports



Both this type and its real-input twin have a matched _flavor_ (`ddc_create_matched` / `ddcr_create_matched`), which is passed straight through to the cascade: the terminal stage carries a matched-filter bank instead of the default Kaiser one, so the chain mixes, decimates and matched-filters in the same dot products it was already doing (see [**RateConverter\_create\_matched()**](RateConverter__core_8h.md#function-rateconverter_create_matched)).


That makes a DDC steerable on **two** ports, which are duals of each other:



```C++
freq_ctrl ──> LO phase accumulator      (carrier, at the INPUT rate)
rate_ctrl ──> terminal stage accumulator (timing, at the OUTPUT rate)
```



Both are per-input deviations added on top of the configured centre value for that sample only, so a tracking loop supplies its full filter output every time and the DDC holds no loop state. A receiver therefore closes a carrier loop and a timing loop with the same `loop_filter`, one per port — the object itself contains no loop.


The LO sits at the input rate (the intermediate rate fs\_in/2 for DdcR), which is where predetection de-rotation belongs: the carrier is wiped off before any filter narrows the band around it.



#### Retuning vs. rebuilding




* **Retune** (centre-frequency change): call ddc\_set\_norm\_freq / ddcr\_set\_norm\_freq. Cheap — updates the LO phase increment without disturbing the resampler history. Seamless across block boundaries.
* **Rate change** (span / decimation change): destroy and recreate the DDC for the new rate.





#### Usage




```C++
// Complex DDC: shift a carrier at +0.1·fs to DC, decimate by 4
ddc_state_t *ddc = ddc_create(-0.1, 0.25);
float _Complex out[4096];
size_t n = ddc_execute(ddc, in, 1024, out, 4096);
ddc_destroy(ddc);
```
 



    
## Public Types Documentation




### typedef ddc\_state\_t 

_Ddc state — an LO and the cascade it feeds._ 
```C++
typedef struct ddc_state ddc_state_t;
```



Do not initialise directly; use [**ddc\_create()**](ddc__core_8h.md#function-ddc_create) or [**ddc\_create\_matched()**](ddc__core_8h.md#function-ddc_create_matched). 


        

<hr>
## Public Functions Documentation




### function ddc\_create 

_Create a complex-input Digital Down-Converter. Allocates internal state for the LO and RateConverter cascade. The RateConverter selects the cheapest multi-stage decimation chain (CIC + optional halfband + polyphase resampler) for the given rate._ 
```C++
ddc_state_t * ddc_create (
    double norm_freq,
    double rate
) 
```





**Parameters:**


* `norm_freq` LO frequency in cycles/sample at the input rate. Set to -f\_carrier to shift a carrier at f\_carrier to DC. Any real value is accepted. 
* `rate` Output rate / input rate. Must be &gt; 0. Values &gt;= 1 are up-sampling; typical use is decimation (0 &lt; rate &lt; 1). 



**Returns:**

Non-NULL on success, NULL on OOM or invalid args.



```C++
>>> from doppler.ddc import DDC
>>> ddc = DDC(norm_freq=-0.1, rate=0.25)
>>> ddc.norm_freq
-0.1
>>> ddc.rate
0.25
```
 


        

<hr>



### function ddc\_create\_matched 

_Create a DDC whose cascade's terminal stage IS a matched filter._ 
```C++
ddc_state_t * ddc_create_matched (
    double norm_freq,
    double rate,
    int pulse,
    double beta,
    size_t span,
    double pulse_sps,
    size_t num_phases
) 
```



The matched _flavor_ of the same object — same state, same methods, one different constructor (Python: `MatchedDDC`). The pulse is a straight passthrough to the cascade, so everything [**RateConverter\_create\_matched()**](RateConverter__core_8h.md#function-rateconverter_create_matched) documents holds here unchanged: the terminal fractional stage always exists, the bank is sized by the POST-decimation rate, and the CIC droop folds into the bank rather than costing a stage. What this layer adds is the mix in front of it, and with it the second control port — [**ddc\_execute\_ctrl()**](ddc__core_8h.md#function-ddc_execute_ctrl) steers the matched filter's polyphase arm (timing) and the LO's phase accumulator (carrier) together.


Droop compensation is not a parameter because it is unconditional here: the fold is worth 28 dB of EVM for six taps per arm and no extra pass over the data, so no operating point wants it off. (The plain [**ddc\_create()**](ddc__core_8h.md#function-ddc_create) path is unchanged and uncompensated.)




**Parameters:**


* `norm_freq` LO frequency in cycles/sample at the input rate, as [**ddc\_create()**](ddc__core_8h.md#function-ddc_create). 
* `rate` Output-to-input sample rate ratio. Rate-agnostic: a caller wanting `m` outputs per symbol asks for `rate = m/sps`; the cascade never learns about symbols. 
* `pulse` RC\_PULSE\_RRC / RC\_PULSE\_IANDD. RC\_PULSE\_NONE is invalid here — use [**ddc\_create()**](ddc__core_8h.md#function-ddc_create) for a plain down-conversion. 
* `beta` RRC roll-off in `[0, 1]` (ignored for the rectangle). 
* `span` One-sided RRC span in symbols (ignored for the rectangle, whose support is exactly one symbol). 
* `pulse_sps` The pulse's period in **output** samples (2 = two samples per symbol out). 
* `num_phases` Terminal-stage arms; a power of two. Sets the timing resolution to `1/num_phases` of an output period. 



**Returns:**

Non-NULL on success, NULL on a bad parameter or OOM.



```C++
>>> from doppler.ddc import MatchedDDC
>>> rx = MatchedDDC(norm_freq=-0.1, rate=2 / 16, pulse="rrc")
>>> rx.rate
0.125
```
 


        

<hr>



### function ddc\_destroy 

_Free all resources held by a DDC instance. Releases the RateConverter and LO substructures, then the struct itself. Passing NULL is a no-op._ 
```C++
void ddc_destroy (
    ddc_state_t * state
) 
```




```C++
>>> from doppler.ddc import DDC
>>> ddc = DDC(norm_freq=0.0, rate=0.25)
>>> ddc.destroy()   # releases C memory immediately
```
 


        

<hr>



### function ddc\_execute 

_Mix and resample a block of CF32 samples. Multiplies each input sample by the current LO phasor (advancing the NCO phase per sample), then feeds the mixed block into the RateConverter. The resampler maintains history across calls, so arbitrary block sizes produce contiguous output with no edge artefacts. Output length ≈ x\_len \* rate (varies by ±1 due to polyphase indexing)._ 
```C++
size_t ddc_execute (
    ddc_state_t * state,
    const float complex * x,
    size_t x_len,
    float complex * out,
    size_t max_out
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `x` CF32 input block; accepted as float32 (auto-cast). 
* `x_len` Number of input samples (C-only, hidden from Python). 
* `out` CF32 output buffer (C-only, hidden from Python). 
* `max_out` Output buffer capacity (C-only, hidden from Python). 



**Returns:**

Number of output samples written (C-only).



```C++
>>> from doppler.ddc import DDC
>>> import numpy as np
>>> ddc = DDC(norm_freq=-0.1, rate=0.25)
>>> t = np.arange(4096)
>>> x = np.exp(1j * 2 * np.pi * 0.1 * t).astype(np.complex64)
>>> y = ddc.execute(x)
>>> y.shape
(1024,)
>>> y.dtype
dtype('complex64')
>>> round(float(abs(y[500])), 2)   # shifted to DC; amplitude ≈ 1
1.0
```
 


        

<hr>



### function ddc\_execute\_ctrl 

_Mix and resample a block, steering both control ports._ 
```C++
size_t ddc_execute_ctrl (
    ddc_state_t * state,
    const float complex * x,
    size_t x_len,
    double rate_ctrl,
    double freq_ctrl,
    float complex * out,
    size_t max_out
) 
```



The control-port form of [**ddc\_execute()**](ddc__core_8h.md#function-ddc_execute): the LO advances by `phase_inc + freq_ctrl` on every sample of this block, and the cascade's terminal stage runs at `stage_rate + rate_ctrl`. Neither deviation is persisted — the centre norm\_freq and rate are untouched — so a tracking loop passes its full filter output on every call and the DDC holds no loop state of its own.


Feeding a stream through [**ddc\_execute\_ctrl\_push()**](ddc__core_8h.md#function-ddc_execute_ctrl_push) one sample at a time reproduces this call bit-for-bit when both controls are held constant, so the cheap block form stays correct for open-loop use (a fixed Doppler offset, a rate trim) and the push form is what a closed loop uses.




**Parameters:**


* `state` Must be non-NULL. 
* `x` CF32 input block. 
* `x_len` Number of input samples. 
* `rate_ctrl` Rate deviation added to the terminal Resampler stage's rate. Referenced to the terminal (post-decimation) rate, not the overall rate; ignored by a plan whose last stage is an integer HB/CIC with nothing to steer. 
* `freq_ctrl` Frequency deviation added to the LO, in cycles/sample at the INPUT rate (any sign). 
* `out` CF32 output buffer. 
* `max_out` Capacity of `out` in samples. 



**Returns:**

Number of output samples written.



```C++
>>> from doppler.ddc import DDC
>>> import numpy as np
>>> ddc = DDC(norm_freq=0.0, rate=0.25)   # LO centred at DC
>>> t = np.arange(4096)
>>> x = np.exp(1j * 2 * np.pi * 0.1 * t).astype(np.complex64)
>>> y = ddc.execute_ctrl(x, 0.0, -0.1)    # freq_ctrl steers +0.1 to DC
>>> y.shape
(1024,)
>>> round(float(abs(y[100:].mean())), 2)  # settled output sits at DC
1.0
```
 


        

<hr>



### function ddc\_execute\_ctrl\_max\_out 

```C++
size_t ddc_execute_ctrl_max_out (
    ddc_state_t * state,
    size_t x_len
) 
```




<hr>



### function ddc\_execute\_ctrl\_push 

_Push ONE input sample; emit whatever outputs it completes._ 
```C++
size_t ddc_execute_ctrl_push (
    ddc_state_t * state,
    float complex x,
    double rate_ctrl,
    double freq_ctrl,
    float complex * out,
    size_t max_out
) 
```



The per-input streaming form of [**ddc\_execute\_ctrl()**](ddc__core_8h.md#function-ddc_execute_ctrl), and the only form a closed loop can use: a block call has to know its whole control history up front, whereas a carrier or timing loop computes each correction _from_ the outputs already emitted. Both loops close once per symbol, so both ports need this form.


The mix costs one LO step per input; the cascade then emits 0 outputs (the common decimating case, between strobes), 1, or several.




**Parameters:**


* `state` Must be non-NULL. 
* `x` One CF32 input sample. 
* `rate_ctrl` Rate deviation for this input (terminal-stage rate). 
* `freq_ctrl` Frequency deviation for this input, cycles/sample at the input rate. 
* `out` Output buffer for any emitted samples. 
* `max_out` Capacity of `out` (emission stops at this bound). 



**Returns:**

Number of outputs written (0, 1, or more).



```C++
>>> from doppler.ddc import DDC
>>> import numpy as np
>>> ddc = DDC(norm_freq=-0.1, rate=0.25)
>>> t = np.arange(64)
>>> x = np.exp(1j * 2 * np.pi * 0.1 * t).astype(np.complex64)
>>> outs = [ddc.execute_ctrl_push(complex(s), 0.0, 0.0) for s in x]
>>> int(sum(len(o) for o in outs))   # 64 inputs, rate 1/4 -> 16 outs
16
>>> [len(o) for o in outs[:4]]        # 0 outs until a strobe completes
[0, 0, 0, 1]
```
 


        

<hr>



### function ddc\_execute\_ctrl\_push\_max\_out 

```C++
size_t ddc_execute_ctrl_push_max_out (
    ddc_state_t * state
) 
```




<hr>



### function ddc\_execute\_ctrl\_push\_tap 

[_**ddc\_execute\_ctrl\_push()**_](ddc__core_8h.md#function-ddc_execute_ctrl_push) _that also hands back the post-LO sample._
```C++
size_t ddc_execute_ctrl_push_tap (
    ddc_state_t * state,
    float complex x,
    double rate_ctrl,
    double freq_ctrl,
    float complex * out,
    size_t max_out,
    float complex * lo_out,
    int * n_lo
) 
```



Identical in every respect, plus a tap on the signal _between_ the mix and the cascade — de-rotated, but not yet decimated or matched-filtered.


The tap exists because a carrier discriminator's unambiguous frequency range is set by the rate it UPDATES at: an M-th-power detector running at rate `F` can only see `|df| < F/(2M)`. Take it from the terminal stage's on-time strobe and that rate is the symbol rate, which is the cleanest possible input and the narrowest possible pull-in. Take it here and the rate is the full input rate — `sps` times wider — at the cost of no matched filtering, so a caller wanting SNR back must run its own arm filter over this stream. That trade is the caller's to make, which is why this is a tap rather than a mode.




**Parameters:**


* `state` Must be non-NULL. 
* `x` One CF32 input sample. 
* `rate_ctrl` Rate deviation for this input (terminal-stage rate). 
* `freq_ctrl` Frequency deviation for this input, cycles/sample at the input rate. 
* `out` Output buffer for any emitted outputs. 
* `max_out` Capacity of `out` (emission stops at this bound). 
* `lo_out` Receives the post-LO, pre-cascade sample when `n_lo` comes back 1. May be NULL. 
* `n_lo` Receives 1 (this front end mixes every input, so always 1 here; the real-input twin gates on its halfband and can return 0). May be NULL. 



**Returns:**

Number of terminal outputs written (0, 1, or more). 





        

<hr>



### function ddc\_execute\_ctrl\_push\_tap2 

[_**ddc\_execute\_ctrl\_push\_tap()**_](ddc__core_8h.md#function-ddc_execute_ctrl_push_tap) _, plus the PRE-TERMINAL tap._
```C++
size_t ddc_execute_ctrl_push_tap2 (
    ddc_state_t * state,
    float complex x,
    double rate_ctrl,
    double freq_ctrl,
    float complex * out,
    size_t max_out,
    float complex * lo_out,
    int * n_lo,
    float complex * pre_out,
    int * n_pre
) 
```



Two taps, at the two points a carrier discriminator can read without symbol timing, and they are not equivalent:



|tap   |where   |cost    |
|-----|-----|-----|
|`lo_out`   |post-LO, pre-cascade   |full input noise BW    |
|`pre_out`   |post-cascade, post-AGC, pre-MF   |none of the above   |






`pre_out` is the better-conditioned of the two for the reasons docs/design/mpsk.md §3.3 gives: the cascade's own filters have already band-limited it and the AGC has already levelled it, so a half-symbol arm filter bolted onto `lo_out` is a hand-rolled approximation of what this node gives for free. Its rate is [**ddc\_get\_bank\_sps()**](ddc__core_8h.md#function-ddc_get_bank_sps) samples per symbol.




**Note:**

"Better conditioned" is not "more accurate", and the distinction is measured rather than assumed. native/validation/rx\_nda\_tap.c finds no residual-frequency-error advantage for this node over the symbol-rate strobe — three taps carrying one loop bandwidth over one signal settle to the same jitter. What it buys is a usable discriminator with no symbol timing and no arm filter; see doppler#766 for the pull-in-range question that would actually separate them.




**Parameters:**


* `state` Must be non-NULL. 
* `x` One CF32 input sample. 
* `rate_ctrl` Rate deviation for this input (terminal-stage rate). 
* `freq_ctrl` Frequency deviation for this input, cycles/sample at the input rate. 
* `out` Output buffer for any emitted outputs. 
* `max_out` Capacity of `out` (emission stops at this bound). 
* `lo_out` Receives the post-LO, pre-cascade sample when `n_lo` comes back 1. May be NULL. 
* `n_lo` Receives 1 (this front end mixes every input, so always 1 here). May be NULL. 
* `pre_out` Receives the pre-terminal sample; may be NULL. 
* `n_pre` Receives 1 if `pre_out` was written, else 0; may be NULL. A non-terminal stage swallows inputs between its decimation strobes, so this is 0 on those calls. 



**Returns:**

Number of terminal outputs written (0, 1, or more). 





        

<hr>



### function ddc\_execute\_max\_out 

_Maximum output samples one execute() of x\_len inputs can produce._ 
```C++
size_t ddc_execute_max_out (
    ddc_state_t * state,
    size_t x_len
) 
```



A DDC decimates (or passes at unity), so the output never exceeds the input length: returns x\_len. The binding sizes the output buffer to this per-call bound and resizes down to the actual count (gh-607).




**Parameters:**


* `state` Must be non-NULL. 
* `x_len` Number of input samples the matching execute() call sees. 



**Returns:**

x\_len (a safe upper bound on the produced samples). 





        

<hr>



### function ddc\_get\_bank\_sps 

_Samples per symbol of the pre-terminal tap; a planner outcome._ 
```C++
double ddc_get_bank_sps (
    const ddc_state_t * state
) 
```




<hr>



### function ddc\_get\_clipped 

_Has the cascade's CIC clipped its input since the last reset?_ 
```C++
bool ddc_get_clipped (
    const ddc_state_t * state
) 
```



Forwarded from [**RateConverter\_get\_clipped()**](RateConverter__core_8h.md#function-rateconverter_get_clipped): a CIC bounds its input to `|Re|, |Im| <= 2.0` (`CIC_PAPR_HEADROOM`, 6 dB above unity — see [**cic\_core.h**](cic__core_8h.md)) and clips silently past it — the output stays finite and plausible, merely distorted, at a cost of ~25 dB of EVM that no downstream metric attributes to the front end. Sticky until [**ddc\_reset()**](ddc__core_8h.md#function-ddc_reset); always false for a plan with no CIC stage, which is the honest answer since those plans are scale-free. 


        

<hr>



### function ddc\_get\_narrow\_pulse 

_Is this object's rectangular matched filter degenerately narrow?_ 
```C++
bool ddc_get_narrow_pulse (
    const ddc_state_t * state
) 
```



True only for the matched flavor built with `pulse = RC_PULSE_IANDD` and fewer than four output samples per symbol: the rectangle is exactly one symbol wide, so its matched filter is a 2-3 tap sum there. It works, it just barely opens the eye — measured on the timing loop this feeds, a lock statistic of -0.34 at two samples per symbol against +0.95 at four. The RRC spans many symbols and is never affected. Construction also raises a UserWarning, so this is the pull half of the same diagnostic. 


        

<hr>



### function ddc\_get\_norm\_freq 

_Return the current LO normalised frequency (cycles/sample)._ 
```C++
double ddc_get_norm_freq (
    const ddc_state_t * state
) 
```




```C++
>>> from doppler.ddc import DDC
>>> ddc = DDC(norm_freq=-0.1, rate=0.25)
>>> ddc.norm_freq
-0.1
```
 


        

<hr>



### function ddc\_get\_rate 

_Return the configured output/input rate ratio (read-only). The rate is fixed at create time; change it by destroying and recreating the DDC with the new value._ 
```C++
double ddc_get_rate (
    const ddc_state_t * state
) 
```




```C++
>>> from doppler.ddc import DDC
>>> ddc = DDC(norm_freq=0.0, rate=0.25)
>>> ddc.rate
0.25
```
 


        

<hr>



### function ddc\_get\_state 

_Serialize_ `state's` _LO + RateConverter state into_`blob` _._
```C++
void ddc_get_state (
    const ddc_state_t * state,
    void * blob
) 
```




<hr>



### function ddc\_reset 

_Zero LO phase and resampler history. After reset, the next execute call produces the same output as the first execute after create — useful for reproducible block-by-block processing or looped test fixtures._ 
```C++
void ddc_reset (
    ddc_state_t * state
) 
```




```C++
>>> from doppler.ddc import DDC
>>> import numpy as np
>>> ddc = DDC(norm_freq=0.0, rate=0.25)
>>> x = np.ones(64, dtype=np.complex64)
>>> y1 = ddc.execute(x)
>>> ddc.reset()
>>> y2 = ddc.execute(x)
>>> bool(np.array_equal(y1, y2))
True
```
 


        

<hr>



### function ddc\_run 

_Pure run:_ `(state_in, input) -> (state_out, output)` _; either blob may be NULL (NULL in = current; NULL out = discard)._
```C++
size_t ddc_run (
    ddc_state_t * state,
    const void * state_in,
    void * state_out,
    const float complex * in,
    size_t n_in,
    float complex * out,
    size_t max_out
) 
```




<hr>



### function ddc\_set\_norm\_freq 

_Retune the LO without resetting phase or resampler history. Updates the NCO phase increment atomically so the carrier shift changes seamlessly across block boundaries. The resampler history and LO phase accumulator are left intact, avoiding the transient that a full reset would cause._ 
```C++
void ddc_set_norm_freq (
    ddc_state_t * state,
    double val
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `val` New normalised frequency (cycles/sample at input rate).


```C++
>>> from doppler.ddc import DDC
>>> ddc = DDC(norm_freq=-0.1, rate=0.25)
>>> ddc.norm_freq = -0.2
>>> ddc.norm_freq
-0.2
```
 


        

<hr>



### function ddc\_set\_state 

_Restore LO + RateConverter state from_ `blob` _._
```C++
int ddc_set_state (
    ddc_state_t * state,
    const void * blob
) 
```





**Returns:**

DP\_OK, or DP\_ERR\_INVALID if the envelope/rate rejects. 





        

<hr>



### function ddc\_set\_telemetry 

_Attach (or detach) a telemetry context on the cascade's AGC._ 
```C++
int ddc_set_telemetry (
    ddc_state_t * state,
    dp_tlm_t * tlm,
    const char * prefix,
    uint32_t decim
) 
```



Forwarded verbatim to [**RateConverter\_set\_telemetry()**](RateConverter__core_8h.md#function-rateconverter_set_telemetry): the mixer and the fixed stages have no loop to report, so the one instrumented child is the cascade's pre-terminal AGC ("&lt;prefix&gt;.gain\_db" and "&lt;prefix&gt;.level\_db"). DP\_OK with no probes when the cascade has no AGC enabled. Setup path, never hot; the context is borrowed and must outlive the attachment.




**Parameters:**


* `state` Must be non-NULL. 
* `tlm` Telemetry context to attach, or NULL to detach. 
* `prefix` Probe-name prefix, e.g. "rx.agc". 
* `decim` Emit every decim-th gain update; &gt;= 1. 



**Returns:**

DP\_OK, or DP\_ERR\_INVALID when the probe table cannot take the AGC's probes (the attach fails whole). 





        

<hr>



### function ddc\_state\_bytes 

_Byte size of_ `state's` _blob (envelope + extra + lo + rc)._
```C++
size_t ddc_state_bytes (
    const ddc_state_t * state
) 
```




<hr>
## Macro Definition Documentation





### define DDC\_STATE\_MAGIC 

```C++
#define DDC_STATE_MAGIC `DP_FOURCC ('D', 'D', 'C', '_')`
```




<hr>



### define DDC\_STATE\_VERSION 

```C++
#define DDC_STATE_VERSION `1u`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/ddc/ddc_core.h`

