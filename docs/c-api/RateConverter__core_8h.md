

# File RateConverter\_core.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**RateConverter**](dir_ab9e07a54a3e9554c466f24859c37292.md) **>** [**RateConverter\_core.h**](RateConverter__core_8h.md)

[Go to the source code of this file](RateConverter__core_8h_source.md)

_Optimal-speed rate conversion cascade._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "dp_state.h"`
* `#include <complex.h>`
* `#include <stdbool.h>`
* `#include <stddef.h>`
* `#include "resamp/resamp_core.h"`
* `#include "fir/fir_core.h"`
* `#include "agc/agc_core.h"`
* `#include "dp_tlm/dp_tlm_core.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**RateConverter\_state\_t**](structRateConverter__state__t.md) <br>_Cascade state_  _owns all sub-stage C objects._ |


## Public Types

| Type | Name |
| ---: | :--- |
| enum  | [**rc\_pulse\_t**](#enum-rc_pulse_t)  <br>_Matched-filter pulse selection for the terminal stage._  |
| enum  | [**rc\_stage\_t**](#enum-rc_stage_t)  <br> |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  double | [**RateConverter\_agc\_gain\_db**](#function-rateconverter_agc_gain_db) (const [**RateConverter\_state\_t**](structRateConverter__state__t.md) \* s) <br>_Gain the pre-terminal AGC last applied, in dB; 0.0 when off._  |
|  double | [**RateConverter\_agc\_ref\_db**](#function-rateconverter_agc_ref_db) (const [**RateConverter\_state\_t**](structRateConverter__state__t.md) \* s) <br>_The pre-terminal AGC's reference level, in dB._  |
|  size\_t | [**RateConverter\_bank\_shape\_value**](#function-rateconverter_bank_shape_value) (const [**RateConverter\_state\_t**](structRateConverter__state__t.md) \* s, size\_t i) <br>_Element_ `i` _of the bank shape: 0 -&gt; num\_phases, 1 -&gt; num\_taps._ |
|  size\_t | [**RateConverter\_convert**](#function-rateconverter_convert) (double rate, int compensate, const float \_Complex \* in, size\_t n\_in, float \_Complex \* out, size\_t max\_out) <br>_One-shot rate conversion — no persistent state required._  |
|  [**RateConverter\_state\_t**](structRateConverter__state__t.md) \* | [**RateConverter\_create**](#function-rateconverter_create) (double rate, int compensate) <br>_Create a rate converter for the given output/input rate ratio. Selects the cheapest cascade of CIC, HalfbandDecimator, and/or polyphase Resampler stages at construction time (see file header for the selection table). Setting compensate=1 appends a closed-form Molnar-Vucic CIC droop-compensating FIR after any CIC stage, which improves passband flatness at the cost of one extra FIR stage._  |
|  [**RateConverter\_state\_t**](structRateConverter__state__t.md) \* | [**RateConverter\_create\_matched**](#function-rateconverter_create_matched) (double rate, int compensate, int pulse, double beta, size\_t span, double pulse\_sps, size\_t num\_phases) <br>_Create a rate converter whose terminal stage IS a matched filter._  |
|  void | [**RateConverter\_destroy**](#function-rateconverter_destroy) ([**RateConverter\_state\_t**](structRateConverter__state__t.md) \* s) <br>_Free all resources. NULL is a no-op._  |
|  int | [**RateConverter\_enable\_agc**](#function-rateconverter_enable_agc) ([**RateConverter\_state\_t**](structRateConverter__state__t.md) \* s, double bn\_sym, double alpha) <br>_Level the stream feeding the terminal (matched) stage._  |
|  size\_t | [**RateConverter\_execute**](#function-rateconverter_execute) ([**RateConverter\_state\_t**](structRateConverter__state__t.md) \* s, const float \_Complex \* in, size\_t n\_in, float \_Complex \* out, size\_t max\_out) <br>_Convert a block of CF32 samples through the cascade. Passes input through each stage in order, ping-ponging between two intermediate buffers. State persists between calls, so contiguous calls on sequential blocks give the same result as one large call. Output length is approximately n\_in \* rate._  |
|  size\_t | [**RateConverter\_execute\_ctrl**](#function-rateconverter_execute_ctrl) ([**RateConverter\_state\_t**](structRateConverter__state__t.md) \* s, const float \_Complex \* x, size\_t n\_in, double ctrl, float \_Complex \* out, size\_t max\_out) <br>_Convert a block, steering the cascade's fractional stage by_ `ctrl` _._ |
|  size\_t | [**RateConverter\_execute\_ctrl\_max\_out**](#function-rateconverter_execute_ctrl_max_out) ([**RateConverter\_state\_t**](structRateConverter__state__t.md) \* s) <br>_As_ [_**RateConverter\_execute\_max\_out()**_](RateConverter__core_8h.md#function-rateconverter_execute_max_out) _, for the block control form._ |
|  size\_t | [**RateConverter\_execute\_ctrl\_push**](#function-rateconverter_execute_ctrl_push) ([**RateConverter\_state\_t**](structRateConverter__state__t.md) \* s, float \_Complex x, double ctrl, float \_Complex \* out, size\_t max\_out) <br>_Push ONE input sample; emit whatever outputs it completes._  |
|  size\_t | [**RateConverter\_execute\_ctrl\_push\_max\_out**](#function-rateconverter_execute_ctrl_push_max_out) ([**RateConverter\_state\_t**](structRateConverter__state__t.md) \* s) <br>_Bound for ONE pushed input:_ `ceil(rate) + 1` _output periods. Non-zero because the push form has no input block to size from._ |
|  size\_t | [**RateConverter\_execute\_max\_out**](#function-rateconverter_execute_max_out) ([**RateConverter\_state\_t**](structRateConverter__state__t.md) \* s) <br>_Upper bound on execute output for a standard 65536-sample block._  |
|  double | [**RateConverter\_gain**](#function-rateconverter_gain) (const [**RateConverter\_state\_t**](structRateConverter__state__t.md) \* s) <br>_The cascade's response to a constant input, from its stages' own coefficients — computed, never measured._  |
|  bool | [**RateConverter\_get\_clipped**](#function-rateconverter_get_clipped) (const [**RateConverter\_state\_t**](structRateConverter__state__t.md) \* s) <br>_Has any planned CIC stage clipped its input since the last reset?_  |
|  bool | [**RateConverter\_get\_narrow\_pulse**](#function-rateconverter_get_narrow_pulse) (const [**RateConverter\_state\_t**](structRateConverter__state__t.md) \* s) <br>_Is this converter's rectangular matched filter degenerately narrow?_  |
|  double | [**RateConverter\_get\_rate**](#function-rateconverter_get_rate) (const [**RateConverter\_state\_t**](structRateConverter__state__t.md) \* s) <br>_Get / set the output-to-input sample rate ratio. The setter rebuilds the entire cascade (new stage selection, new sub-objects) and resets all filter memories — equivalent to destroying and recreating with the new rate. Setting rate &lt;= 0 is silently ignored._  |
|  void | [**RateConverter\_get\_state**](#function-rateconverter_get_state) (const [**RateConverter\_state\_t**](structRateConverter__state__t.md) \* s, void \* blob) <br>_Serialize_ `s's` _active-stage state into_`blob` _._ |
|  size\_t | [**RateConverter\_num\_bank\_shape**](#function-rateconverter_num_bank_shape) (const [**RateConverter\_state\_t**](structRateConverter__state__t.md) \* s) <br>_Terminal polyphase bank shape (backs the_ `bank_shape` _property)._ |
|  size\_t | [**RateConverter\_num\_stages**](#function-rateconverter_num_stages) (const [**RateConverter\_state\_t**](structRateConverter__state__t.md) \* s) <br>_Number of planned cascade stages (backs the_ `stages` _property)._ |
|  void | [**RateConverter\_reset**](#function-rateconverter_reset) ([**RateConverter\_state\_t**](structRateConverter__state__t.md) \* s) <br>_Zero all sub-stage filter memories. Rate, stage count, and stage types are preserved. Processing from a reset state produces the same output as a freshly created converter fed the same input. Use between signal bursts to suppress transient artefacts from prior filter memory._  |
|  void | [**RateConverter\_set\_rate**](#function-rateconverter_set_rate) ([**RateConverter\_state\_t**](structRateConverter__state__t.md) \* s, double rate) <br>_Change the rate; rebuilds the cascade and resets all filter state. Silently ignores rate &lt;= 0._  |
|  int | [**RateConverter\_set\_state**](#function-rateconverter_set_state) ([**RateConverter\_state\_t**](structRateConverter__state__t.md) \* s, const void \* blob) <br>_Restore active-stage state from_ `blob` _(same rate)._ |
|  int | [**RateConverter\_set\_telemetry**](#function-rateconverter_set_telemetry) ([**RateConverter\_state\_t**](structRateConverter__state__t.md) \* s, [**dp\_tlm\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_t) \* tlm, const char \* prefix, uint32\_t decim) <br>_Attach (or detach) a telemetry context on the pre-terminal AGC._  |
|  int | [**RateConverter\_stage\_label**](#function-rateconverter_stage_label) ([**RateConverter\_state\_t**](structRateConverter__state__t.md) \* s, int i, char \* buf, size\_t len) <br>_Write a human-readable label for stage i into buf._  |
|  const char \* | [**RateConverter\_stages\_value**](#function-rateconverter_stages_value) (const [**RateConverter\_state\_t**](structRateConverter__state__t.md) \* s, size\_t i) <br>_Label of stage_ `i` _, e.g. "CIC(8)+FIR" or "Resampler(0.923,rrc)"._ |
|  size\_t | [**RateConverter\_state\_bytes**](#function-rateconverter_state_bytes) (const [**RateConverter\_state\_t**](structRateConverter__state__t.md) \* s) <br>_Bytes_ [_**RateConverter\_get\_state()**_](RateConverter__core_8h.md#function-rateconverter_get_state) _writes for_`s` _(envelope + stages)._ |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**RC\_MAX\_STAGES**](RateConverter__core_8h.md#define-rc_max_stages)  `3`<br> |
| define  | [**RC\_STATE\_MAGIC**](RateConverter__core_8h.md#define-rc_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('R', 'C', 'V', 'T')`<br> |
| define  | [**RC\_STATE\_VERSION**](RateConverter__core_8h.md#define-rc_state_version)  `2u`<br> |

## Detailed Description


Selects the cheapest cascade of CIC, HalfbandDecimator, and/or polyphase Resampler stages for a given output/input sample rate ratio at creation time. All sub-stage C objects are owned by the state struct.


Stage selection (D = 1/rate):


rate &gt;= 1.0 or D &lt; 2 `[Resampler(rate)]` D ~= 2^1 `[HalfbandDecimator]` D ~= 2^2 `[HalfbandDecimator, HalfbandDecimator]` D ~= 2^n, n&gt;=3, D&lt;=4096 `[CIC(D)]` D &gt;= 8, non-power-of-2 `[CIC(R*), Resampler correction]` R\* = nearest power-of-2 to D otherwise (2 &lt;= D &lt; 8, non-int) `[Resampler(rate)]`


**INPUT AMPLITUDE IS BOUNDED whenever the plan contains a CIC stage** — that is, any decimation by 8 or more: \|Re\| and \|Im\| &lt;= 2.0, clipped beyond that, before any filtering. The bound is `CIC_PAPR_HEADROOM` (6 dB above unity), which is there so a pulse-shaped waveform's PEAKS have somewhere to sit above its unit average; see [**cic\_core.h**](cic__core_8h.md). `stages` is how you tell: a plan naming `CIC(...)` is not scale-free, every other plan is. This is the one property of this object a caller cannot infer from an output that is finite and looks plausible — an overdriven RRC-BPSK waveform matched-filters to -25 dB EVM where the same waveform well inside the bound reaches -50 dB.


Lifecycle: 
```C++
RateConverter_state_t *rc = RateConverter_create(0.1, 0);
// rc->n_stages == 2: CIC(8) then Resampler(0.8)
float _Complex out[512];
size_t n = RateConverter_execute(rc, in, 4096, out, 512);
RateConverter_destroy(rc);
```
 


    
## Public Types Documentation




### enum rc\_pulse\_t 

_Matched-filter pulse selection for the terminal stage._ 
```C++
enum rc_pulse_t {
    RC_PULSE_IANDD = 0,
    RC_PULSE_RRC = 1,
    RC_PULSE_NONE = 2
};
```



A cascade whose terminal stage carries a _pulse-shaped_ bank instead of the default Kaiser anti-alias bank IS the matched filter — the same dot product does the rate conversion and the matched filtering, and its arm is the fractional timing delay a downstream loop steers. Values 0/1 match `MPSK_RX_PULSE_IANDD`/`_RRC` so one vocabulary covers the family. 


        

<hr>



### enum rc\_stage\_t 

```C++
enum rc_stage_t {
    RC_STAGE_HB = 0,
    RC_STAGE_CIC = 1,
    RC_STAGE_RESAMP = 2
};
```



Stage type tags. 


        

<hr>
## Public Functions Documentation




### function RateConverter\_agc\_gain\_db 

_Gain the pre-terminal AGC last applied, in dB; 0.0 when off._ 
```C++
double RateConverter_agc_gain_db (
    const RateConverter_state_t * s
) 
```



The cascade's time-varying gain, kept deliberately separate from [**RateConverter\_gain()**](RateConverter__core_8h.md#function-rateconverter_gain): that function reports the response computed from the stages' own COEFFICIENTS, and an AGC has none. A caller asking "what did
this cascade do to my amplitude" with the AGC on wants both, and they multiply. 


        

<hr>



### function RateConverter\_agc\_ref\_db 

_The pre-terminal AGC's reference level, in dB._ 
```C++
double RateConverter_agc_ref_db (
    const RateConverter_state_t * s
) 
```



`10*log10(bank_e0 / bank_sps)` — the average power a unit-amplitude symbol stream has where the AGC sits, derived from the terminal bank's own pulse energy. Defined for any MATCHED cascade whether or not the AGC is enabled, because it describes the bank rather than the loop; 0.0 for a plain one. 


        

<hr>



### function RateConverter\_bank\_shape\_value 

_Element_ `i` _of the bank shape: 0 -&gt; num\_phases, 1 -&gt; num\_taps._
```C++
size_t RateConverter_bank_shape_value (
    const RateConverter_state_t * s,
    size_t i
) 
```




<hr>



### function RateConverter\_convert 

_One-shot rate conversion — no persistent state required._ 
```C++
size_t RateConverter_convert (
    double rate,
    int compensate,
    const float _Complex * in,
    size_t n_in,
    float _Complex * out,
    size_t max_out
) 
```



Creates a temporary converter, converts n\_in samples, destroys it. Equivalent to: 
```C++
RateConverter_state_t *rc = RateConverter_create(rate, compensate);
size_t n = RateConverter_execute(rc, in, n_in, out, max_out);
RateConverter_destroy(rc);
```



Use [**RateConverter\_create()**](RateConverter__core_8h.md#function-rateconverter_create) directly when processing multiple blocks at the same rate — the one-shot form resets filter memory on every call.




**Parameters:**


* `rate` Output-to-input sample rate ratio. 
* `compensate` Non-zero to enable CIC droop compensation. 
* `in` CF32 input samples. 
* `n_in` Number of input samples. 
* `out` Output buffer. 
* `max_out` Output buffer capacity in samples. 



**Returns:**

Number of output samples written; 0 only if OOM or n\_in == 0. 





        

<hr>



### function RateConverter\_create 

_Create a rate converter for the given output/input rate ratio. Selects the cheapest cascade of CIC, HalfbandDecimator, and/or polyphase Resampler stages at construction time (see file header for the selection table). Setting compensate=1 appends a closed-form Molnar-Vucic CIC droop-compensating FIR after any CIC stage, which improves passband flatness at the cost of one extra FIR stage._ 
```C++
RateConverter_state_t * RateConverter_create (
    double rate,
    int compensate
) 
```





**Parameters:**


* `rate` Output-to-input sample rate ratio. Any positive float. 
* `compensate` Non-zero to append a CIC passband-droop compensating FIR after any CIC stage. 



**Returns:**

Non-NULL on success; NULL if rate &lt;= 0 or OOM.



```C++
>>> from doppler.resample import RateConverter
>>> rc = RateConverter(rate=0.5, compensate=0)
>>> rc.rate
0.5
```
 


        

<hr>



### function RateConverter\_create\_matched 

_Create a rate converter whose terminal stage IS a matched filter._ 
```C++
RateConverter_state_t * RateConverter_create_matched (
    double rate,
    int compensate,
    int pulse,
    double beta,
    size_t span,
    double pulse_sps,
    size_t num_phases
) 
```



Plans the same cheap cascade as [**RateConverter\_create()**](RateConverter__core_8h.md#function-rateconverter_create), then puts a pulse-shaped polyphase bank on the **terminal** stage instead of the default Kaiser one. The cascade therefore does rate conversion and matched filtering in a single dot product, and that stage's polyphase arm is the fractional timing delay — which is what makes [**RateConverter\_execute\_ctrl()**](RateConverter__core_8h.md#function-rateconverter_execute_ctrl) a timing control port rather than just a Doppler knob.


Three things this does that plain create() cannot:



* **The terminal fractional stage always exists.** The ordinary planner drops it for an exact power-of-two decimation, and again when the correction lands within 1e-6 of 1.0 — so `rate = 2/64` plans a bare `CIC(32)` with nothing steerable at the end. Here the terminal stage is simultaneously the matched filter and the timing element, so it is appended (at rate 1.0 if there is no rate left to correct).
* **The bank is sized by the POST-decimation rate.** Matched-filtering at the input rate costs taps proportional to the input samples per symbol (4225 taps/arm at 256 samples/symbol — 17 MB of bank); after the integer stages have done the bulk decimation it is ~`2*span*pulse_sps` taps, constant in the input rate.
* **CIC droop folds into the bank**, exactly rather than approximately: the Molnar-Vucic compensator (ciccompmf) runs at the decimated rate, which IS the terminal stage's tap grid, so the fold is a per-arm convolution and costs no extra stage. `compensate` therefore adds no FIR on this path.




Measured on RRC-BPSK (beta 0.35, span 8, two outputs per symbol), best-case timing phase, noiseless: a halfband cascade reaches -60 dB EVM; a CIC cascade reaches **-50 dB with `compensate = 1` and only -22 dB without**, so on this path compensation is not a refinement — it is 28 dB, for six extra taps per arm and no extra pass over the data. Folded or appended agree to within 0.6 dB, i.e. the fold gives up nothing to a separate comp FIR.




**Note:**

Keep the INPUT inside +-1.0 whenever the plan contains a CIC stage — see the file header. The clip is silent, and it costs 25 dB of the EVM quoted above for reasons that have nothing to do with the matched filter.




**Parameters:**


* `rate` Output-to-input sample rate ratio (any positive float). Rate-agnostic: this object never learns about symbols — a caller wanting `m` samples per symbol asks for `rate = m/sps`. 
* `compensate` Non-zero to correct CIC passband droop (folded into the bank here, not appended as a stage). 
* `pulse` RC\_PULSE\_RRC / RC\_PULSE\_IANDD. RC\_PULSE\_NONE is invalid here — use [**RateConverter\_create()**](RateConverter__core_8h.md#function-rateconverter_create) for a plain conversion. 
* `beta` RRC roll-off in `[0, 1]` (ignored for the rectangle). 
* `span` One-sided RRC span in symbols (ignored for the rectangle, whose support is always exactly one symbol). 
* `pulse_sps` The pulse's period measured in **output** samples (2 = two samples per symbol out). This is a shape parameter, not a rate-planning one: a matched filter has a symbol duration, and the planner still knows nothing of symbols. 
* `num_phases` Terminal-stage arms; power of two. Sets the fractional timing resolution to `1/num_phases` of an output period. 



**Returns:**

Non-NULL on success; NULL on a bad parameter or OOM. 





        

<hr>



### function RateConverter\_destroy 

_Free all resources. NULL is a no-op._ 
```C++
void RateConverter_destroy (
    RateConverter_state_t * s
) 
```




<hr>



### function RateConverter\_enable\_agc 

_Level the stream feeding the terminal (matched) stage._ 
```C++
int RateConverter_enable_agc (
    RateConverter_state_t * s,
    double bn_sym,
    double alpha
) 
```



Wedges an AGC into the cascade immediately BEFORE the terminal polyphase stage — after every integer decimation, ahead of the matched filter and the timing element. Off until this is called, and off is what both constructors build, so a plain cascade is untouched and [**RateConverter\_gain()**](RateConverter__core_8h.md#function-rateconverter_gain) still reads exactly 1.0.




**
**

The consumer is a timing-error detector. A TED's raw output is the timing error multiplied by three things it did not choose — the signal amplitude, the transition density, and the detector's own slope — and only the last is the detector's to divide out ([**symsync\_ted\_slope()**](symsync__core_8h.md#function-symsync_ted_slope), which is computed at construct FOR A UNIT-AMPLITUDE SYMBOL STREAM). Amplitude enters as `A^2` for Gardner and `A^1` for DTTL, so a 4x level error is a 16x loop-gain error. Levelling it is this object's job because this object owns the bank that sets what "unit amplitude" means.


The tap is pre-terminal rather than post because the terminal stage's OUTPUT rate is the one a timing loop is actively steering, and an AGC whose bandwidth is quoted in cycles per sample of a stream another loop is stretching is coupled to that loop. The pre-terminal rate is fixed.




**
**

The AGC sets average POWER; the TED wants unit symbol AMPLITUDE. The bridge is the pulse's own energy on its own tap grid — `bank_e0 = sum h(t)^2`, the quantity the bank is already normalised by — so for i.i.d. unit-power symbols at `bank_sps` samples per symbol the pre-terminal average power is `bank_e0 / bank_sps` and that is the reference. No caller supplies a level; read it back with [**RateConverter\_agc\_ref\_db()**](RateConverter__core_8h.md#function-rateconverter_agc_ref_db).




**Note:**

This levels signal PLUS noise, so at finite Es/N0 the symbols land slightly low — about 0.95x amplitude at 10 dB Es/N0, i.e. 0.91x Gardner loop gain. That is a fact of the measurement, not an error to estimate away: an AGC that tried to exclude noise would be estimating the very quantity the receiver is trying to measure.




**
**

`bn_sym` is in cycles per SYMBOL, matching every other loop bandwidth in this family, and is converted to the AGC's own per-sample units with the one number that describes its position (`bn_sym / bank_sps`). It must stay well below the bandwidth of every loop downstream — an AGC divides out the amplitude those loops' discriminators are built around, so one running near a loop's bandwidth corrects the excursions that loop is itself producing. See [**mpsk\_rx\_agc\_bn()**](mpsk__rx__loops_8h.md#function-mpsk_rx_agc_bn) for the ratio a composing receiver uses.


The loop starts at unity gain and walks to the level; there is no seed and no sample is treated specially at the start. A seed is a STEP in gain, and one taken off a signal that has not arrived is a shock the loops downstream cannot absorb  see \_agc\_tap() for the measurement that settled this. So `bn_sym` also sets how fast a level error is corrected, and a very slow AGC leaves the early symbols under- or over-driven for a loop time constant.




**Parameters:**


* `s` Must be non-NULL, and must be a MATCHED cascade ([**RateConverter\_create\_matched()**](RateConverter__core_8h.md#function-rateconverter_create_matched)) — a plain one has no pulse and therefore no reference to derive. 
* `bn_sym` AGC loop noise bandwidth in cycles/symbol; &gt; 0. 
* `alpha` Power-detector EMA coefficient, in (0, 1]. 



**Returns:**

DP\_OK, or DP\_ERR\_INVALID for a plain cascade or a bad parameter (the converter is left exactly as it was, AGC still off).



```C++
RateConverter_state_t *rc =
    RateConverter_create_matched (2.0 / 8.0, 1, RC_PULSE_RRC, 0.35, 8,
                                  2.0, 1024);
RateConverter_enable_agc (rc, 1e-4, 0.01);
printf ("%.2f dB\n", RateConverter_agc_ref_db (rc));
RateConverter_destroy (rc);
```
 


        

<hr>



### function RateConverter\_execute 

_Convert a block of CF32 samples through the cascade. Passes input through each stage in order, ping-ponging between two intermediate buffers. State persists between calls, so contiguous calls on sequential blocks give the same result as one large call. Output length is approximately n\_in \* rate._ 
```C++
size_t RateConverter_execute (
    RateConverter_state_t * s,
    const float _Complex * in,
    size_t n_in,
    float _Complex * out,
    size_t max_out
) 
```





**Parameters:**


* `s` Pointer to a valid [**RateConverter\_state\_t**](structRateConverter__state__t.md). 
* `in` CF32 input block. 
* `n_in` Number of input samples. 
* `out` Output buffer; must hold at least max\_out samples. 
* `max_out` Capacity of out in samples. 



**Returns:**

CF32 output array; length is approximately n\_in \* rate.



```C++
>>> from doppler.resample import RateConverter
>>> import numpy as np
>>> rc = RateConverter(rate=0.5, compensate=0)
>>> y = rc.execute(np.zeros(1024, dtype=np.complex64))
>>> y.shape, y.dtype
((512,), dtype('complex64'))
```
 


        

<hr>



### function RateConverter\_execute\_ctrl 

_Convert a block, steering the cascade's fractional stage by_ `ctrl` _._
```C++
size_t RateConverter_execute_ctrl (
    RateConverter_state_t * s,
    const float _Complex * x,
    size_t n_in,
    double ctrl,
    float _Complex * out,
    size_t max_out
) 
```



The control-port form of [**RateConverter\_execute()**](RateConverter__core_8h.md#function-rateconverter_execute): the fixed integer stages (HalfbandDecimator / CIC) run unchanged, and the scalar rate deviation `ctrl` is forwarded to the **terminal polyphase Resampler stage's** accumulator (via resamp\_execute\_ctrl\_push) — so its effective rate becomes `stage_rate + ctrl` for this call. This exposes the fractional tail's control port that [**RateConverter\_execute()**](RateConverter__core_8h.md#function-rateconverter_execute) hides: a timing/rate-tracking loop can decimate a high input rate cheaply through the HB/CIC stages and then arbitrary-rate + strobe-align in the last stage, updating `ctrl` per block.


`ctrl` is referenced to the terminal stage's (post-decimation) rate, not the overall rate. It is meaningful only when the cascade actually ends in a Resampler stage; a pure integer HB/CIC cascade has no fractional stage to steer, so this **falls through to [**RateConverter\_execute()**](RateConverter__core_8h.md#function-rateconverter_execute)** (ctrl ignored).




**Parameters:**


* `s` Pointer to a valid [**RateConverter\_state\_t**](structRateConverter__state__t.md). 
* `x` CF32 input block. 
* `n_in` Number of input samples. 
* `ctrl` Rate deviation added to the terminal Resampler stage's rate. 
* `out` Output buffer; must hold at least max\_out samples. 
* `max_out` Capacity of out in samples. 



**Returns:**

CF32 output array; length tracks the accumulated effective rate.



```C++
>>> from doppler.resample import RateConverter
>>> import numpy as np
>>> rc = RateConverter(rate=0.8, compensate=0)  # -> Resampler(0.8)
>>> x = np.ones(1000, dtype=np.complex64)
>>> rc.execute_ctrl(x, 0.0).shape[0]    # base rate: 1000 -> 800
800
>>> rc2 = RateConverter(rate=0.8, compensate=0)
>>> rc2.execute_ctrl(x, 0.05).shape[0]  # +ctrl speeds the tail up
851
```
 


        

<hr>



### function RateConverter\_execute\_ctrl\_max\_out 

_As_ [_**RateConverter\_execute\_max\_out()**_](RateConverter__core_8h.md#function-rateconverter_execute_max_out) _, for the block control form._
```C++
size_t RateConverter_execute_ctrl_max_out (
    RateConverter_state_t * s
) 
```




<hr>



### function RateConverter\_execute\_ctrl\_push 

_Push ONE input sample; emit whatever outputs it completes._ 
```C++
size_t RateConverter_execute_ctrl_push (
    RateConverter_state_t * s,
    float _Complex x,
    double ctrl,
    float _Complex * out,
    size_t max_out
) 
```



The per-input streaming form of [**RateConverter\_execute\_ctrl()**](RateConverter__core_8h.md#function-rateconverter_execute_ctrl), and the only form a closed loop can use: a block call must know its whole `ctrl` history up front, whereas a timing loop computes each correction _from_ the outputs already emitted. Feeding a stream one sample at a time through this reproduces [**RateConverter\_execute\_ctrl()**](RateConverter__core_8h.md#function-rateconverter_execute_ctrl) on the same block bit-for-bit when `ctrl` is held constant (the cascade is block-boundary invariant), so the cheap block form stays correct for open-loop use.


The integer HB/CIC stages consume the sample and emit at most one intermediate sample each; the terminal Resampler stage then emits 0 outputs (a decimator between strobes — the common case), 1, or several (an interpolator). A cascade with no terminal Resampler ignores `ctrl`.




**Parameters:**


* `s` Pointer to a valid [**RateConverter\_state\_t**](structRateConverter__state__t.md). 
* `x` One CF32 input sample. 
* `ctrl` Rate deviation added to the terminal stage's rate for this input (referenced to the terminal, post-decimation rate). 
* `out` Output buffer for any emitted samples. 
* `max_out` Capacity of `out` (emission stops at this bound). 



**Returns:**

CF32 array of the outputs completed by this input (0, 1, or more).



```C++
>>> from doppler.resample import RateConverter
>>> import numpy as np
>>> rc = RateConverter(rate=0.8, compensate=0)  # -> Resampler(0.8)
>>> x = (np.arange(10, dtype=np.float32) + 1).astype(np.complex64)
>>> # a decimator emits 0 between strobes, 1 on a strobe:
>>> [rc.execute_ctrl_push(complex(v), 0.0).shape[0] for v in x]
[1, 1, 1, 1, 0, 1, 1, 1, 1, 0]
```
 


        

<hr>



### function RateConverter\_execute\_ctrl\_push\_max\_out 

_Bound for ONE pushed input:_ `ceil(rate) + 1` _output periods. Non-zero because the push form has no input block to size from._
```C++
size_t RateConverter_execute_ctrl_push_max_out (
    RateConverter_state_t * s
) 
```




<hr>



### function RateConverter\_execute\_max\_out 

_Upper bound on execute output for a standard 65536-sample block._ 
```C++
size_t RateConverter_execute_max_out (
    RateConverter_state_t * s
) 
```



Returns (size\_t)(65536 \* max(rate, 1.0)) + 2. The Python extension uses this to pre-allocate the output buffer on the first execute call. 


        

<hr>



### function RateConverter\_gain 

_The cascade's response to a constant input, from its stages' own coefficients — computed, never measured._ 
```C++
double RateConverter_gain (
    const RateConverter_state_t * s
) 
```



Each stage answers for itself ([**hbdecim\_dc\_gain()**](hbdecim__core_8h.md#function-hbdecim_dc_gain), [**cic\_dc\_gain()**](cic__core_8h.md#function-cic_dc_gain) times [**fir\_dc\_gain()**](fir__core_8h.md#function-fir_dc_gain) for a compensated CIC, [**resamp\_dc\_gain()**](resamp__core_8h.md#function-resamp_dc_gain)) and this is their product. So the number tracks whatever the stages actually hold: if a filter's normalisation drifts, this moves with it, and a gate comparing it against a measured DC probe catches the drift from either side.


**A plain cascade is unity** — a rate conversion that adds gain of its own is a defect, and `RateConverter_create()` returns 1.0 here at every rate.


**A matched cascade is not, and should not be.** Its terminal stage is a matched filter, which is deliberately not flat; the invariant that holds there is at the SYMBOL level (a symbol of amplitude A in, amplitude A out), not at DC. This function still reports that cascade's true DC gain, which is the pulse's `sum(h)/sum(h^2)`.




**Parameters:**


* `s` State. Must be non-NULL. 



**Returns:**

The DC gain of the whole cascade.



```C++
RateConverter_state_t *rc = RateConverter_create (1.0 / 12.0, 1);
printf ("%.4f\n", RateConverter_gain (rc));   // 1.0000
RateConverter_destroy (rc);
```
 


        

<hr>



### function RateConverter\_get\_clipped 

_Has any planned CIC stage clipped its input since the last reset?_ 
```C++
bool RateConverter_get_clipped (
    const RateConverter_state_t * s
) 
```



The cascade inherits cic\_core's input bound (\|Re\|, \|Im\| &lt;= 1.0) whenever the plan contains a CIC — any decimation by 8 or more — and the clip does not show up in the samples: the output stays finite and plausible, merely distorted. This is the only reliable way to find out, and it is free (the boundary comparisons run on every sample regardless).


Sticky, cleared by [**RateConverter\_reset()**](RateConverter__core_8h.md#function-rateconverter_reset). Always 0 for a cascade with no CIC stage, which is the honest answer: those plans are scale-free.




**Parameters:**


* `s` Pointer to a valid [**RateConverter\_state\_t**](structRateConverter__state__t.md). 



**Returns:**

1 if any CIC stage has clipped, else 0. 





        

<hr>



### function RateConverter\_get\_narrow\_pulse 

_Is this converter's rectangular matched filter degenerately narrow?_ 
```C++
bool RateConverter_get_narrow_pulse (
    const RateConverter_state_t * s
) 
```



True only for a matched cascade built with RC\_PULSE\_IANDD and `pulse_sps < 4`: the rectangle is exactly one symbol wide, so its matched filter is a 2-3 tap sum there. It works — it just barely opens the eye (measured on the timing loop this feeds, a lock statistic of -0.34 at two samples per symbol against +0.95 at four). The RRC spans many symbols and is never affected. Construction also raises a UserWarning. 


        

<hr>



### function RateConverter\_get\_rate 

_Get / set the output-to-input sample rate ratio. The setter rebuilds the entire cascade (new stage selection, new sub-objects) and resets all filter memories — equivalent to destroying and recreating with the new rate. Setting rate &lt;= 0 is silently ignored._ 
```C++
double RateConverter_get_rate (
    const RateConverter_state_t * s
) 
```




```C++
>>> from doppler.resample import RateConverter
>>> rc = RateConverter(rate=0.5, compensate=0)
>>> rc.rate
0.5
>>> rc.rate = 2.0
>>> rc.rate
2.0
```
 


        

<hr>



### function RateConverter\_get\_state 

_Serialize_ `s's` _active-stage state into_`blob` _._
```C++
void RateConverter_get_state (
    const RateConverter_state_t * s,
    void * blob
) 
```




<hr>



### function RateConverter\_num\_bank\_shape 

_Terminal polyphase bank shape (backs the_ `bank_shape` _property)._
```C++
size_t RateConverter_num_bank_shape (
    const RateConverter_state_t * s
) 
```





**Returns:**

2 when the cascade ends in a Resampler stage, else 0. 





        

<hr>



### function RateConverter\_num\_stages 

_Number of planned cascade stages (backs the_ `stages` _property)._
```C++
size_t RateConverter_num_stages (
    const RateConverter_state_t * s
) 
```




<hr>



### function RateConverter\_reset 

_Zero all sub-stage filter memories. Rate, stage count, and stage types are preserved. Processing from a reset state produces the same output as a freshly created converter fed the same input. Use between signal bursts to suppress transient artefacts from prior filter memory._ 
```C++
void RateConverter_reset (
    RateConverter_state_t * s
) 
```




```C++
>>> from doppler.resample import RateConverter
>>> rc = RateConverter(rate=0.5, compensate=0)
>>> rc.reset()
>>> rc.rate
0.5
```
 


        

<hr>



### function RateConverter\_set\_rate 

_Change the rate; rebuilds the cascade and resets all filter state. Silently ignores rate &lt;= 0._ 
```C++
void RateConverter_set_rate (
    RateConverter_state_t * s,
    double rate
) 
```





**Parameters:**


* `s` Pointer to a valid [**RateConverter\_state\_t**](structRateConverter__state__t.md). 
* `rate` New output/input rate ratio. 




        

<hr>



### function RateConverter\_set\_state 

_Restore active-stage state from_ `blob` _(same rate)._
```C++
int RateConverter_set_state (
    RateConverter_state_t * s,
    const void * blob
) 
```





**Returns:**

DP\_OK, or DP\_ERR\_INVALID if the blob's envelope rejects. 





        

<hr>



### function RateConverter\_set\_telemetry 

_Attach (or detach) a telemetry context on the pre-terminal AGC._ 
```C++
int RateConverter_set_telemetry (
    RateConverter_state_t * s,
    dp_tlm_t * tlm,
    const char * prefix,
    uint32_t decim
) 
```



The cascade has no loop of its own to report — the stages are fixed filters — so this forwards to the one child that does: the pre-terminal AGC, under `prefix` verbatim. It registers that child's probes ("&lt;prefix&gt;.gain\_db" and "&lt;prefix&gt;.level\_db"; see [**agc\_set\_telemetry()**](agc__core_8h.md#function-agc_set_telemetry)) and nothing else, which is why the prefix is not extended with a component name — there is no second thing here to disambiguate it from.


A composing object forwards its own prefix down: an `mpsk_receiver` attached as "rx" passes "rx.agc", and the receiver's gain trajectory joins its carrier and timing probes on one context.


With the AGC off (a plain cascade, or a matched one where [**RateConverter\_enable\_agc()**](RateConverter__core_8h.md#function-rateconverter_enable_agc) was never called) there is nothing to instrument and this is a successful no-op — DP\_OK with no probes registered. That is deliberate: whether the AGC exists is the composing receiver's construction-time choice (`agc = 0`), and a caller attaching telemetry should not have to know which way that went to avoid an error.


The attachment is remembered as a REQUEST, so it survives the AGC being rebuilt by a rate change and is applied to an AGC enabled after the fact. One consequence of that: an attach made before the AGC exists reports DP\_OK here, and if the probe table has filled by the time the AGC is built the probes are dropped without failing the build — signal processing does not fail because observation could not be set up. Attach after construction (which is what every composing object here does) and the return value covers it; otherwise check the context's probe names.


Setup path, never hot: call before the producer thread starts; the context is borrowed and must outlive the attachment (SPSC rules in [**dp\_tlm/dp\_tlm\_core.h**](dp__tlm__core_8h.md)). Passing NULL detaches.




**Parameters:**


* `s` Must be non-NULL. 
* `tlm` Telemetry context to attach, or NULL to detach. 
* `prefix` Probe-name prefix, e.g. "agc" or "rx.agc". 
* `decim` Emit every decim-th gain update; &gt;= 1. 



**Returns:**

DP\_OK — including when no AGC is enabled — or DP\_ERR\_INVALID when the probe table cannot take the AGC's probes (the attach fails whole; the AGC stays detached).



```C++
RateConverter_state_t *rc =
    RateConverter_create_matched (2.0 / 8.0, 1, RC_PULSE_RRC, 0.35, 8,
                                  2.0, 1024);
RateConverter_enable_agc (rc, 1e-4, 0.01);
dp_tlm_t *tlm = dp_tlm_create (1 << 12);
RateConverter_set_telemetry (rc, tlm, "agc", 1);
RateConverter_destroy (rc);
dp_tlm_destroy (tlm);
```
 


        

<hr>



### function RateConverter\_stage\_label 

_Write a human-readable label for stage i into buf._ 
```C++
int RateConverter_stage_label (
    RateConverter_state_t * s,
    int i,
    char * buf,
    size_t len
) 
```



Examples: "HalfbandDecimator", "CIC(8)", "CIC(8)+FIR", "Resampler(0.8)".




**Parameters:**


* `s` Must be non-NULL. 
* `i` Stage index in `[0, s->n_stages)`. 
* `buf` Output buffer. 
* `len` Capacity of buf in bytes. 



**Returns:**

1 on success, 0 if i is out of range. 





        

<hr>



### function RateConverter\_stages\_value 

_Label of stage_ `i` _, e.g. "CIC(8)+FIR" or "Resampler(0.923,rrc)"._
```C++
const char * RateConverter_stages_value (
    const RateConverter_state_t * s,
    size_t i
) 
```



Points at a per-thread scratch buffer valid until this thread's next call — the binding converts it to a Python string immediately. NULL if `i` is out of range. 


        

<hr>



### function RateConverter\_state\_bytes 

_Bytes_ [_**RateConverter\_get\_state()**_](RateConverter__core_8h.md#function-rateconverter_get_state) _writes for_`s` _(envelope + stages)._
```C++
size_t RateConverter_state_bytes (
    const RateConverter_state_t * s
) 
```




<hr>
## Macro Definition Documentation





### define RC\_MAX\_STAGES 

```C++
#define RC_MAX_STAGES `3`
```



Maximum number of visible cascade stages. 


        

<hr>



### define RC\_STATE\_MAGIC 

```C++
#define RC_STATE_MAGIC `DP_FOURCC ('R', 'C', 'V', 'T')`
```




<hr>



### define RC\_STATE\_VERSION 

```C++
#define RC_STATE_VERSION `2u`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/RateConverter/RateConverter_core.h`

