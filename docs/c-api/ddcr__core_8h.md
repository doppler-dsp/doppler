

# File ddcr\_core.h



[**FileList**](files.md) **>** [**ddcr**](dir_46c04c942eb84c8716610cebe515b046.md) **>** [**ddcr\_core.h**](ddcr__core_8h.md)

[Go to the source code of this file](ddcr__core_8h_source.md)

_Real-input Digital Down-Converter — halfband R2C + LO + cascade._ [More...](#detailed-description)

* `#include <complex.h>`
* `#include <stdbool.h>`
* `#include <stddef.h>`
* `#include "lo/lo_core.h"`
* `#include "RateConverter/RateConverter_core.h"`
* `#include "resamp/resamp_core.h"`
* `#include "hbdecim/hbdecim_core.h"`
* `#include "hbdecim/hbdecim_r2c_core.h"`
* `#include "cic/cic_core.h"`
* `#include "fir/fir_core.h"`
* `#include "resample/resample_core.h"`
* `#include "agc/agc_core.h"`
* `#include "dp_tlm/dp_tlm_core.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**ddcr\_extra\_t**](structddcr__extra__t.md) <br> |
| struct | [**ddcr\_state**](structddcr__state.md) <br>_DdcR state — the real-to-complex front end, an LO and a cascade._  |


## Public Types

| Type | Name |
| ---: | :--- |
| typedef struct [**ddcr\_state**](structddcr__state.md) | [**ddcr\_state\_t**](#typedef-ddcr_state_t)  <br>_DdcR state — the real-to-complex front end, an LO and a cascade._  |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**ddcr\_state\_t**](ddcr__core_8h.md#typedef-ddcr_state_t) \* | [**ddcr\_create**](#function-ddcr_create) (double norm\_freq, double rate) <br>_Create a real-input Digital Down-Converter (Architecture D2). The signal chain is: halfband R2C (2:1, bakes in +fs/4 shift) -&gt; fine LO mix at the intermediate rate (fs\_in/2) -&gt; RateConverter -&gt; CF32 output. The halfband stage uses +-1/0 coefficients (no multiplications) and puts the fine LO and the cascade at fs\_in/2. That is worth ~1.1-1.7x in a whole receiver (it halves the rate ahead of the polyphase matched filter, so the gain grows with samples/symbol) and close to nothing for the front end alone_  _see the file header for the measurements. Use it because the input IS real._ |
|  [**ddcr\_state\_t**](ddcr__core_8h.md#typedef-ddcr_state_t) \* | [**ddcr\_create\_matched**](#function-ddcr_create_matched) (double norm\_freq, double rate, int pulse, double beta, size\_t span, double pulse\_sps, size\_t num\_phases) <br>_Create a real-input DDC whose terminal stage IS a matched filter._  |
|  void | [**ddcr\_destroy**](#function-ddcr_destroy) ([**ddcr\_state\_t**](ddcr__core_8h.md#typedef-ddcr_state_t) \* s) <br>_Free all resources held by a DDCR instance. Releases the halfband, RateConverter, and LO substructures, then the struct itself. Passing NULL is a no-op._  |
|  size\_t | [**ddcr\_execute**](#function-ddcr_execute) ([**ddcr\_state\_t**](ddcr__core_8h.md#typedef-ddcr_state_t) \* s, const float \* in, size\_t n\_in, float \_Complex \* out, size\_t max\_out) <br>_Process a block of real float32 samples through the full DDCR signal chain: halfband R2C → LO mix → RateConverter → CF32. The halfband decimates by 2 and applies a built-in +fs/4 frequency shift; the fine NCO then completes the tuning. State is maintained across calls for contiguous streaming. Output length ≈ n\_in \* rate (±1 from polyphase indexing). A real tone at input normalised frequency f\_c has amplitude 0.5 in the baseband output (one-sided spectrum), consistent with analytic signal theory._  |
|  size\_t | [**ddcr\_execute\_ctrl**](#function-ddcr_execute_ctrl) ([**ddcr\_state\_t**](ddcr__core_8h.md#typedef-ddcr_state_t) \* s, const float \* x, size\_t n\_in, double rate\_ctrl, double freq\_ctrl, float \_Complex \* out, size\_t max\_out) <br>_Process a real block, steering both control ports._  |
|  size\_t | [**ddcr\_execute\_ctrl\_max\_out**](#function-ddcr_execute_ctrl_max_out) ([**ddcr\_state\_t**](ddcr__core_8h.md#typedef-ddcr_state_t) \* s) <br>_As_ [_**ddcr\_execute\_max\_out()**_](ddcr__core_8h.md#function-ddcr_execute_max_out) _, for the block control-port form._ |
|  size\_t | [**ddcr\_execute\_ctrl\_push**](#function-ddcr_execute_ctrl_push) ([**ddcr\_state\_t**](ddcr__core_8h.md#typedef-ddcr_state_t) \* s, float x, double rate\_ctrl, double freq\_ctrl, float \_Complex \* out, size\_t max\_out) <br>_Push ONE real input sample; emit whatever outputs it completes._  |
|  size\_t | [**ddcr\_execute\_ctrl\_push\_max\_out**](#function-ddcr_execute_ctrl_push_max_out) ([**ddcr\_state\_t**](ddcr__core_8h.md#typedef-ddcr_state_t) \* s) <br>_Bound for ONE pushed input:_ `ceil(rate) + 1` _output periods. Non-zero because the push form has no input block to size from._ |
|  size\_t | [**ddcr\_execute\_ctrl\_push\_tap**](#function-ddcr_execute_ctrl_push_tap) ([**ddcr\_state\_t**](ddcr__core_8h.md#typedef-ddcr_state_t) \* s, float x, double rate\_ctrl, double freq\_ctrl, float \_Complex \* out, size\_t max\_out, float \_Complex \* lo\_out, int \* n\_lo) <br>[_**ddcr\_execute\_ctrl\_push()**_](ddcr__core_8h.md#function-ddcr_execute_ctrl_push) _that also hands back the post-LO sample._ |
|  size\_t | [**ddcr\_execute\_ctrl\_push\_tap2**](#function-ddcr_execute_ctrl_push_tap2) ([**ddcr\_state\_t**](ddcr__core_8h.md#typedef-ddcr_state_t) \* s, float x, double rate\_ctrl, double freq\_ctrl, float \_Complex \* out, size\_t max\_out, float \_Complex \* lo\_out, int \* n\_lo, float \_Complex \* pre\_out, int \* n\_pre) <br>[_**ddcr\_execute\_ctrl\_push\_tap()**_](ddcr__core_8h.md#function-ddcr_execute_ctrl_push_tap) _, plus the MFR-INPUT tap._ |
|  size\_t | [**ddcr\_execute\_max\_out**](#function-ddcr_execute_max_out) ([**ddcr\_state\_t**](ddcr__core_8h.md#typedef-ddcr_state_t) \* s) <br>_Upper bound on one execute call's output, or 0 to let the caller size it from the input block (a decimator never exceeds its input)._  |
|  double | [**ddcr\_get\_bank\_sps**](#function-ddcr_get_bank_sps) (const [**ddcr\_state\_t**](ddcr__core_8h.md#typedef-ddcr_state_t) \* s) <br>_Samples per symbol of the MFR-input tap; a planner outcome. Identical to the complex twin's at every rate ratio —_ `bank_sps` _is symbol-relative, so the halfband's 2:1 is absorbed by the plan._ |
|  bool | [**ddcr\_get\_clipped**](#function-ddcr_get_clipped) (const [**ddcr\_state\_t**](ddcr__core_8h.md#typedef-ddcr_state_t) \* s) <br>_Has the cascade's CIC clipped its input since the last reset?_  |
|  bool | [**ddcr\_get\_narrow\_pulse**](#function-ddcr_get_narrow_pulse) (const [**ddcr\_state\_t**](ddcr__core_8h.md#typedef-ddcr_state_t) \* s) <br>_Is this object's rectangular matched filter degenerately narrow?_  |
|  double | [**ddcr\_get\_norm\_freq**](#function-ddcr_get_norm_freq) (const [**ddcr\_state\_t**](ddcr__core_8h.md#typedef-ddcr_state_t) \* s) <br>_Return the current fine NCO normalised frequency at the intermediate rate (fs\_in/2, cycles/sample)._  |
|  double | [**ddcr\_get\_rate**](#function-ddcr_get_rate) (const [**ddcr\_state\_t**](ddcr__core_8h.md#typedef-ddcr_state_t) \* s) <br>_Return the total configured rate (fs\_out / fs\_in, read-only). This is the end-to-end ratio from ADC input to CF32 output. Change it by destroying and recreating the DDCR._  |
|  void | [**ddcr\_get\_state**](#function-ddcr_get_state) (const [**ddcr\_state\_t**](ddcr__core_8h.md#typedef-ddcr_state_t) \* s, void \* blob) <br>_Serialize_ `s's` _full-chain state into_`blob` _._ |
|  void | [**ddcr\_reset**](#function-ddcr_reset) ([**ddcr\_state\_t**](ddcr__core_8h.md#typedef-ddcr_state_t) \* s) <br>_Zero halfband filter history, LO phase, and resampler history. After reset, the next execute call reproduces the output of the first call after create, enabling repeatable block-by-block tests._  |
|  size\_t | [**ddcr\_run**](#function-ddcr_run) ([**ddcr\_state\_t**](ddcr__core_8h.md#typedef-ddcr_state_t) \* s, const void \* state\_in, void \* state\_out, const float \* in, size\_t n\_in, float \_Complex \* out, size\_t max\_out) <br>_Pure run: inject_ `state_in` _, process_`in` _, export_`state_out` _—_`(state_in, input) -> (state_out, output)` _over an engine treated as immutable config. Either state may be NULL (NULL in = use current; NULL out = discard)._`state_in` _/_`state_out` _may alias._ |
|  void | [**ddcr\_set\_norm\_freq**](#function-ddcr_set_norm_freq) ([**ddcr\_state\_t**](ddcr__core_8h.md#typedef-ddcr_state_t) \* s, double norm\_freq) <br>_Retune the fine NCO without resetting halfband or resampler history. Updates the LO phase increment only; state is preserved for seamless tuning across block boundaries._  |
|  int | [**ddcr\_set\_state**](#function-ddcr_set_state) ([**ddcr\_state\_t**](ddcr__core_8h.md#typedef-ddcr_state_t) \* s, const void \* blob) <br>_Restore full-chain state from_ `blob` _into_`s` _._ |
|  int | [**ddcr\_set\_telemetry**](#function-ddcr_set_telemetry) ([**ddcr\_state\_t**](ddcr__core_8h.md#typedef-ddcr_state_t) \* s, [**dp\_tlm\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_t) \* tlm, const char \* prefix, uint32\_t decim) <br>_Attach (or detach) a telemetry context on the cascade's AGC._  |
|  size\_t | [**ddcr\_state\_bytes**](#function-ddcr_state_bytes) (const [**ddcr\_state\_t**](ddcr__core_8h.md#typedef-ddcr_state_t) \* s) <br>_Byte size of_ `s's` _state blob (envelope + extra + chain)._ |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**DDCR\_STATE\_MAGIC**](ddcr__core_8h.md#define-ddcr_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('D', 'D', 'C', 'R')`<br> |
| define  | [**DDCR\_STATE\_VERSION**](ddcr__core_8h.md#define-ddcr_state_version)  `1u`<br> |

## Detailed Description


The real-input twin of [**ddc/ddc\_core.h**](ddc__core_8h.md)'s Ddc: identical from the LO onwards, behind a real-to-complex front end.



```C++
float in (fs_in)  →  halfband R2C (2:1, embedded fs/4 shift)
                  →  LO mix at intermediate rate (fs_in/2)
                  →  RateConverter  →  CF32 out (fs_out)
```



norm\_freq: Fine NCO frequency at the INTERMEDIATE rate (fs\_in/2). To tune a real tone at f\_carrier (input normalised) to DC: set norm\_freq = -(2\*f\_carrier + 0.5). Total output rate: fs\_out = rate \* fs\_in (rate &lt; 0.5).


The halfband R2C step has an fs/4 frequency shift baked in at zero extra multiplications — the +/-1/0 coefficients multiply for free — and everything after it (the fine LO and the whole cascade) runs at fs\_in/2.


What that is worth, measured rather than assumed: for the FRONT END alone, against Ddc fed the same stream promoted to complex, essentially nothing — 1.04x to 1.40x end to end at total rates 0.25/0.125/0.0625, and 0.74x to 1.13x once the real-&gt;complex promote is charged to Ddc, with the ratio wandering by block size the way a memory-bound measurement does. The free coefficients are real; multiplies are simply not what this path pays for.


Where the half rate DOES pay is a whole receiver, because it halves the sample rate ahead of the polyphase matched filter: MpskReceiverR against MpskReceiver on the same stream measures 1.13x at sps=20/m\_out=8, 1.50x at sps=32/m\_out=8 and 1.69x at sps=64/m\_out=8. It rises toward 2x with sps (more of the total cost is then pre-MF) but cannot reach it, since both paths fire the same m\_out terminal dot products per symbol and those dominate at low sps. Choose DdcR because your input IS real, not for a factor of two.


Like Ddc it has a matched _flavor_ (ddcr\_create\_matched, Python `MatchedDdcr`) that puts the pulse on the cascade's terminal stage, and the same two control ports — see [**ddc/ddc\_core.h**](ddc__core_8h.md)'s file header for what the ports are and why they are duals.



```C++
// Tune a real tone at +0.1*fs to DC, decimate by 4
// norm_freq at intermediate rate: -(2 * 0.1 + 0.5) = -0.7
ddcr_state_t *ddcr = ddcr_create(-0.7, 0.25);
float _Complex out[4096];
size_t m = ddcr_execute(ddcr, real_in, 1024, out, 4096);
ddcr_destroy(ddcr);
```
 


    
## Public Types Documentation




### typedef ddcr\_state\_t 

_DdcR state — the real-to-complex front end, an LO and a cascade._ 
```C++
typedef struct ddcr_state ddcr_state_t;
```



Do not initialise directly; use [**ddcr\_create()**](ddcr__core_8h.md#function-ddcr_create) or [**ddcr\_create\_matched()**](ddcr__core_8h.md#function-ddcr_create_matched). 


        

<hr>
## Public Functions Documentation




### function ddcr\_create 

_Create a real-input Digital Down-Converter (Architecture D2). The signal chain is: halfband R2C (2:1, bakes in +fs/4 shift) -&gt; fine LO mix at the intermediate rate (fs\_in/2) -&gt; RateConverter -&gt; CF32 output. The halfband stage uses +-1/0 coefficients (no multiplications) and puts the fine LO and the cascade at fs\_in/2. That is worth ~1.1-1.7x in a whole receiver (it halves the rate ahead of the polyphase matched filter, so the gain grows with samples/symbol) and close to nothing for the front end alone_  _see the file header for the measurements. Use it because the input IS real._
```C++
ddcr_state_t * ddcr_create (
    double norm_freq,
    double rate
) 
```





**Parameters:**


* `norm_freq` Fine NCO frequency at the intermediate rate (fs\_in/2, cycles/sample). To tune a real tone at normalised input frequency f\_c to DC, set norm\_freq = -(2\*f\_c + 0.5). 
* `rate` Total output/input rate. Must be in (0, 0.5) because the halfband pre-decimates by 2. 



**Returns:**

Non-NULL on success, NULL on OOM or invalid args.



```C++
>>> from doppler.ddc import Ddcr
>>> ddcr = Ddcr(norm_freq=-0.7, rate=0.25)
>>> ddcr.norm_freq
-0.7
>>> ddcr.rate
0.25
```
 


        

<hr>



### function ddcr\_create\_matched 

_Create a real-input DDC whose terminal stage IS a matched filter._ 
```C++
ddcr_state_t * ddcr_create_matched (
    double norm_freq,
    double rate,
    int pulse,
    double beta,
    size_t span,
    double pulse_sps,
    size_t num_phases
) 
```



The matched flavor of DdcR (Python: `MatchedDdcr`), and identical to [**ddc\_create\_matched()**](ddc__core_8h.md#function-ddc_create_matched) from the LO onwards — the halfband R2C front end is a fixed 2:1 integer stage, so the pulse still lands on the cascade's terminal stage and both control ports mean exactly what they mean there.


Note the rate arithmetic the halfband imposes: the cascade behind it runs at `2*rate`, and this function does that on the caller's behalf, so a caller wanting `m` outputs per symbol still passes the TOTAL `rate = m/sps`. `pulse_sps` is in **output** samples, so the front end does not affect it.




**Parameters:**


* `norm_freq` Fine NCO frequency at the INTERMEDIATE rate (fs\_in/2) — the same reference [**ddcr\_create()**](ddcr__core_8h.md#function-ddcr_create) uses. 
* `rate` Total output/input rate; must be in (0, 0.5). 
* `pulse` RC\_PULSE\_RRC / RC\_PULSE\_IANDD (RC\_PULSE\_NONE is invalid here — use [**ddcr\_create()**](ddcr__core_8h.md#function-ddcr_create)). 
* `beta` RRC roll-off in `[0, 1]` (ignored for the rectangle). 
* `span` One-sided RRC span in symbols (ignored for the rectangle). 
* `pulse_sps` The pulse's period in **output** samples. 
* `num_phases` Terminal-stage arms; a power of two. 



**Returns:**

Non-NULL on success, NULL on a bad parameter or OOM.



```C++
>>> from doppler.ddc import MatchedDdcr
>>> rx = MatchedDdcr(norm_freq=-0.6875, rate=2 / 16, pulse="rrc")
>>> rx.rate
0.125
```
 


        

<hr>



### function ddcr\_destroy 

_Free all resources held by a DDCR instance. Releases the halfband, RateConverter, and LO substructures, then the struct itself. Passing NULL is a no-op._ 
```C++
void ddcr_destroy (
    ddcr_state_t * s
) 
```




```C++
>>> from doppler.ddc import Ddcr
>>> ddcr = Ddcr(norm_freq=0.0, rate=0.25)
>>> ddcr.close()   # releases C memory immediately
```
 


        

<hr>



### function ddcr\_execute 

_Process a block of real float32 samples through the full DDCR signal chain: halfband R2C → LO mix → RateConverter → CF32. The halfband decimates by 2 and applies a built-in +fs/4 frequency shift; the fine NCO then completes the tuning. State is maintained across calls for contiguous streaming. Output length ≈ n\_in \* rate (±1 from polyphase indexing). A real tone at input normalised frequency f\_c has amplitude 0.5 in the baseband output (one-sided spectrum), consistent with analytic signal theory._ 
```C++
size_t ddcr_execute (
    ddcr_state_t * s,
    const float * in,
    size_t n_in,
    float _Complex * out,
    size_t max_out
) 
```





**Parameters:**


* `s` Must be non-NULL. 
* `in` Real float32 input block. 
* `n_in` Number of input samples (C-only, hidden from Python). 
* `out` CF32 output buffer (C-only, hidden from Python). 
* `max_out` Output buffer capacity (C-only, hidden from Python). 



**Returns:**

Number of output samples written (C-only).



```C++
>>> from doppler.ddc import Ddcr
>>> import numpy as np
>>> ddcr = Ddcr(norm_freq=-0.7, rate=0.25)
>>> t = np.arange(4096)
>>> x = np.cos(2 * np.pi * 0.1 * t).astype(np.float32)
>>> out = np.empty(len(x), dtype=np.complex64)
>>> y = ddcr.execute(x, out)
>>> y.shape
(1024,)
>>> y.dtype
dtype('complex64')
>>> round(float(abs(y[500])), 2)   # analytic signal of a unit cosine
1.0
```
 


        

<hr>



### function ddcr\_execute\_ctrl 

_Process a real block, steering both control ports._ 
```C++
size_t ddcr_execute_ctrl (
    ddcr_state_t * s,
    const float * x,
    size_t n_in,
    double rate_ctrl,
    double freq_ctrl,
    float _Complex * out,
    size_t max_out
) 
```



The control-port form of [**ddcr\_execute()**](ddcr__core_8h.md#function-ddcr_execute); see [**ddc\_execute\_ctrl()**](ddc__core_8h.md#function-ddc_execute_ctrl) for the semantics, which are identical except for where the LO lives.




**Parameters:**


* `s` Must be non-NULL. 
* `x` Real float32 input block. 
* `n_in` Number of input samples. 
* `rate_ctrl` Rate deviation added to the terminal Resampler stage's rate (referenced to the terminal, post-decimation rate). 
* `freq_ctrl` Frequency deviation added to the fine LO, in cycles/sample at the INTERMEDIATE rate (fs\_in/2) — the halfband has already decimated by two by the time the mix happens, so a discriminator working in cycles per ADC sample must be doubled before it lands here. 
* `out` CF32 output buffer. 
* `max_out` Capacity of `out` in samples. 



**Returns:**

Number of output samples written.



```C++
>>> from doppler.ddc import Ddcr
>>> import numpy as np
>>> ddcr = Ddcr(norm_freq=-0.5, rate=0.25)  # LO 0.2 short of tune
>>> t = np.arange(4096)
>>> x = np.cos(2 * np.pi * 0.1 * t).astype(np.float32)
>>> y = ddcr.execute_ctrl(x, 0.0, -0.2)     # ctrl completes the tune
>>> y.shape
(1024,)
>>> round(float(abs(y[100:].mean())), 2)    # real tone -> DC, amp 1.0
1.0
```
 


        

<hr>



### function ddcr\_execute\_ctrl\_max\_out 

_As_ [_**ddcr\_execute\_max\_out()**_](ddcr__core_8h.md#function-ddcr_execute_max_out) _, for the block control-port form._
```C++
size_t ddcr_execute_ctrl_max_out (
    ddcr_state_t * s
) 
```




<hr>



### function ddcr\_execute\_ctrl\_push 

_Push ONE real input sample; emit whatever outputs it completes._ 
```C++
size_t ddcr_execute_ctrl_push (
    ddcr_state_t * s,
    float x,
    double rate_ctrl,
    double freq_ctrl,
    float _Complex * out,
    size_t max_out
) 
```



The per-input streaming form of [**ddcr\_execute\_ctrl()**](ddcr__core_8h.md#function-ddcr_execute_ctrl), for a closed loop. The halfband consumes two inputs per intermediate sample, so every other push does no mixing and emits nothing at all — the LO advances (and its control is applied) once per _intermediate_ sample, which is the rate the LO runs at.




**Parameters:**


* `s` Must be non-NULL. 
* `x` One real float32 input sample. 
* `rate_ctrl` Rate deviation for this input (terminal-stage rate). 
* `freq_ctrl` Frequency deviation, cycles/sample at fs\_in/2. 
* `out` Output buffer for any emitted samples. 
* `max_out` Capacity of `out`. 



**Returns:**

Number of outputs written (0, 1, or more).



```C++
>>> from doppler.ddc import Ddcr
>>> import numpy as np
>>> ddcr = Ddcr(norm_freq=-0.7, rate=0.25)
>>> x = np.cos(2 * np.pi * 0.1 * np.arange(128)).astype(np.float32)
>>> outs = [ddcr.execute_ctrl_push(float(s), 0.0, 0.0) for s in x]
>>> int(sum(len(o) for o in outs))  # 128 real inputs, rate 1/4 -> 32
32
>>> [len(o) for o in outs[:4]]      # halfband: 0 until a strobe
[0, 0, 0, 1]
```
 


        

<hr>



### function ddcr\_execute\_ctrl\_push\_max\_out 

_Bound for ONE pushed input:_ `ceil(rate) + 1` _output periods. Non-zero because the push form has no input block to size from._
```C++
size_t ddcr_execute_ctrl_push_max_out (
    ddcr_state_t * s
) 
```




<hr>



### function ddcr\_execute\_ctrl\_push\_tap 

[_**ddcr\_execute\_ctrl\_push()**_](ddcr__core_8h.md#function-ddcr_execute_ctrl_push) _that also hands back the post-LO sample._
```C++
size_t ddcr_execute_ctrl_push_tap (
    ddcr_state_t * s,
    float x,
    double rate_ctrl,
    double freq_ctrl,
    float _Complex * out,
    size_t max_out,
    float _Complex * lo_out,
    int * n_lo
) 
```



The real-input twin of [**ddc\_execute\_ctrl\_push\_tap()**](ddc__core_8h.md#function-ddc_execute_ctrl_push_tap); see that function for why the tap exists (a carrier discriminator's unambiguous range is set by the rate it updates at, so a caller may want the widest, least-filtered stream rather than the cleanest one).


The one difference is that this front end does NOT mix every input: the 2:1 halfband consumes two real inputs per intermediate sample, so `n_lo` comes back 0 on every other push and `lo_out` is untouched. The tapped stream therefore runs at `fs_in/2`, the LO's own rate — half as fast as the complex twin's for the same nominal `sps`, which halves this tap's frequency range in input-referred terms exactly as it halves everything else the LO sees.




**Parameters:**


* `s` Must be non-NULL. 
* `x` One real float32 input sample. 
* `rate_ctrl` Rate deviation for this input (terminal-stage rate). 
* `freq_ctrl` Frequency deviation, cycles/sample at fs\_in/2. 
* `out` Output buffer for any emitted outputs. 
* `max_out` Capacity of `out`. 
* `lo_out` Receives the post-LO, pre-cascade sample when `n_lo` comes back 1. May be NULL. 
* `n_lo` Receives 1 when the halfband fired for this input and the LO stepped, 0 otherwise. May be NULL. 



**Returns:**

Number of terminal outputs written (0, 1, or more). 





        

<hr>



### function ddcr\_execute\_ctrl\_push\_tap2 

[_**ddcr\_execute\_ctrl\_push\_tap()**_](ddcr__core_8h.md#function-ddcr_execute_ctrl_push_tap) _, plus the MFR-INPUT tap._
```C++
size_t ddcr_execute_ctrl_push_tap2 (
    ddcr_state_t * s,
    float x,
    double rate_ctrl,
    double freq_ctrl,
    float _Complex * out,
    size_t max_out,
    float _Complex * lo_out,
    int * n_lo,
    float _Complex * pre_out,
    int * n_pre
) 
```



The real-input twin of [**ddc\_execute\_ctrl\_push\_tap2()**](ddc__core_8h.md#function-ddc_execute_ctrl_push_tap2). `pre_out` receives the cascade's output after every integer stage and after the AGC but ahead of the terminal matched filter — the node an NDA carrier discriminator can read with no symbol timing. Its rate is [**ddcr\_get\_bank\_sps()**](ddcr__core_8h.md#function-ddcr_get_bank_sps) samples per symbol.


The halfband gates the whole call: on the inputs it swallows there is no LO step and no cascade push, so `n_lo` and `n_pre` both come back 0.




**Parameters:**


* `s` Must be non-NULL. 
* `x` One real input sample. 
* `rate_ctrl` Rate deviation for this input (terminal-stage rate). 
* `freq_ctrl` Frequency deviation, cycles/sample at the LO's own (halved) intermediate rate. 
* `out` Output buffer for any emitted outputs. 
* `max_out` Capacity of `out`. 
* `lo_out` Receives the post-LO, pre-cascade sample when `n_lo` comes back 1. May be NULL. 
* `n_lo` Receives 1 when the halfband emitted and the LO stepped, else 0. May be NULL. 
* `pre_out` Receives the MFR-input sample; may be NULL. 
* `n_pre` Receives 1 if `pre_out` was written, else 0; may be NULL. 



**Returns:**

Number of terminal outputs written. 





        

<hr>



### function ddcr\_execute\_max\_out 

_Upper bound on one execute call's output, or 0 to let the caller size it from the input block (a decimator never exceeds its input)._ 
```C++
size_t ddcr_execute_max_out (
    ddcr_state_t * s
) 
```




<hr>



### function ddcr\_get\_bank\_sps 

_Samples per symbol of the MFR-input tap; a planner outcome. Identical to the complex twin's at every rate ratio —_ `bank_sps` _is symbol-relative, so the halfband's 2:1 is absorbed by the plan._
```C++
double ddcr_get_bank_sps (
    const ddcr_state_t * s
) 
```




<hr>



### function ddcr\_get\_clipped 

_Has the cascade's CIC clipped its input since the last reset?_ 
```C++
bool ddcr_get_clipped (
    const ddcr_state_t * s
) 
```



Forwarded from [**RateConverter\_get\_clipped()**](RateConverter__core_8h.md#function-rateconverter_get_clipped); see [**ddc\_get\_clipped()**](ddc__core_8h.md#function-ddc_get_clipped). The halfband R2C front end has unity passband gain and a real tone lands at amplitude 0.5 in the analytic output, so a full-scale ADC stream sits comfortably inside the CIC's bound — but a scaled-up input does not. 


        

<hr>



### function ddcr\_get\_narrow\_pulse 

_Is this object's rectangular matched filter degenerately narrow?_ 
```C++
bool ddcr_get_narrow_pulse (
    const ddcr_state_t * s
) 
```



The real chain's copy of [**ddc\_get\_narrow\_pulse()**](ddc__core_8h.md#function-ddc_get_narrow_pulse): true only for the matched flavor with `pulse = RC_PULSE_IANDD` and fewer than four output samples per symbol, where the one-symbol-wide rectangle's matched filter is a 2-3 tap sum. Construction also raises a UserWarning. 


        

<hr>



### function ddcr\_get\_norm\_freq 

_Return the current fine NCO normalised frequency at the intermediate rate (fs\_in/2, cycles/sample)._ 
```C++
double ddcr_get_norm_freq (
    const ddcr_state_t * s
) 
```




```C++
>>> from doppler.ddc import Ddcr
>>> ddcr = Ddcr(norm_freq=-0.7, rate=0.25)
>>> ddcr.norm_freq
-0.7
```
 


        

<hr>



### function ddcr\_get\_rate 

_Return the total configured rate (fs\_out / fs\_in, read-only). This is the end-to-end ratio from ADC input to CF32 output. Change it by destroying and recreating the DDCR._ 
```C++
double ddcr_get_rate (
    const ddcr_state_t * s
) 
```




```C++
>>> from doppler.ddc import Ddcr
>>> ddcr = Ddcr(norm_freq=0.0, rate=0.25)
>>> ddcr.rate
0.25
```
 


        

<hr>



### function ddcr\_get\_state 

_Serialize_ `s's` _full-chain state into_`blob` _._
```C++
void ddcr_get_state (
    const ddcr_state_t * s,
    void * blob
) 
```




<hr>



### function ddcr\_reset 

_Zero halfband filter history, LO phase, and resampler history. After reset, the next execute call reproduces the output of the first call after create, enabling repeatable block-by-block tests._ 
```C++
void ddcr_reset (
    ddcr_state_t * s
) 
```




```C++
>>> from doppler.ddc import Ddcr
>>> import numpy as np
>>> ddcr = Ddcr(norm_freq=0.0, rate=0.25)
>>> x = np.ones(64, dtype=np.float32)
>>> out = np.empty(64, dtype=np.complex64)
>>> y1 = ddcr.execute(x, out).copy()
>>> ddcr.reset()
>>> y2 = ddcr.execute(x, out)
>>> bool(np.array_equal(y1, y2))
True
```
 


        

<hr>



### function ddcr\_run 

_Pure run: inject_ `state_in` _, process_`in` _, export_`state_out` _—_`(state_in, input) -> (state_out, output)` _over an engine treated as immutable config. Either state may be NULL (NULL in = use current; NULL out = discard)._`state_in` _/_`state_out` _may alias._
```C++
size_t ddcr_run (
    ddcr_state_t * s,
    const void * state_in,
    void * state_out,
    const float * in,
    size_t n_in,
    float _Complex * out,
    size_t max_out
) 
```





**Returns:**

Number of CF32 output samples written. 





        

<hr>



### function ddcr\_set\_norm\_freq 

_Retune the fine NCO without resetting halfband or resampler history. Updates the LO phase increment only; state is preserved for seamless tuning across block boundaries._ 
```C++
void ddcr_set_norm_freq (
    ddcr_state_t * s,
    double norm_freq
) 
```





**Parameters:**


* `s` Must be non-NULL. 
* `norm_freq` New frequency at the intermediate rate (fs\_in/2).


```C++
>>> from doppler.ddc import Ddcr
>>> ddcr = Ddcr(norm_freq=-0.7, rate=0.25)
>>> ddcr.norm_freq = -0.5
>>> ddcr.norm_freq
-0.5
```
 


        

<hr>



### function ddcr\_set\_state 

_Restore full-chain state from_ `blob` _into_`s` _._
```C++
int ddcr_set_state (
    ddcr_state_t * s,
    const void * blob
) 
```





**Returns:**

DP\_OK, or DP\_ERR\_INVALID if the envelope/rate disagree with `s` (rebuild the engine from the matching descriptor first). 





        

<hr>



### function ddcr\_set\_telemetry 

_Attach (or detach) a telemetry context on the cascade's AGC._ 
```C++
int ddcr_set_telemetry (
    ddcr_state_t * s,
    dp_tlm_t * tlm,
    const char * prefix,
    uint32_t decim
) 
```



The twin of [**ddc\_set\_telemetry()**](ddc__core_8h.md#function-ddc_set_telemetry), forwarded to the same [**RateConverter\_set\_telemetry()**](RateConverter__core_8h.md#function-rateconverter_set_telemetry) over the same cascade: the R2C front end and the fixed stages have no loop to report, so the one instrumented child is the pre-terminal AGC ("&lt;prefix&gt;.gain\_db" and "&lt;prefix&gt;.level\_db"). DP\_OK with no probes when the cascade has no AGC enabled.




**Parameters:**


* `s` Must be non-NULL. 
* `tlm` Telemetry context to attach, or NULL to detach. 
* `prefix` Probe-name prefix, e.g. "rx.agc". 
* `decim` Emit every decim-th gain update; &gt;= 1. 



**Returns:**

DP\_OK, or DP\_ERR\_INVALID when the probe table cannot take the AGC's probes (the attach fails whole). 





        

<hr>



### function ddcr\_state\_bytes 

_Byte size of_ `s's` _state blob (envelope + extra + chain)._
```C++
size_t ddcr_state_bytes (
    const ddcr_state_t * s
) 
```




<hr>
## Macro Definition Documentation





### define DDCR\_STATE\_MAGIC 

```C++
#define DDCR_STATE_MAGIC `DP_FOURCC ('D', 'D', 'C', 'R')`
```




<hr>



### define DDCR\_STATE\_VERSION 

```C++
#define DDCR_STATE_VERSION `1u`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/ddcr/ddcr_core.h`

