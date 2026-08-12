

# File ratesync\_core.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**ratesync**](dir_bd24358a1650cccc3777ef85b64503d5.md) **>** [**ratesync\_core.h**](ratesync__core_8h.md)

[Go to the source code of this file](ratesync__core_8h_source.md)

_RateSync — symbol-timing recovery on a matched-filter rate cascade._ [More...](#detailed-description)

* `#include "RateConverter/RateConverter_core.h"`
* `#include "cic/cic_core.h"`
* `#include "clib_common.h"`
* `#include "dp_state.h"`
* `#include "fir/fir_core.h"`
* `#include "hbdecim/hbdecim_core.h"`
* `#include "jm_perf.h"`
* `#include "lockdet/lockdet_core.h"`
* `#include "loop_filter/loop_filter_core.h"`
* `#include "resamp/resamp_core.h"`
* `#include "resample/resample_core.h"`
* `#include "symsync/symsync_core.h"`
* `#include "dp_tlm/dp_tlm_core.h"`
* `#include "telemetry/telemetry_core.h"`
* `#include "ber/ber_core.h"`
* `#include "pn/pn_core.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**ratesync\_loop\_t**](structratesync__loop__t.md) <br>_The symbol-timing loop, independent of what feeds it._  |
| struct | [**ratesync\_state\_t**](structratesync__state__t.md) <br>_RateSync state: a matched-filter cascade and the timing loop._  |
| struct | [**ratesync\_tlm\_t**](structratesync__tlm__t.md) <br>_Telemetry attachment: a borrowed context + this object's probe ids. NULL ctx (the default) means detached — every probe site is then one predicted-not-taken branch per recovered symbol._  |


## Public Types

| Type | Name |
| ---: | :--- |
| enum  | [**ratesync\_\_core\_8h\_1a61dadd085c1777f559549e05962b2c9e**](#enum-ratesync__core_8h_1a61dadd085c1777f559549e05962b2c9e)  <br>_Timing-error-detector selection for ratesync\_state\_t::ted._  |
| enum  | [**ratesync\_\_core\_8h\_1a726ca809ffd3d67ab4b8476646f26635**](#enum-ratesync__core_8h_1a726ca809ffd3d67ab4b8476646f26635)  <br>_Matched-filter pulse shape._  |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  void | [**ratesync\_configure**](#function-ratesync_configure) ([**ratesync\_state\_t**](structratesync__state__t.md) \* state, double bn, double zeta) <br>_Recompute the loop gains for a new bandwidth/damping, keeping the timing estimate._  |
|  void | [**ratesync\_configure\_lock\_raw**](#function-ratesync_configure_lock_raw) ([**ratesync\_state\_t**](structratesync__state__t.md) \* state, size\_t avgs, double up\_thresh, double down\_thresh, uint32\_t n\_up, uint32\_t n\_down) <br>_Set the lock detector's geometry directly._  |
|  [**ratesync\_state\_t**](structratesync__state__t.md) \* | [**ratesync\_create**](#function-ratesync_create) (double sps, int pulse, double beta, size\_t span, size\_t m, size\_t num\_phases, double bn, double zeta, int ted) <br>_Create a RateSync instance._  |
|  void | [**ratesync\_destroy**](#function-ratesync_destroy) ([**ratesync\_state\_t**](structratesync__state__t.md) \* state) <br>_Destroy a RateSync instance and release all memory._  |
|  double | [**ratesync\_get\_bn**](#function-ratesync_get_bn) (const [**ratesync\_state\_t**](structratesync__state__t.md) \* state) <br> |
|  int | [**ratesync\_get\_clipped**](#function-ratesync_get_clipped) (const [**ratesync\_state\_t**](structratesync__state__t.md) \* state) <br>_Has the cascade's CIC stage clipped its input since the last reset? Forwarded from the RateConverter: a CIC bounds its input to +-1.0 and clips silently past that, which no timing metric reveals. Always 0 when the plan has no CIC stage._  |
|  double | [**ratesync\_get\_ctrl**](#function-ratesync_get_ctrl) (const [**ratesync\_state\_t**](structratesync__state__t.md) \* state) <br>_Current per-input control deviation steering the strobe._  |
|  double | [**ratesync\_get\_lock\_stat**](#function-ratesync_get_lock_stat) (const [**ratesync\_state\_t**](structratesync__state__t.md) \* state) <br>_Last block-averaged lock statistic (the eye-opening ratio)._  |
|  int | [**ratesync\_get\_locked**](#function-ratesync_get_locked) (const [**ratesync\_state\_t**](structratesync__state__t.md) \* state) <br>_Current lock decision (1 = locked), verify-counted._  |
|  double | [**ratesync\_get\_rate**](#function-ratesync_get_rate) (const [**ratesync\_state\_t**](structratesync__state__t.md) \* state) <br>_Smoothed tracked samples per symbol. Departs from the nominal_ `sps` _by exactly the sample-clock offset being tracked, so it is the estimator a rate-disciplining caller reads._ |
|  void | [**ratesync\_get\_state**](#function-ratesync_get_state) (const [**ratesync\_state\_t**](structratesync__state__t.md) \* state, void \* blob) <br>_Serialize the mutable state into_ `blob` _._ |
|  double | [**ratesync\_get\_timing\_error**](#function-ratesync_get_timing_error) (const [**ratesync\_state\_t**](structratesync__state__t.md) \* state) <br>_Last normalised TED error — the loop stress._  |
|  void | [**ratesync\_loop\_bind\_cascade**](#function-ratesync_loop_bind_cascade) ([**ratesync\_loop\_t**](structratesync__loop__t.md) \* l, const [**RateConverter\_state\_t**](structRateConverter__state__t.md) \* rc) <br>_Read that geometry straight off a cascade._  |
|  void | [**ratesync\_loop\_configure**](#function-ratesync_loop_configure) ([**ratesync\_loop\_t**](structratesync__loop__t.md) \* l, double bn, double zeta) <br>_Retune the loop; preserves the integrator (and so the lock)._  |
|  void | [**ratesync\_loop\_configure\_lock\_raw**](#function-ratesync_loop_configure_lock_raw) ([**ratesync\_loop\_t**](structratesync__loop__t.md) \* l, size\_t avgs, double up\_thresh, double down\_thresh, uint32\_t n\_up, uint32\_t n\_down) <br>_Set the lock detector's geometry; see_ [_**ratesync\_configure\_lock\_raw()**_](ratesync__core_8h.md#function-ratesync_configure_lock_raw) _, which forwards here._ |
|  void | [**ratesync\_loop\_get\_state**](#function-ratesync_loop_get_state) (const [**ratesync\_loop\_t**](structratesync__loop__t.md) \* l, void \* blob) <br>_Serialize the loop's mutable state into_ `blob` _._ |
|  void | [**ratesync\_loop\_init**](#function-ratesync_loop_init) ([**ratesync\_loop\_t**](structratesync__loop__t.md) \* l, double sps, size\_t m, double bn, double zeta, int ted) <br>_Initialise a standalone timing loop._  |
|  void | [**ratesync\_loop\_reset**](#function-ratesync_loop_reset) ([**ratesync\_loop\_t**](structratesync__loop__t.md) \* l) <br>_Re-seed the loop: integrator, strobe ring, lock detector and the prime countdown. Configuration and cascade geometry are kept._  |
|  void | [**ratesync\_loop\_set\_cascade**](#function-ratesync_loop_set_cascade) ([**ratesync\_loop\_t**](structratesync__loop__t.md) \* l, double term\_rate, size\_t prime\_taps) <br>_Tell the loop the geometry of the accumulator it steers._  |
|  int | [**ratesync\_loop\_set\_state**](#function-ratesync_loop_set_state) ([**ratesync\_loop\_t**](structratesync__loop__t.md) \* l, const void \* blob) <br>_Restore the loop's mutable state from_ `blob` _._ |
|  int | [**ratesync\_loop\_set\_telemetry**](#function-ratesync_loop_set_telemetry) ([**ratesync\_loop\_t**](structratesync__loop__t.md) \* l, [**dp\_tlm\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_t) \* tlm, const char \* prefix, uint32\_t decim) <br>_Register the six timing probes; see_ [_**ratesync\_set\_telemetry()**_](ratesync__core_8h.md#function-ratesync_set_telemetry) _, which forwards here. NULL_`tlm` _detaches._ |
|  size\_t | [**ratesync\_loop\_state\_bytes**](#function-ratesync_loop_state_bytes) (const [**ratesync\_loop\_t**](structratesync__loop__t.md) \* l) <br>_Bytes_ [_**ratesync\_loop\_get\_state()**_](ratesync__core_8h.md#function-ratesync_loop_get_state) _writes (envelope + payload + the loop filter's child blob)._ |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) int | [**ratesync\_loop\_take\_output**](#function-ratesync_loop_take_output) ([**ratesync\_loop\_t**](structratesync__loop__t.md) \* s, float complex y, float complex \* y\_out, int ted) <br>_Fold one terminal-stage output into the timing loop._  |
|  void | [**ratesync\_loop\_tlm\_flush**](#function-ratesync_loop_tlm_flush) (const [**ratesync\_loop\_t**](structratesync__loop__t.md) \* l) <br>_Emit the timing loop's telemetry for the symbol just recovered._  |
|  void | [**ratesync\_reset**](#function-ratesync_reset) ([**ratesync\_state\_t**](structratesync__state__t.md) \* state) <br>_Reset to the post-create state: the cascade, the loop integrator, the lock detector, the strobe ring and the prime countdown._  |
|  void | [**ratesync\_set\_bn**](#function-ratesync_set_bn) ([**ratesync\_state\_t**](structratesync__state__t.md) \* state, double val) <br> |
|  int | [**ratesync\_set\_state**](#function-ratesync_set_state) ([**ratesync\_state\_t**](structratesync__state__t.md) \* state, const void \* blob) <br>_Restore mutable state from_ `blob` _into an identically built instance._ |
|  int | [**ratesync\_set\_telemetry**](#function-ratesync_set_telemetry) ([**ratesync\_state\_t**](structratesync__state__t.md) \* state, [**dp\_tlm\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_t) \* tlm, const char \* prefix, uint32\_t decim) <br>_Attach (or detach) a telemetry context and register the probes._  |
|  size\_t | [**ratesync\_state\_bytes**](#function-ratesync_state_bytes) (const [**ratesync\_state\_t**](structratesync__state__t.md) \* state) <br>_Bytes_ [_**ratesync\_get\_state()**_](ratesync__core_8h.md#function-ratesync_get_state) _writes (envelope + payload + child)._ |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) int | [**ratesync\_step**](#function-ratesync_step) ([**ratesync\_state\_t**](structratesync__state__t.md) \* s, float complex x, float complex \* y\_out) <br>_Per-input timing step (the inline composition API)._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) int | [**ratesync\_step\_ted**](#function-ratesync_step_ted) ([**ratesync\_state\_t**](structratesync__state__t.md) \* s, float complex x, float complex \* y\_out, int ted) <br>_Per-input timing step with the TED selection as a parameter._  |
|  size\_t | [**ratesync\_steps**](#function-ratesync_steps) ([**ratesync\_state\_t**](structratesync__state__t.md) \* state, const float complex \* x, size\_t x\_len, float complex \* out, size\_t max\_out) <br>_Recover symbols from a block of oversampled cf32 baseband._  |
|  size\_t | [**ratesync\_steps\_max\_out**](#function-ratesync_steps_max_out) ([**ratesync\_state\_t**](structratesync__state__t.md) \* state) <br>_Output-buffer hint for the generated binding; 0 means "the input
length is already a safe bound" — with_ `sps >= m >= 2` _a block can never yield more symbols than it has samples (mirrors symsync)._ |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**RATESYNC\_LOCK\_EPS**](ratesync__core_8h.md#define-ratesync_lock_eps)  `1e-12`<br> |
| define  | [**RATESYNC\_LOOP\_STATE\_MAGIC**](ratesync__core_8h.md#define-ratesync_loop_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('R', 'S', 'L', 'P')`<br> |
| define  | [**RATESYNC\_LOOP\_STATE\_VERSION**](ratesync__core_8h.md#define-ratesync_loop_state_version)  `/* multi line expression */`<br> |
| define  | [**RATESYNC\_MAX\_M**](ratesync__core_8h.md#define-ratesync_max_m)  `8`<br> |
| define  | [**RATESYNC\_STATE\_MAGIC**](ratesync__core_8h.md#define-ratesync_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('R', 'A', 'T', 'S')`<br> |
| define  | [**RATESYNC\_STATE\_VERSION**](ratesync__core_8h.md#define-ratesync_state_version)  `2u /\* v2: running state moved into the loop \*/`<br> |

## Detailed Description


RateSync owns a `RateConverter` whose **terminal stage carries the pulse** (`RateConverter_create_matched`) and closes a timing loop around that stage's control port. It builds no filters of its own: the matched filter IS the cascade's last dot product, and the polyphase arm that dot product selects IS the fractional timing delay. One filter, no Farrow, no separate matched-filter pass.


Where SymbolSync separates the jobs (a matched FIR, then a Farrow interpolator steered by a timing NCO), this fuses them — and because the cascade in front is a full `RateConverter`, the fusion inherits its planning: HB/CIC stages do the bulk decimation for free, so the matched filter is sized by the POST-decimation rate. A matched filter at 256 input samples per symbol costs the same bank as one at 4.


**Arbitrary rate, by construction.** `sps` is a double — 4, 17.33389, an irrational ratio, or a slowly drifting clock — because the terminal stage's accumulator is a double and the loop only has to steer the strobe. That is the real-world case whenever the ADC clock is free-running against the symbol clock.


### Two things this object gets right that are easy to get wrong



**1. The TED normaliser is `|on|^2 + |mid|^2`, never `|on|^2`.** The on-time energy vanishes exactly when the strobe sits on the symbol transitions — which is precisely the state the loop must recover FROM — so normalising by it alone divides by zero at the worst possible moment. Measured: the error reaches -91, the control drives the terminal stage's effective rate NEGATIVE, its accumulator stops advancing, and the cascade emits nothing ever again (2 symbols where 4000 were expected — a permanent death, not a transient). The two energies are the same signal half a symbol apart, so their sum is bounded away from zero at every timing phase; the lock statistic already needs both. This is why RateSync needs no clamp on the control anywhere: with a normaliser that cannot vanish, there is no runaway to clamp.


**2. The loop stays open until the cascade is primed.** A cascade's first outputs are its delay lines filling, not signal (the eye statistic swings over its whole +-2 range through them). Steering on them is meaningless and was worth one lost acquisition in sixteen. [**ratesync\_create()**](ratesync__core_8h.md#function-ratesync_create) computes the prime length from the terminal bank's own geometry.



### The T/2 role ambiguity resolves itself



Gardner needs an on-time strobe and a transition gate half a symbol earlier. Running at `rate = m/sps` and taking every m-th output as on-time makes that a parity count, which looks like it should be ambiguous — and a half-symbol error is indeed an equilibrium of the detector. It is an **unstable** one: measured over a fine sweep, each parity's S-curve has exactly two zeros per symbol, one at the eye centre with negative slope (stable) and one at the T/2 point with positive slope (unstable). The loop runs away from the wrong one on its own, so **the parity does not matter** and no eye-sign detector or counter flip is needed. (An earlier prototype ran two displaced banks to pin the roles structurally; measurement showed that buys nothing and costs double the multiplies.)



### Measured



RRC-BPSK, noiseless, eight initial timing offsets each, `bn = 0.01`:



|sps   |planned cascade   |lock   |EVM    |
|-----|-----|-----|-----|
|4   |HB + Resampler(1,rrc)   |8/8   |-40.1 dB    |
|17.333   |CIC(8) + Resampler(0.923,rrc)   |8/8   |-37.4 dB    |
|64   |CIC(32) + Resampler(1,rrc)   |8/8   |-37.3 dB   |






`bn` behaves identically across all three (within ~2 dB at every setting), which is the point of referencing the control to the terminal rate: the loop bandwidth means the same thing whatever the planner decided to do in front of it. `bn = 0.005` measured best here (-46 / -40 / -40 dB); lower settings acquire too slowly to have settled within the test length.


Lifecycle: `create -> (step / steps / reset)* -> destroy`



```C++
ratesync_state_t *rx = ratesync_create (17.33389, RATESYNC_PULSE_RRC, 0.35,
                                        8, 2, 1024, 0.01, 0.707,
                                        RATESYNC_TED_GARDNER);
float complex sym;
if (ratesync_step (rx, x, &sym))
  consume (sym);
ratesync_destroy (rx);
```
 



    
## Public Types Documentation




### enum ratesync\_\_core\_8h\_1a61dadd085c1777f559549e05962b2c9e 

_Timing-error-detector selection for ratesync\_state\_t::ted._ 
```C++
enum ratesync__core_8h_1a61dadd085c1777f559549e05962b2c9e {
    RATESYNC_TED_GARDNER = 0,
    RATESYNC_TED_DTTL = 1
};
```




<hr>



### enum ratesync\_\_core\_8h\_1a726ca809ffd3d67ab4b8476646f26635 

_Matched-filter pulse shape._ 
```C++
enum ratesync__core_8h_1a726ca809ffd3d67ab4b8476646f26635 {
    RATESYNC_PULSE_IANDD = RC_PULSE_IANDD,
    RATESYNC_PULSE_RRC = RC_PULSE_RRC
};
```



Aliases of the cascade's own vocabulary (`rc_pulse_t`) so one set of names covers the family; the pulse is built by the RateConverter, which is the only party that knows its own CIC geometry. 


        

<hr>
## Public Functions Documentation




### function ratesync\_configure 

_Recompute the loop gains for a new bandwidth/damping, keeping the timing estimate._ 
```C++
void ratesync_configure (
    ratesync_state_t * state,
    double bn,
    double zeta
) 
```



Only the PI coefficients change; the integrator, and therefore the tracked rate and the lock, carries through untouched. Use it to narrow the loop after acquisition (a wide `bn` pulls in fast, a narrow one tracks with less jitter) without forcing a re-acquire.




**Parameters:**


* `state` Must be non-NULL. 
* `bn` Loop noise bandwidth, normalised to the symbol rate. 
* `zeta` Damping factor (0.707 = critically damped). 
```C++
>>> import numpy as np
>>> from doppler.track import RateSync
>>> syms = np.where(np.random.default_rng(3).integers(0, 2, 3000) > 0,
...                 1.0, -1.0)
>>> x = (0.25 * np.repeat(syms, 8)).astype(np.complex64)  # 8 samp/sym
>>> rs = RateSync(sps=8.0, pulse="iandd", m=4, bn=0.01)
>>> _ = rs.steps(x)              # acquire and lock
>>> rs.locked
True
>>> rs.configure(0.002, 0.707)   # narrow the loop; lock is kept
>>> round(rs.bn, 3)
0.002
>>> rs.locked
True
```
 




        

<hr>



### function ratesync\_configure\_lock\_raw 

_Set the lock detector's geometry directly._ 
```C++
void ratesync_configure_lock_raw (
    ratesync_state_t * state,
    size_t avgs,
    double up_thresh,
    double down_thresh,
    uint32_t n_up,
    uint32_t n_down
) 
```



The block size (avgs), a split declare/drop threshold pair on lock\_stat (level hysteresis) and both verify counts (time hysteresis). Re-tuning clears the in-flight block sum and drops the lock, so the next decision uses only looks gathered under the new config.




**Parameters:**


* `state` Must be non-NULL. 
* `avgs` Looks per decision; clamped &gt;= 1. 
* `up_thresh` Declare threshold on lock\_stat. 
* `down_thresh` Drop threshold; &lt;= up\_thresh for level hysteresis. 
* `n_up` Consecutive above-threshold decisions to declare. 
* `n_down` Consecutive below-threshold decisions to drop. 
```C++
>>> import numpy as np
>>> from doppler.track import RateSync
>>> syms = np.where(np.random.default_rng(3).integers(0, 2, 3000) > 0,
...                 1.0, -1.0)
>>> x = (0.25 * np.repeat(syms, 8)).astype(np.complex64)
>>> rs = RateSync(sps=8.0, pulse="iandd", m=4, bn=0.01)
>>> _ = rs.steps(x)
>>> rs.locked
True
>>> rs.configure_lock_raw(64, 0.5, 0.4, 2, 4)  # drops the lock
>>> rs.locked
False
>>> rs.lock_stat                 # the in-flight block was cleared
0.0
```
 




        

<hr>



### function ratesync\_create 

_Create a RateSync instance._ 
```C++
ratesync_state_t * ratesync_create (
    double sps,
    int pulse,
    double beta,
    size_t span,
    size_t m,
    size_t num_phases,
    double bn,
    double zeta,
    int ted
) 
```



Builds a `RateConverter(rate = m/sps, pulse, ..., pulse_sps = m)` with CIC droop compensation on — folded into the bank, so it costs six taps per arm and no extra stage, and is worth ~28 dB of EVM on any cascade that plans a CIC. See [**RateConverter\_create\_matched()**](RateConverter__core_8h.md#function-rateconverter_create_matched).




**
**

Present **unit-amplitude symbols**. This object carries no AGC, and that is deliberate: a receiver composing it already levels in its own front-end cascade ([**RateConverter\_enable\_agc()**](RateConverter__core_8h.md#function-rateconverter_enable_agc), one per receiver), so an AGC here would be a second one integrating against the first. The level to hit is not a tuned number — the TED normalises by its own construct-time slope, and that slope is computed for the reference the bank already defines, `10*log10(bank_e0 / bank_sps)`, which is ~0 dB because the bank normalises by its own pulse energy. See [**RateConverter\_agc\_ref\_db()**](RateConverter__core_8h.md#function-rateconverter_agc_ref_db), which is defined for any matched cascade whether or not an AGC is enabled.


Under-driving costs EVM with nothing to reveal it: at `sps = 17.333`, quarter-amplitude input measures -21.6 dB EVM against -37.0 dB at unit amplitude — 15 dB — with `lock_stat` 0.70 either way, because the loop really does lock and only the demodulation degrades. Over-driving is the other end of the same axis and IS reported, by [**ratesync\_get\_clipped()**](ratesync__core_8h.md#function-ratesync_get_clipped): a CIC bounds its input to +-1.0. There is no under-drive twin of that flag — tracked as gh-661.




**Parameters:**


* `sps` Nominal samples per symbol; any double &gt;= `m` (17.33389 is as valid as 4). The bound is `m`, not 2, because the terminal stage must not be asked to interpolate: `rate = m/sps <= 1`. 
* `pulse` RATESYNC\_PULSE\_IANDD (rectangular/NRZ) or \_RRC. 
* `beta` RRC roll-off in `[0, 1]` (ignored for the rectangle). 
* `span` One-sided RRC span in symbols (ignored for the rectangle, whose support is always one symbol). 
* `m` Terminal outputs per symbol: even, `2 <= m <= RATESYNC_MAX_M`. Gardner needs the half-symbol gate, so m must be even and at least 2. The oversampled stream is a by-product, not an extra cost. **Use m &gt;= 4 with RATESYNC\_PULSE\_IANDD**: the rectangle is one symbol wide, so at m = 2 its matched filter is a two-tap sum and the eye barely opens (measured lock\_stat -0.34 at m = 2 against +0.95 at m = 4 on the same NRZ stream). The RRC spans many symbols and is unaffected. 
* `num_phases` Matched-filter arms; power of two (1024 is a good default). Sets the fractional-timing resolution to `1/num_phases` of an output period. 
* `bn` Loop noise bandwidth, normalised to the symbol rate. 
* `zeta` Damping factor (0.707 = critically damped). 
* `ted` RATESYNC\_TED\_GARDNER (blind) or RATESYNC\_TED\_DTTL (decision-directed; BPSK/QPSK only). 



**Returns:**

Heap-allocated state, or NULL if a parameter is out of range or allocation fails. 




**Note:**

Caller must call [**ratesync\_destroy()**](ratesync__core_8h.md#function-ratesync_destroy) when done. 





        

<hr>



### function ratesync\_destroy 

_Destroy a RateSync instance and release all memory._ 
```C++
void ratesync_destroy (
    ratesync_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function ratesync\_get\_bn 

```C++
double ratesync_get_bn (
    const ratesync_state_t * state
) 
```




<hr>



### function ratesync\_get\_clipped 

_Has the cascade's CIC stage clipped its input since the last reset? Forwarded from the RateConverter: a CIC bounds its input to +-1.0 and clips silently past that, which no timing metric reveals. Always 0 when the plan has no CIC stage._ 
```C++
int ratesync_get_clipped (
    const ratesync_state_t * state
) 
```




<hr>



### function ratesync\_get\_ctrl 

_Current per-input control deviation steering the strobe._ 
```C++
double ratesync_get_ctrl (
    const ratesync_state_t * state
) 
```




<hr>



### function ratesync\_get\_lock\_stat 

_Last block-averaged lock statistic (the eye-opening ratio)._ 
```C++
double ratesync_get_lock_stat (
    const ratesync_state_t * state
) 
```



This, not an error-vector magnitude, is the honest lock indicator: a single cycle slip during acquisition drags a windowed EVM by 20 dB while the eye stays wide open at +0.75. Judge lock here. 


        

<hr>



### function ratesync\_get\_locked 

_Current lock decision (1 = locked), verify-counted._ 
```C++
int ratesync_get_locked (
    const ratesync_state_t * state
) 
```




<hr>



### function ratesync\_get\_rate 

_Smoothed tracked samples per symbol. Departs from the nominal_ `sps` _by exactly the sample-clock offset being tracked, so it is the estimator a rate-disciplining caller reads._
```C++
double ratesync_get_rate (
    const ratesync_state_t * state
) 
```




<hr>



### function ratesync\_get\_state 

_Serialize the mutable state into_ `blob` _._
```C++
void ratesync_get_state (
    const ratesync_state_t * state,
    void * blob
) 
```




<hr>



### function ratesync\_get\_timing\_error 

_Last normalised TED error — the loop stress._ 
```C++
double ratesync_get_timing_error (
    const ratesync_state_t * state
) 
```




<hr>



### function ratesync\_loop\_bind\_cascade 

_Read that geometry straight off a cascade._ 
```C++
void ratesync_loop_bind_cascade (
    ratesync_loop_t * l,
    const RateConverter_state_t * rc
) 
```



Walks `rc` to its terminal stage and forwards its rate and tap count to [**ratesync\_loop\_set\_cascade()**](ratesync__core_8h.md#function-ratesync_loop_set_cascade). Every owner of this loop owns a `RateConverter` somewhere — RateSync directly, the receivers inside their DDC — so the walk lives here once rather than in each of them.




**Parameters:**


* `l` Must be non-NULL. 
* `rc` The cascade whose terminal stage the loop steers. 




        

<hr>



### function ratesync\_loop\_configure 

_Retune the loop; preserves the integrator (and so the lock)._ 
```C++
void ratesync_loop_configure (
    ratesync_loop_t * l,
    double bn,
    double zeta
) 
```




<hr>



### function ratesync\_loop\_configure\_lock\_raw 

_Set the lock detector's geometry; see_ [_**ratesync\_configure\_lock\_raw()**_](ratesync__core_8h.md#function-ratesync_configure_lock_raw) _, which forwards here._
```C++
void ratesync_loop_configure_lock_raw (
    ratesync_loop_t * l,
    size_t avgs,
    double up_thresh,
    double down_thresh,
    uint32_t n_up,
    uint32_t n_down
) 
```




<hr>



### function ratesync\_loop\_get\_state 

_Serialize the loop's mutable state into_ `blob` _._
```C++
void ratesync_loop_get_state (
    const ratesync_loop_t * l,
    void * blob
) 
```




<hr>



### function ratesync\_loop\_init 

_Initialise a standalone timing loop._ 
```C++
void ratesync_loop_init (
    ratesync_loop_t * l,
    double sps,
    size_t m,
    double bn,
    double zeta,
    int ted
) 
```



Sets the loop filter (update period = one symbol, so `bn` is normalised to the symbol rate) and the default lock-detector geometry, then seeds every running field. The caller must still describe the accumulator being steered with [**ratesync\_loop\_set\_cascade()**](ratesync__core_8h.md#function-ratesync_loop_set_cascade) before pushing outputs through.




**Parameters:**


* `l` Loop to initialise. Must be non-NULL. 
* `sps` Nominal samples per symbol (any double). 
* `m` Terminal outputs per symbol; even, 2..RATESYNC\_MAX\_M. 
* `bn` Loop noise bandwidth, normalised to the symbol rate. 
* `zeta` Damping factor. 
* `ted` RATESYNC\_TED\_GARDNER or RATESYNC\_TED\_DTTL. 




        

<hr>



### function ratesync\_loop\_reset 

_Re-seed the loop: integrator, strobe ring, lock detector and the prime countdown. Configuration and cascade geometry are kept._ 
```C++
void ratesync_loop_reset (
    ratesync_loop_t * l
) 
```





**Parameters:**


* `l` Must be non-NULL. 




        

<hr>



### function ratesync\_loop\_set\_cascade 

_Tell the loop the geometry of the accumulator it steers._ 
```C++
void ratesync_loop_set_cascade (
    ratesync_loop_t * l,
    double term_rate,
    size_t prime_taps
) 
```





**Parameters:**


* `l` Loop. Must be non-NULL. 
* `term_rate` The terminal stage's own rate. `ctrl` is referenced to this, not to the overall cascade rate — they differ by the whole integer decimation in front, which would under-drive the loop by exactly that factor. 
* `prime_taps` The terminal bank's tap count; the loop discards `prime_taps + 1` outputs before closing, because those are the delay lines filling rather than signal. 




        

<hr>



### function ratesync\_loop\_set\_state 

_Restore the loop's mutable state from_ `blob` _._
```C++
int ratesync_loop_set_state (
    ratesync_loop_t * l,
    const void * blob
) 
```





**Returns:**

DP\_OK, or DP\_ERR\_INVALID if any envelope rejects. 





        

<hr>



### function ratesync\_loop\_set\_telemetry 

_Register the six timing probes; see_ [_**ratesync\_set\_telemetry()**_](ratesync__core_8h.md#function-ratesync_set_telemetry) _, which forwards here. NULL_`tlm` _detaches._
```C++
int ratesync_loop_set_telemetry (
    ratesync_loop_t * l,
    dp_tlm_t * tlm,
    const char * prefix,
    uint32_t decim
) 
```




<hr>



### function ratesync\_loop\_state\_bytes 

_Bytes_ [_**ratesync\_loop\_get\_state()**_](ratesync__core_8h.md#function-ratesync_loop_get_state) _writes (envelope + payload + the loop filter's child blob)._
```C++
size_t ratesync_loop_state_bytes (
    const ratesync_loop_t * l
) 
```




<hr>



### function ratesync\_loop\_take\_output 

_Fold one terminal-stage output into the timing loop._ 
```C++
JM_FORCEINLINE  JM_HOT int ratesync_loop_take_output (
    ratesync_loop_t * s,
    float complex y,
    float complex * y_out,
    int ted
) 
```



The whole of the loop's per-output work, and the reason the loop is a struct of its own: it never touches the cascade, so a receiver that owns its cascade inside a DDC drives this with exactly the same call RateSync makes.




**Parameters:**


* `s` Loop state. Must be non-NULL. 
* `y` One terminal-stage output. 
* `y_out` Receives the symbol when the return is 1. 
* `ted` RATESYNC\_TED\_GARDNER or RATESYNC\_TED\_DTTL — pass a literal for a specialised (branch-free) instantiation. 



**Returns:**

1 if this output was an on-time strobe that produced a symbol. 





        

<hr>



### function ratesync\_loop\_tlm\_flush 

_Emit the timing loop's telemetry for the symbol just recovered._ 
```C++
void ratesync_loop_tlm_flush (
    const ratesync_loop_t * l
) 
```



Out-of-line on purpose: the emit machinery must not inline into the per-sample hot loop (the same body-growth cost symsync measured). Callers gate on `l->tlm.ctx`, so the detached cost is one predicted-not-taken branch per symbol.




**Parameters:**


* `l` Loop with a non-NULL tlm.ctx (caller-checked). 




        

<hr>



### function ratesync\_reset 

_Reset to the post-create state: the cascade, the loop integrator, the lock detector, the strobe ring and the prime countdown._ 
```C++
void ratesync_reset (
    ratesync_state_t * state
) 
```



Configuration (sps, pulse, bank, bn, zeta, ted, lock geometry) is kept; only the running state is cleared, so a re-run of the same stream from a reset object reproduces its first-run symbols bit for bit.




**Parameters:**


* `state` Must be non-NULL. 
```C++
>>> import numpy as np
>>> from doppler.track import RateSync
>>> syms = np.where(np.random.default_rng(3).integers(0, 2, 3000) > 0,
...                 1.0, -1.0)
>>> x = (0.25 * np.repeat(syms, 8)).astype(np.complex64)
>>> rs = RateSync(sps=8.0, pulse="iandd", m=4, bn=0.01)
>>> first = np.array(rs.steps(x))
>>> rs.reset()
>>> rs.ctrl, rs.locked           # back to the post-create state
(0.0, False)
>>> bool(np.array_equal(first, np.array(rs.steps(x))))  # reproducible
True
```
 




        

<hr>



### function ratesync\_set\_bn 

```C++
void ratesync_set_bn (
    ratesync_state_t * state,
    double val
) 
```




<hr>



### function ratesync\_set\_state 

_Restore mutable state from_ `blob` _into an identically built instance._
```C++
int ratesync_set_state (
    ratesync_state_t * state,
    const void * blob
) 
```





**Returns:**

DP\_OK, or DP\_ERR\_INVALID if any envelope rejects. 





        

<hr>



### function ratesync\_set\_telemetry 

_Attach (or detach) a telemetry context and register the probes._ 
```C++
int ratesync_set_telemetry (
    ratesync_state_t * state,
    dp_tlm_t * tlm,
    const char * prefix,
    uint32_t decim
) 
```



Registers six probes, emitted once per recovered symbol and further thinned by `decim:` "&lt;prefix&gt;.e" (normalised TED error), "&lt;prefix&gt;.ctrl" (the per-input control steering the strobe), "&lt;prefix&gt;.rate" (tracked samples/symbol), "&lt;prefix&gt;.lock" (last block-averaged lock\_signal), "&lt;prefix&gt;.locked" (0/1) and "&lt;prefix&gt;.mu" (the timing NCO's fractional phase — see [**resamp\_get\_ctrl\_acc()**](resamp__core_8h.md#function-resamp_get_ctrl_acc)). Passing NULL detaches. Setup path, never hot: the context is borrowed and must outlive the attachment (SPSC rules in [**dp\_tlm/dp\_tlm\_core.h**](dp__tlm__core_8h.md)).


The three form one readable picture of the loop: `e` is what the detector saw, `ctrl` is what the filter did about it, and `mu` is where the sampling instant ended up as a result — the only one of the three that is a physical position rather than a correction.




**Parameters:**


* `state` Must be non-NULL. 
* `tlm` Telemetry context to attach, or NULL to detach. 
* `prefix` Probe-name prefix, e.g. "sync". 
* `decim` Emit every decim-th symbol; &gt;= 1. 



**Returns:**

DP\_OK, or DP\_ERR\_INVALID when the probe table cannot take all six probes (the attach fails whole; the object stays detached). 
```C++
>>> from doppler.track import RateSync
>>> from doppler.telemetry import Telemetry
>>> tlm = Telemetry(1 << 14)
>>> rs = RateSync(sps=8.0, pulse="iandd", m=4, bn=0.01)
>>> rs.set_telemetry(tlm, "sync")   # register the six timing probes
>>> tlm.probe_count
6
>>> "sync.rate" in tlm.probe_names   # tracked samples/symbol
True
```
 





        

<hr>



### function ratesync\_state\_bytes 

_Bytes_ [_**ratesync\_get\_state()**_](ratesync__core_8h.md#function-ratesync_get_state) _writes (envelope + payload + child)._
```C++
size_t ratesync_state_bytes (
    const ratesync_state_t * state
) 
```




<hr>



### function ratesync\_step 

_Per-input timing step (the inline composition API)._ 
```C++
JM_FORCEINLINE  JM_HOT int ratesync_step (
    ratesync_state_t * s,
    float complex x,
    float complex * y_out
) 
```



The public form of [**ratesync\_step\_ted()**](ratesync__core_8h.md#function-ratesync_step_ted): dispatches on the configured detector and flushes telemetry when attached.




**Parameters:**


* `s` State. Must be non-NULL. 
* `x` One input sample. 
* `y_out` Receives the symbol when the return is 1. 



**Returns:**

1 if a symbol was emitted (into `y_out`), 0 otherwise. 





        

<hr>



### function ratesync\_step\_ted 

_Per-input timing step with the TED selection as a parameter._ 
```C++
JM_FORCEINLINE  JM_HOT int ratesync_step_ted (
    ratesync_state_t * s,
    float complex x,
    float complex * y_out,
    int ted
) 
```



The workhorse behind [**ratesync\_step()**](ratesync__core_8h.md#function-ratesync_step)/ratesync\_steps(). Pushes one input through the cascade at the current control deviation. `rate = m/sps <= 1` so the terminal stage emits at most one output per input; every m-th output is an on-time strobe, and the output m/2 back is the transition gate. On a strobe the TED compares the two, the PI loop steers the next control, and the on-time sample is the recovered symbol.


Passing a literal `ted` lets the force-inlined body constant-fold the detector branch away, exactly as [**symsync\_step\_ted()**](symsync__core_8h.md#function-symsync_step_ted) does.




**Parameters:**


* `s` State. Must be non-NULL. 
* `x` One input sample. 
* `y_out` Receives the symbol when the return is 1. 
* `ted` RATESYNC\_TED\_GARDNER or RATESYNC\_TED\_DTTL — pass a literal for a specialised (branch-free) instantiation. 



**Returns:**

1 if a symbol was emitted (into `y_out`), 0 otherwise. 





        

<hr>



### function ratesync\_steps 

_Recover symbols from a block of oversampled cf32 baseband._ 
```C++
size_t ratesync_steps (
    ratesync_state_t * state,
    const float complex * x,
    size_t x_len,
    float complex * out,
    size_t max_out
) 
```



[**ratesync\_step()**](ratesync__core_8h.md#function-ratesync_step) in a loop, with the TED specialised per detector; state carries across calls, so contiguous blocks give the same symbols as one large block.




**Parameters:**


* `state` Must be non-NULL. 
* `x` Input samples. 
* `x_len` Number of inputs. 
* `out` Recovered symbols. 
* `max_out` Capacity of `out`. 



**Returns:**

Symbols written to `out`. 
```C++
>>> import numpy as np
>>> from doppler.track import RateSync
>>> syms = np.where(np.random.default_rng(3).integers(0, 2, 3000) > 0,
...                 1.0, -1.0)
>>> x = (0.25 * np.repeat(syms, 8)).astype(np.complex64)  # 8 samp/sym
>>> rs = RateSync(sps=8.0, pulse="iandd", m=4, bn=0.01)
>>> y = rs.steps(x)             # one symbol per transmitted symbol
>>> round(rs.rate, 2)           # tracked samples per symbol
8.0
>>> bool(rs.lock_stat > 0.55)   # the timing loop has locked
True
```
 





        

<hr>



### function ratesync\_steps\_max\_out 

_Output-buffer hint for the generated binding; 0 means "the input
length is already a safe bound" — with_ `sps >= m >= 2` _a block can never yield more symbols than it has samples (mirrors symsync)._
```C++
size_t ratesync_steps_max_out (
    ratesync_state_t * state
) 
```




<hr>
## Macro Definition Documentation





### define RATESYNC\_LOCK\_EPS 

```C++
#define RATESYNC_LOCK_EPS `1e-12`
```




<hr>



### define RATESYNC\_LOOP\_STATE\_MAGIC 

```C++
#define RATESYNC_LOOP_STATE_MAGIC `DP_FOURCC ('R', 'S', 'L', 'P')`
```




<hr>



### define RATESYNC\_LOOP\_STATE\_VERSION 

```C++
#define RATESYNC_LOOP_STATE_VERSION `/* multi line expression */`
```




<hr>



### define RATESYNC\_MAX\_M 

```C++
#define RATESYNC_MAX_M `8`
```



Largest supported outputs-per-symbol (bounds the strobe ring in-struct). 


        

<hr>



### define RATESYNC\_STATE\_MAGIC 

```C++
#define RATESYNC_STATE_MAGIC `DP_FOURCC ('R', 'A', 'T', 'S')`
```




<hr>



### define RATESYNC\_STATE\_VERSION 

```C++
#define RATESYNC_STATE_VERSION `2u /* v2: running state moved into the loop */`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/ratesync/ratesync_core.h`

