

# File ratesync\_core.h



[**FileList**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**ratesync**](dir_6debd7d9ad90888bb5b8a40a122d95e8.md) **>** [**ratesync\_core.h**](ratesync__core_8h.md)

[Go to the source code of this file](ratesync__core_8h_source.md)

_RateSync — symbol-timing recovery on a matched-filter rate cascade._ [More...](#detailed-description)

* `#include "doppler/RateConverter/RateConverter_core.h"`
* `#include "doppler/cic/cic_core.h"`
* `#include "doppler/clib_common.h"`
* `#include "doppler/dp_state.h"`
* `#include "doppler/fir/fir_core.h"`
* `#include "doppler/hbdecim/hbdecim_core.h"`
* `#include "doppler/jm_perf.h"`
* `#include "doppler/lockdet/lockdet_core.h"`
* `#include "doppler/loop_filter/loop_filter_core.h"`
* `#include "doppler/resamp/resamp_core.h"`
* `#include "doppler/resample/resample_core.h"`
* `#include "doppler/timing_est/timing_est_core.h"`
* `#include "doppler/symsync/symsync_core.h"`
* `#include "doppler/dp_tlm/dp_tlm_core.h"`
* `#include "doppler/telemetry/telemetry_core.h"`
* `#include "doppler/ber/ber_core.h"`
* `#include "doppler/pn/pn_core.h"`
* `#include "doppler/nco/nco_core.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**dp\_ratesync\_state\_t**](structdp__ratesync__state__t.md) <br>_RateSync state: a matched-filter cascade and the timing loop._  |
| struct | [**ratesync\_loop\_t**](structratesync__loop__t.md) <br>_The symbol-timing loop, independent of what feeds it._  |
| struct | [**ratesync\_tlm\_t**](structratesync__tlm__t.md) <br>_Telemetry attachment: a borrowed context + this object's probe ids. NULL ctx (the default) means detached — every probe site is then one predicted-not-taken branch per recovered symbol._  |


## Public Types

| Type | Name |
| ---: | :--- |
| enum  | [**ratesync\_\_core\_8h\_1a0411cd49bb5b71852cecd93bcbf0ca2d**](#enum-ratesync__core_8h_1a0411cd49bb5b71852cecd93bcbf0ca2d)  <br>_Matched-filter pulse shape._  |
| enum  | [**ratesync\_\_core\_8h\_1a726ca809ffd3d67ab4b8476646f26635**](#enum-ratesync__core_8h_1a726ca809ffd3d67ab4b8476646f26635)  <br>_Timing-error-detector selection for dp\_ratesync\_state\_t::ted._  |
| enum  | [**ratesync\_lifecycle\_t**](#enum-ratesync_lifecycle_t)  <br> |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  void | [**dp\_ratesync\_acquire\_push**](#function-dp_ratesync_acquire_push) ([**dp\_ratesync\_state\_t**](structdp__ratesync__state__t.md) \* state, float \_Complex x) <br>_Hand one input to the estimator while the loop is not tracking, and seed the loop from the block that passes._  |
|  int | [**dp\_ratesync\_configure**](#function-dp_ratesync_configure) ([**dp\_ratesync\_state\_t**](structdp__ratesync__state__t.md) \* state, double bn, double zeta) <br>_Recompute the loop gains for a new bandwidth/damping, keeping the timing estimate._  |
|  int | [**dp\_ratesync\_configure\_acquisition**](#function-dp_ratesync_configure_acquisition) ([**dp\_ratesync\_state\_t**](structdp__ratesync__state__t.md) \* state, size\_t symbols, double quality) <br>_Turn the built-in acquisition on, or off._  |
|  void | [**dp\_ratesync\_configure\_lock\_raw**](#function-dp_ratesync_configure_lock_raw) ([**dp\_ratesync\_state\_t**](structdp__ratesync__state__t.md) \* state, size\_t avgs, double up\_thresh, double down\_thresh, uint32\_t n\_up, uint32\_t n\_down) <br>_Set the lock detector's geometry directly._  |
|  [**dp\_ratesync\_state\_t**](structdp__ratesync__state__t.md) \* | [**dp\_ratesync\_create**](#function-dp_ratesync_create) (double sps, int pulse, double beta, size\_t span, size\_t m, size\_t num\_phases, double bn, double zeta, int ted) <br>_Create a RateSync instance._  |
|  void | [**dp\_ratesync\_destroy**](#function-dp_ratesync_destroy) ([**dp\_ratesync\_state\_t**](structdp__ratesync__state__t.md) \* state) <br>_Destroy a RateSync instance and release all memory._  |
|  double | [**dp\_ratesync\_get\_bn**](#function-dp_ratesync_get_bn) (const [**dp\_ratesync\_state\_t**](structdp__ratesync__state__t.md) \* state) <br>_Timing-loop noise bandwidth, normalised to the symbol rate. Writing it re-derives the loop gains at the damping already in use, exactly as configure() does._  |
|  int | [**dp\_ratesync\_get\_clipped**](#function-dp_ratesync_get_clipped) (const [**dp\_ratesync\_state\_t**](structdp__ratesync__state__t.md) \* state) <br>_Has the cascade's CIC stage clipped its input since the last reset? Forwarded from the RateConverter: a CIC bounds its input to its full scale (CIC\_PAPR\_WIDE as built) and clips silently at and past that, which no timing metric reveals. Always 0 when the plan has no CIC stage._  |
|  double | [**dp\_ratesync\_get\_ctrl**](#function-dp_ratesync_get_ctrl) (const [**dp\_ratesync\_state\_t**](structdp__ratesync__state__t.md) \* state) <br>_Current per-input control deviation steering the strobe._  |
|  int | [**dp\_ratesync\_get\_lifecycle**](#function-dp_ratesync_get_lifecycle) (const [**dp\_ratesync\_state\_t**](structdp__ratesync__state__t.md) \* state) <br>_Where the timing loop is: acquiring, tracking or coasting._  |
|  double | [**dp\_ratesync\_get\_lock\_stat**](#function-dp_ratesync_get_lock_stat) (const [**dp\_ratesync\_state\_t**](structdp__ratesync__state__t.md) \* state) <br>_Last block-averaged lock statistic (the eye-opening ratio)._  |
|  int | [**dp\_ratesync\_get\_locked**](#function-dp_ratesync_get_locked) (const [**dp\_ratesync\_state\_t**](structdp__ratesync__state__t.md) \* state) <br>_Current lock decision (1 = locked), verify-counted._  |
|  int | [**dp\_ratesync\_get\_presence**](#function-dp_ratesync_get_presence) (const [**dp\_ratesync\_state\_t**](structdp__ratesync__state__t.md) \* state) <br>_The presence input as it stands._  |
|  double | [**dp\_ratesync\_get\_rate**](#function-dp_ratesync_get_rate) (const [**dp\_ratesync\_state\_t**](structdp__ratesync__state__t.md) \* state) <br>_Smoothed tracked samples per symbol. Departs from the nominal_ `sps` _by exactly the sample-clock offset being tracked, so it is the estimator a rate-disciplining caller reads._ |
|  double | [**dp\_ratesync\_get\_settle\_time**](#function-dp_ratesync_get_settle_time) (const [**dp\_ratesync\_state\_t**](structdp__ratesync__state__t.md) \* state) <br>_Symbols emitted before the timing has settled: where a steady-state measurement of the output may start._  |
|  void | [**dp\_ratesync\_get\_state**](#function-dp_ratesync_get_state) (const [**dp\_ratesync\_state\_t**](structdp__ratesync__state__t.md) \* state, void \* blob) <br>_Serialize the mutable state into_ `blob` _._ |
|  double | [**dp\_ratesync\_get\_timing\_error**](#function-dp_ratesync_get_timing_error) (const [**dp\_ratesync\_state\_t**](structdp__ratesync__state__t.md) \* state) <br>_Last normalised TED error — the loop stress._  |
|  int | [**dp\_ratesync\_get\_valid**](#function-dp_ratesync_get_valid) (const [**dp\_ratesync\_state\_t**](structdp__ratesync__state__t.md) \* state) <br>_Is the symbol just emitted valid?_  |
|  void | [**dp\_ratesync\_loop\_bind\_cascade**](#function-dp_ratesync_loop_bind_cascade) ([**ratesync\_loop\_t**](structratesync__loop__t.md) \* l, const [**dp\_RateConverter\_state\_t**](structdp__RateConverter__state__t.md) \* rc) <br>_Read that geometry straight off a cascade._  |
|  int | [**dp\_ratesync\_loop\_configure**](#function-dp_ratesync_loop_configure) ([**ratesync\_loop\_t**](structratesync__loop__t.md) \* l, double bn, double zeta) <br>_Retune the loop; preserves the integrator (and so the lock)._  |
|  void | [**dp\_ratesync\_loop\_configure\_lock\_raw**](#function-dp_ratesync_loop_configure_lock_raw) ([**ratesync\_loop\_t**](structratesync__loop__t.md) \* l, size\_t avgs, double up\_thresh, double down\_thresh, uint32\_t n\_up, uint32\_t n\_down) <br>_Set the lock detector's geometry; see_ [_**dp\_ratesync\_configure\_lock\_raw()**_](ratesync__core_8h.md#function-dp_ratesync_configure_lock_raw) _, which forwards here._ |
|  void | [**dp\_ratesync\_loop\_decide**](#function-dp_ratesync_loop_decide) ([**ratesync\_loop\_t**](structratesync__loop__t.md) \* l, int was, int now) <br>_A lock decision's consequence for the state; called from_ [_**ratesync\_loop\_take\_output()**_](ratesync__core_8h.md#function-ratesync_loop_take_output) _on a loop whose owner acquires._ |
|  void | [**dp\_ratesync\_loop\_get\_state**](#function-dp_ratesync_loop_get_state) (const [**ratesync\_loop\_t**](structratesync__loop__t.md) \* l, void \* blob) <br>_Serialize the loop's mutable state into_ `blob` _._ |
|  void | [**dp\_ratesync\_loop\_init**](#function-dp_ratesync_loop_init) ([**ratesync\_loop\_t**](structratesync__loop__t.md) \* l, double sps, size\_t m, double bn, double zeta, int ted) <br>_Initialise a standalone timing loop._  |
|  void | [**dp\_ratesync\_loop\_reset**](#function-dp_ratesync_loop_reset) ([**ratesync\_loop\_t**](structratesync__loop__t.md) \* l) <br>_Re-seed the loop: integrator, strobe ring, lock detector and the prime countdown. Configuration and cascade geometry are kept._  |
|  int | [**dp\_ratesync\_loop\_seed**](#function-dp_ratesync_loop_seed) ([**ratesync\_loop\_t**](structratesync__loop__t.md) \* l, double ahead, double rate, int set\_rate) <br>_Seed the loop: say where the symbol clock is and close on it._  |
|  void | [**dp\_ratesync\_loop\_set\_acquisition**](#function-dp_ratesync_loop_set_acquisition) ([**ratesync\_loop\_t**](structratesync__loop__t.md) \* l, int on) <br>_Say whether the loop's owner acquires for it._  |
|  void | [**dp\_ratesync\_loop\_set\_cascade**](#function-dp_ratesync_loop_set_cascade) ([**ratesync\_loop\_t**](structratesync__loop__t.md) \* l, double term\_rate, size\_t prime\_taps) <br>_Tell the loop the geometry of the accumulator it steers._  |
|  int | [**dp\_ratesync\_loop\_set\_filter\_telemetry**](#function-dp_ratesync_loop_set_filter_telemetry) ([**ratesync\_loop\_t**](structratesync__loop__t.md) \* l, [**dp\_tlm\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_t) \* tlm, const char \* prefix, uint32\_t decim) <br>_Attach (or detach) the timing loop-filter set: "&lt;prefix&gt;.prop" (kp \* e) and "&lt;prefix&gt;.integ" (the filter's integrator), once a recovered symbol, beside the six probes._  |
|  void | [**dp\_ratesync\_loop\_set\_presence**](#function-dp_ratesync_loop_set_presence) ([**ratesync\_loop\_t**](structratesync__loop__t.md) \* l, int present) <br>_The owner's presence input: is the signal known to be there by evidence other than the symbol lock?_  |
|  int | [**dp\_ratesync\_loop\_set\_state**](#function-dp_ratesync_loop_set_state) ([**ratesync\_loop\_t**](structratesync__loop__t.md) \* l, const void \* blob) <br>_Restore the loop's mutable state from_ `blob` _._ |
|  int | [**dp\_ratesync\_loop\_set\_telemetry**](#function-dp_ratesync_loop_set_telemetry) ([**ratesync\_loop\_t**](structratesync__loop__t.md) \* l, [**dp\_tlm\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_t) \* tlm, const char \* prefix, uint32\_t decim) <br>_Register the six timing probes; see_ [_**dp\_ratesync\_set\_telemetry()**_](ratesync__core_8h.md#function-dp_ratesync_set_telemetry) _, which forwards here. NULL_`tlm` _detaches._ |
|  double | [**dp\_ratesync\_loop\_settle\_time**](#function-dp_ratesync_loop_settle_time) (const [**ratesync\_loop\_t**](structratesync__loop__t.md) \* l) <br>_Symbols the loop emits before its timing has settled._  |
|  size\_t | [**dp\_ratesync\_loop\_state\_bytes**](#function-dp_ratesync_loop_state_bytes) (const [**ratesync\_loop\_t**](structratesync__loop__t.md) \* l) <br>_Bytes_ [_**dp\_ratesync\_loop\_get\_state()**_](ratesync__core_8h.md#function-dp_ratesync_loop_get_state) _writes (envelope + payload + the loop filter's child blob)._ |
|  void | [**dp\_ratesync\_loop\_tlm\_flush**](#function-dp_ratesync_loop_tlm_flush) (const [**ratesync\_loop\_t**](structratesync__loop__t.md) \* l) <br>_Emit the timing loop's telemetry for the symbol just recovered._  |
|  int | [**dp\_ratesync\_m\_ok**](#function-dp_ratesync_m_ok) (size\_t m) <br>_The domain of_ `m` _, the terminal outputs per symbol: even, and_`2 <= m <= RATESYNC_MAX_M` _._ |
|  void | [**dp\_ratesync\_reset**](#function-dp_ratesync_reset) ([**dp\_ratesync\_state\_t**](structdp__ratesync__state__t.md) \* state) <br>_Reset to the post-create state: the cascade, the loop integrator, the lock detector, the strobe ring and the prime countdown._  |
|  int | [**dp\_ratesync\_seed**](#function-dp_ratesync_seed) ([**dp\_ratesync\_state\_t**](structdp__ratesync__state__t.md) \* state, double eye, double rate) <br>_Seed the loop from a timing a caller already has._  |
|  int | [**dp\_ratesync\_set\_bn**](#function-dp_ratesync_set_bn) ([**dp\_ratesync\_state\_t**](structdp__ratesync__state__t.md) \* state, double val) <br>_Retune bn at the current zeta, through_ [_**dp\_ratesync\_configure()**_](ratesync__core_8h.md#function-dp_ratesync_configure) _._ |
|  void | [**dp\_ratesync\_set\_presence**](#function-dp_ratesync_set_presence) ([**dp\_ratesync\_state\_t**](structdp__ratesync__state__t.md) \* state, int val) <br>_Set the presence input:_ [_**dp\_ratesync\_loop\_set\_presence()**_](ratesync__core_8h.md#function-dp_ratesync_loop_set_presence) _on this object's loop._ |
|  int | [**dp\_ratesync\_set\_state**](#function-dp_ratesync_set_state) ([**dp\_ratesync\_state\_t**](structdp__ratesync__state__t.md) \* state, const void \* blob) <br>_Restore mutable state from_ `blob` _into an identically built instance._ |
|  int | [**dp\_ratesync\_set\_telemetry**](#function-dp_ratesync_set_telemetry) ([**dp\_ratesync\_state\_t**](structdp__ratesync__state__t.md) \* state, [**dp\_tlm\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_t) \* tlm, const char \* prefix, uint32\_t decim) <br>_Attach (or detach) a telemetry context and register the probes._  |
|  size\_t | [**dp\_ratesync\_state\_bytes**](#function-dp_ratesync_state_bytes) (const [**dp\_ratesync\_state\_t**](structdp__ratesync__state__t.md) \* state) <br>_Bytes_ [_**dp\_ratesync\_get\_state()**_](ratesync__core_8h.md#function-dp_ratesync_get_state) _writes (envelope + payload + child)._ |
|  size\_t | [**dp\_ratesync\_steps**](#function-dp_ratesync_steps) ([**dp\_ratesync\_state\_t**](structdp__ratesync__state__t.md) \* state, const float \_Complex \* x, size\_t x\_len, float \_Complex \* out, size\_t max\_out) <br>_Recover symbols from a block of oversampled cf32 baseband._  |
|  size\_t | [**dp\_ratesync\_steps\_max\_out**](#function-dp_ratesync_steps_max_out) ([**dp\_ratesync\_state\_t**](structdp__ratesync__state__t.md) \* state) <br>_Output-buffer hint for the generated binding; 0 means "the input
length is already a safe bound" — with_ `sps >= m >= 2` _a block can never yield more symbols than it has samples (mirrors symsync)._ |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) int | [**ratesync\_loop\_take\_output**](#function-ratesync_loop_take_output) ([**ratesync\_loop\_t**](structratesync__loop__t.md) \* s, float \_Complex y, float \_Complex \* y\_out, int ted) <br>_Fold one terminal-stage output into the timing loop._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) int | [**ratesync\_step**](#function-ratesync_step) ([**dp\_ratesync\_state\_t**](structdp__ratesync__state__t.md) \* s, float \_Complex x, float \_Complex \* y\_out) <br>_Per-input timing step (the inline composition API)._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) int | [**ratesync\_step\_ted**](#function-ratesync_step_ted) ([**dp\_ratesync\_state\_t**](structdp__ratesync__state__t.md) \* s, float \_Complex x, float \_Complex \* y\_out, int ted) <br>_Per-input timing step with the TED selection as a parameter._  |


## Public Static Functions

| Type | Name |
| ---: | :--- |
|  int | [**dp\_ratesync\_loop\_valid**](#function-dp_ratesync_loop_valid) (const [**ratesync\_loop\_t**](structratesync__loop__t.md) \* l) <br>_Is the symbol just emitted valid: the loop tracking, no seed in progress and the lock declared. On a loop whose owner does not acquire it is the lock flag._  |
|  double | [**ratesync\_rate\_bound**](#function-ratesync_rate_bound) (size\_t m) <br>_The steered-rate bound a timing loop's owner sets on its converter (RateConverter.ctrl\_rate\_max, doppler#3289)._  |

























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**RATESYNC\_FOLD\_MAX**](ratesync__core_8h.md#define-ratesync_fold_max)  `4u`<br>_Terminal outputs one input's call folds: the rest wait as the converter's debt (_ `ctrl_ahead` _) and come out on a later input. The buffer RateSync's and MpskReceiver's steps fold into._ |
| define  | [**RATESYNC\_LOCK\_BUDGET**](ratesync__core_8h.md#define-ratesync_lock_budget)  `1e-6`<br>_The per-decision false-transition budget the default verify counts are derived from: n\_up = dp\_det\_verify\_count(RATESYNC\_LOCK\_PFA,
budget), n\_down = dp\_det\_verify\_count(1 - RATESYNC\_LOCK\_PD, budget). 1e-6 is the library's convention for a detector's own default, not a new number: the Dll's declare budget at its default pfa, the async receiver's code-lock drop, and symsync's SYMSYNC\_LOCK\_BUDGET._  |
| define  | [**RATESYNC\_LOCK\_EPS**](ratesync__core_8h.md#define-ratesync_lock_eps)  `1e-12`<br>_Numerical guard on the on-time+mid energy sum feeding the lock statistic (not tunable) — mirrors SYMSYNC\_LOCK\_EPS._  |
| define  | [**RATESYNC\_LOCK\_PD**](ratesync__core_8h.md#define-ratesync_lock_pd)  `0.9`<br>_See RATESYNC\_LOCK\_PFA._  |
| define  | [**RATESYNC\_LOCK\_PFA**](ratesync__core_8h.md#define-ratesync_lock_pfa)  `1e-3`<br>_The lock detector's sizing rates per decision: the false-alarm probability and detection probability the 0.311 threshold is sized for (symsync's sizing, rolloff 0.35, Es/N0 10 dB)._  |
| define  | [**RATESYNC\_LOOP\_STATE\_MAGIC**](ratesync__core_8h.md#define-ratesync_loop_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('R', 'S', 'L', 'P')`<br>_State-blob magic for the timing loop (FourCC "RSLP")._  |
| define  | [**RATESYNC\_LOOP\_STATE\_VERSION**](ratesync__core_8h.md#define-ratesync_loop_state_version)  `/* multi line expression */`<br>_Timing-loop blob layout version._  |
| define  | [**RATESYNC\_MAX\_M**](ratesync__core_8h.md#define-ratesync_max_m)  `8`<br> |
| define  | [**RATESYNC\_STATE\_MAGIC**](ratesync__core_8h.md#define-ratesync_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('R', 'A', 'T', 'S')`<br>_State-blob magic for RateSync (FourCC "RATS")._  |
| define  | [**RATESYNC\_STATE\_VERSION**](ratesync__core_8h.md#define-ratesync_state_version)  `3u /\* v3: the acquisition (its two settings, the stream counts, the estimator's child blob); v2: running state moved into the loop \*/`<br>_RateSync blob layout version._  |

## Detailed Description


RateSync owns a `RateConverter` whose **terminal stage carries the pulse** (`dp_RateConverter_create_matched`) and closes a timing loop around that stage's control port. It builds no filters of its own: the matched filter IS the cascade's last dot product, and the polyphase arm that dot product selects IS the fractional timing delay. One filter, no Farrow, no separate matched-filter pass.


Where SymbolSync separates the jobs (a matched FIR, then a Farrow interpolator steered by a timing NCO), this fuses them — and because the cascade in front is a full `RateConverter`, the fusion inherits its planning: HB/CIC stages do the bulk decimation for free, so the matched filter is sized by the POST-decimation rate. A matched filter at 256 input samples per symbol costs the same bank as one at 4.


**Arbitrary rate, by construction.** `sps` is a double — 4, 17.33389, an irrational ratio, or a slowly drifting clock — because the terminal stage (resamp) takes its rate as a double and keeps its place in a uint32\_t phase word (a 2^-32 fraction of one input interval), and the loop only has to steer the strobe. That is the real-world case whenever the ADC clock is free-running against the symbol clock.


### Two things this object gets right that are easy to get wrong



**1. Nothing in the error path can vanish.** The TED error is the raw detector output times `ted_scale` — a CONSTRUCT-TIME reciprocal of the detector's own slope against this pulse ([**dp\_symsync\_ted\_slope()**](symsync__core_8h.md#function-dp_symsync_ted_slope)), not a running power estimate. A running normaliser is what the error path used to carry, and getting it wrong killed the loop outright: normalising by the on-time energy `|on|^2` alone divides by zero exactly when the strobe sits on the symbol transitions, which is precisely the state the loop must recover FROM. Measured, the error reached -91, the control drove the terminal stage's effective rate NEGATIVE, its accumulator stopped advancing, and the cascade emitted nothing ever again (2 symbols where 4000 were expected — a permanent death, not a transient). The fix at the time was to normalise by `|on|^2 + |mid|^2`: the same signal half a symbol apart, so the sum is bounded away from zero at every timing phase. That sum survives as the LOCK STATISTIC's normaliser, which is the only place it is still computed; the error path moved to the construct-time constant (RATESYNC\_LOOP\_STATE\_VERSION 2 records the change), which settles the question rather than answering it — a constant cannot vanish.


Either way the conclusion holds and is what a caller cares about: RateSync needs no clamp on the control anywhere, and there is none in the source. Measured, `ctrl` stays inside a few hundredths driven from the worst initial offset at the widest recommended `bn`, bounded by the detector's own S-curve, which is bounded by construction (report §2.7).


**2. The loop stays open until the cascade is primed.** A cascade's first outputs are its delay lines filling, not signal (the eye statistic swings over its whole +-2 range through them). Steering on them is meaningless and was worth one lost acquisition in sixteen. [**dp\_ratesync\_create()**](ratesync__core_8h.md#function-dp_ratesync_create) computes the prime length from the terminal bank's own geometry.



### The T/2 role ambiguity resolves itself



Gardner needs an on-time strobe and a transition gate half a symbol earlier. Running at `rate = m/sps` and taking every m-th output as on-time makes that a parity count, which looks like it should be ambiguous — and a half-symbol error is indeed an equilibrium of the detector. It is an **unstable** one: measured over a fine sweep, each parity's S-curve has exactly two zeros per symbol, one at the eye centre with negative slope (stable) and one at the T/2 point with positive slope (unstable). The loop runs away from the wrong one on its own, so **the parity does not matter** and no eye-sign detector or counter flip is needed. (An earlier prototype ran two displaced banks to pin the roles structurally; measurement showed that buys nothing and costs double the multiplies.)



### Measured



RRC-BPSK, noiseless, eight initial timing offsets each: every offset acquires, on every cascade the planner builds — HB + Resampler(1,rrc) at `sps = 4`, CIC(8) + Resampler(0.923,rrc) at 17.333, CIC(32) + Resampler(1,rrc) at 64.


**The EVM those runs reach is deliberately not quoted here.** A table of literals in a header is the documentation form of a snapshot nothing re-runs, and this one had drifted 3 to 4 dB optimistic by the time anything re-measured it. The live figures are regenerated on every push into `src/doppler/track/tests/validation/ratesync/results.md` — §2.4 for acquisition against the settling budget, §2.5 for the `bn` sweep — and that report's Limits section is executed by `test_validation_limits.py`.


What belongs here is the shape, which is stable. `bn` means the same thing on every planned cascade, and that is the point of referencing the control to the TERMINAL stage's rate rather than the cascade rate: the alternative is measured costing 18 dB. How closely depends on how narrow the loop is — the spread across the three cascades widens monotonically as `bn` narrows, from a few tenths of a dB at `bn = 0.02` to several dB at 0.002, so read it as "within ~1 dB at the recommended settings" rather than as a universal. What that measurement does NOT establish is the mechanism: the cascades differ in front-end group delay and in residual ISI, and which of those a narrow loop stops averaging over is not determined. `bn = 0.005` is the best of the recommended settings and `bn = 0.01` the safe default; lower settings acquire too slowly to have settled inside a fixed record, so a record length that does not scale with `1/bn` measures nothing there.


Lifecycle: `create -> (step / steps / reset)* -> destroy`



```C++
dp_ratesync_state_t *rx = dp_ratesync_create (17.33389, RATESYNC_PULSE_RRC, 0.35,
                                        8, 2, 1024, 0.01, 0.707,
                                        RATESYNC_TED_GARDNER);
float _Complex sym;
if (ratesync_step (rx, x, &sym))
  consume (sym);
dp_ratesync_destroy (rx);
```
 



    
## Public Types Documentation




### enum ratesync\_\_core\_8h\_1a0411cd49bb5b71852cecd93bcbf0ca2d 

_Matched-filter pulse shape._ 
```C++
enum ratesync__core_8h_1a0411cd49bb5b71852cecd93bcbf0ca2d {
    RATESYNC_PULSE_IANDD = RC_PULSE_IANDD,
    RATESYNC_PULSE_RRC = RC_PULSE_RRC
};
```



Aliases of the cascade's own vocabulary (`rc_pulse_t`) so one set of names covers the family; the pulse is built by the RateConverter, which is the only party that knows its own CIC geometry. 


        

<hr>



### enum ratesync\_\_core\_8h\_1a726ca809ffd3d67ab4b8476646f26635 

_Timing-error-detector selection for dp\_ratesync\_state\_t::ted._ 
```C++
enum ratesync__core_8h_1a726ca809ffd3d67ab4b8476646f26635 {
    RATESYNC_TED_GARDNER = 0,
    RATESYNC_TED_DTTL = 1
};
```




<hr>



### enum ratesync\_lifecycle\_t 

```C++
enum ratesync_lifecycle_t {
    RATESYNC_ACQUIRING = 0,
    RATESYNC_TRACKING = 1,
    RATESYNC_COASTING = 2
};
```



The timing loop's states (docs/design/ratesync-timing.md, "Acquisition,
then tracking"). A loop with no estimator beside it ([**dp\_ratesync\_loop\_set\_acquisition()**](ratesync__core_8h.md#function-dp_ratesync_loop_set_acquisition) never called) is in tracking from reset and never leaves it. 


        

<hr>
## Public Functions Documentation




### function dp\_ratesync\_acquire\_push 

_Hand one input to the estimator while the loop is not tracking, and seed the loop from the block that passes._ 
```C++
void dp_ratesync_acquire_push (
    dp_ratesync_state_t * state,
    float _Complex x
) 
```



Called by [**ratesync\_step\_ted()**](ratesync__core_8h.md#function-ratesync_step_ted) for every input taken outside tracking. The first input of an acquisition resets the estimator, so its `eye` counts from there; a block of the configured length is closed, and one whose quality is at or above the configured figure goes to [**dp\_ratesync\_seed()**](ratesync__core_8h.md#function-dp_ratesync_seed)'s path with the loop's held rate kept. A block that does not pass is dropped and the next one starts.




**Parameters:**


* `state` Must be non-NULL, with acquisition configured. 
* `x` The input just taken. 




        

<hr>



### function dp\_ratesync\_configure 

_Recompute the loop gains for a new bandwidth/damping, keeping the timing estimate._ 
```C++
int dp_ratesync_configure (
    dp_ratesync_state_t * state,
    double bn,
    double zeta
) 
```



Only the PI coefficients change; the integrator, and therefore the tracked rate and the lock, carries through untouched. Use it to narrow the loop after acquisition (a wide `bn` pulls in fast, a narrow one tracks with less jitter) without forcing a re-acquire.




**Parameters:**


* `state` Must be non-NULL. 
* `bn` Loop noise bandwidth, normalised to the symbol rate. 
* `zeta` Damping factor (1.0 = critical; 0.707 usual). 



**Returns:**

DP\_OK, or DP\_ERR\_INVALID when (`bn`, `zeta`) is outside [**dp\_loop\_filter\_params\_ok()**](loop__filter__core_8h.md#function-dp_loop_filter_params_ok); nothing changes then (Python raises ValueError). 
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



### function dp\_ratesync\_configure\_acquisition 

_Turn the built-in acquisition on, or off._ 
```C++
int dp_ratesync_configure_acquisition (
    dp_ratesync_state_t * state,
    size_t symbols,
    double quality
) 
```



On, the object starts by estimating: a feedforward timing estimate over `symbols` symbols on a matched filter of its own ([**dp\_timing\_est\_create\_matched()**](timing__est__core_8h.md#function-dp_timing_est_create_matched)), repeated until one has a quality of `quality` or more, then handed to the loop as a seed. The loop closes on it, so the time to a closed loop is a count: the estimator's lead-in, then `symbols` symbols an attempt. A declared lock that drops starts it again ([**dp\_ratesync\_set\_presence()**](ratesync__core_8h.md#function-dp_ratesync_set_presence) says from which rate). [**dp\_ratesync\_get\_settle\_time()**](ratesync__core_8h.md#function-dp_ratesync_get_settle_time) counts one attempt.


Both figures are the caller's. The block's length sets the estimate's error and the threshold its false-alarm probability, and neither has a derived default yet: the estimator's validation report (`src/doppler/estimation/tests/validation/timing_est/results.md`) gives the error and the pass rate against both.


If the lock is not declared when this is called the loop is put in acquiring at once. Off (`symbols` 0), the loop is in tracking and stays there, which is the object as it was built.




**Parameters:**


* `state` Must be non-NULL. 
* `symbols` Symbols an attempt estimates over; 0 turns it off. 
* `quality` The estimate's `quality` at or above which it seeds; over 0 and under 1. Ignored when `symbols` is 0. 



**Returns:**

DP\_OK, or DP\_ERR\_INVALID with nothing changed: `quality` out of range, or an `sps` under TIMING\_EST\_MATCHED\_SPS, where the estimator's matched filter cannot be built. 
```C++
>>> import numpy as np
>>> from doppler.track import RateSync
>>> syms = np.where(np.random.default_rng(3).integers(0, 2, 3000) > 0,
...                 1.0, -1.0)
>>> x = (0.25 * np.repeat(syms, 8)).astype(np.complex64)
>>> rs = RateSync(sps=8.0, pulse="iandd", m=4, bn=0.01)
>>> rs.configure_acquisition(128, 0.05)
>>> rs.lifecycle
'acquiring'
>>> y = rs.steps(x)
>>> rs.lifecycle, rs.valid
('tracking', True)
```
 





        

<hr>



### function dp\_ratesync\_configure\_lock\_raw 

_Set the lock detector's geometry directly._ 
```C++
void dp_ratesync_configure_lock_raw (
    dp_ratesync_state_t * state,
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



### function dp\_ratesync\_create 

_Create a RateSync instance._ 
```C++
dp_ratesync_state_t * dp_ratesync_create (
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



Builds a `RateConverter(rate = m/sps, pulse, ..., pulse_sps = m)` with CIC droop compensation on — folded into the bank, so it costs six taps per arm and no extra stage, and is worth ~28 dB of EVM on any cascade that plans a CIC. See [**dp\_RateConverter\_create\_matched()**](RateConverter__core_8h.md#function-dp_rateconverter_create_matched).




**
**

Present **unit-amplitude symbols**. This object carries no AGC, and that is deliberate: a receiver composing it already levels in its own front-end cascade ([**dp\_RateConverter\_enable\_agc()**](RateConverter__core_8h.md#function-dp_rateconverter_enable_agc), one per receiver), so an AGC here would be a second one integrating against the first. The level to hit is not a tuned number — the TED normalises by its own construct-time slope, and that slope is computed for the reference the bank already defines, `10*log10(bank_e0 / bank_sps)`, which is ~0 dB because the bank normalises by its own pulse energy. See [**dp\_RateConverter\_agc\_ref\_db()**](RateConverter__core_8h.md#function-dp_rateconverter_agc_ref_db), which is defined for any matched cascade whether or not an AGC is enabled.


**The level axis is two-sided and it is not monotone**, so "as long as
it is not clipping" is not a level check. Both ends cost EVM for one reason: the Gardner error carries an `A^2` factor, so the input level multiplies the loop gain and the level axis IS the `bn` axis — too hot tracks noisily, too cold has not settled. Measured, EVM is flat to within a few tenths of a dB either side of the contracted level and falls off sharply outside that, by well over 15 dB at twice amplitude and again at a quarter of it (report §2.6, regenerated every push).


**Nothing here reports either end reliably.** Over-drive is reported only on the subset of plans that happen to contain a CIC: [**dp\_ratesync\_get\_clipped()**](ratesync__core_8h.md#function-dp_ratesync_get_clipped) is a CIC quantiser flag, and whether the plan HAS a CIC is the planner's decision, not the caller's — a CIC-free cascade (which is what `sps = 8` plans) reads 0 however hard it is driven. Under-drive has no flag on any plan at all; that gap is tracked as gh-661. And `locked` is not a substitute for either: it answers "is the
eye open", which even a badly mis-levelled loop eventually manages, so it declares lock while demodulating far worse. Judge the input level by measuring it, not by reading a flag off this object.




**Parameters:**


* `sps` Nominal samples per symbol; any double &gt;= `m` (17.33389 is as valid as 4). The bound is `m`, not 2, because the terminal stage must not be asked to interpolate: `rate = m/sps <= 1`. 
* `pulse` RATESYNC\_PULSE\_IANDD (rectangular/NRZ) or \_RRC. 
* `beta` RRC roll-off in `[0, 1]` (ignored for the rectangle). 
* `span` One-sided RRC span in symbols (ignored for the rectangle, whose support is always one symbol). 
* `m` Terminal outputs per symbol: even, `2 <= m <= RATESYNC_MAX_M`. Gardner needs the half-symbol gate, so m must be even and at least 2. The oversampled stream is a by-product, not an extra cost. Both pulses lock at every m. With RATESYNC\_PULSE\_IANDD a small m costs EVM, not lock: on a noiseless NRZ stream at 8 samples a symbol the error vector is largest at m = 2, smaller at m = 4 and gone at m = 6 and 8; report §2.7 sweeps it. The RRC reads the same at every m. 
* `num_phases` Matched-filter arms; power of two (1024 is a good default). Sets the fractional-timing resolution to `1/num_phases` of an output period. 
* `bn` Loop noise bandwidth, normalised to the symbol rate, of the closed loop given a unit-slope detector (see LoopFilter). The detector is normalised by its own slope against the pulse, which makes the slope 1 at UNIT symbol amplitude only: Gardner's goes as the amplitude squared and DTTL's as the amplitude, so a quarter-amplitude stream runs a Gardner loop at 1/16 of `bn`. 
* `zeta` Damping factor (1.0 = critical; 0.707 usual). 
* `ted` RATESYNC\_TED\_GARDNER (blind) or RATESYNC\_TED\_DTTL (decision-directed; BPSK/QPSK only). 



**Returns:**

Heap-allocated state, or NULL if a parameter is out of range or allocation fails. 




**Note:**

Caller must call [**dp\_ratesync\_destroy()**](ratesync__core_8h.md#function-dp_ratesync_destroy) when done. 





        

<hr>



### function dp\_ratesync\_destroy 

_Destroy a RateSync instance and release all memory._ 
```C++
void dp_ratesync_destroy (
    dp_ratesync_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function dp\_ratesync\_get\_bn 

_Timing-loop noise bandwidth, normalised to the symbol rate. Writing it re-derives the loop gains at the damping already in use, exactly as configure() does._ 
```C++
double dp_ratesync_get_bn (
    const dp_ratesync_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

bn: the create() value or the last one configure() accepted. 





        

<hr>



### function dp\_ratesync\_get\_clipped 

_Has the cascade's CIC stage clipped its input since the last reset? Forwarded from the RateConverter: a CIC bounds its input to its full scale (CIC\_PAPR\_WIDE as built) and clips silently at and past that, which no timing metric reveals. Always 0 when the plan has no CIC stage._ 
```C++
int dp_ratesync_get_clipped (
    const dp_ratesync_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

1 if the CIC has clipped since the last reset, else 0. 





        

<hr>



### function dp\_ratesync\_get\_ctrl 

_Current per-input control deviation steering the strobe._ 
```C++
double dp_ratesync_get_ctrl (
    const dp_ratesync_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Rate deviation added to the terminal stage's rate (same units as that rate); 0 after create/reset. 





        

<hr>



### function dp\_ratesync\_get\_lifecycle 

_Where the timing loop is: acquiring, tracking or coasting._ 
```C++
int dp_ratesync_get_lifecycle (
    const dp_ratesync_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

A ratesync\_lifecycle\_t; RATESYNC\_TRACKING always, with no acquisition configured. 





        

<hr>



### function dp\_ratesync\_get\_lock\_stat 

_Last block-averaged lock statistic (the eye-opening ratio)._ 
```C++
double dp_ratesync_get_lock_stat (
    const dp_ratesync_state_t * state
) 
```



This, not an error-vector magnitude, is the honest lock indicator: a single cycle slip during acquisition drags a windowed EVM by 20 dB while the eye stays wide open at +0.75. Judge lock here. 

**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Mean lock\_signal over the last completed block of avgs looks; 0.0 until the first block completes. 





        

<hr>



### function dp\_ratesync\_get\_locked 

_Current lock decision (1 = locked), verify-counted._ 
```C++
int dp_ratesync_get_locked (
    const dp_ratesync_state_t * state
) 
```



One decision per block of 133 looks: the block's mean lock statistic against 0.311, the threshold symsync sizes at a per-decision pfa of 1e-3 and pd of 0.9 (rolloff 0.35, Es/N0 10 dB). The lock declares after 2 consecutive decisions above it and drops after 6 below it: the counts [**dp\_det\_verify\_count()**](detection__core_8h.md#function-dp_det_verify_count) gives for those rates at a per-decision false-transition budget of 1e-6. Those rates are the sizing point's; at another Es/N0 the counts meet the budget only if the statistic's rates do, which a caller measures, and [**dp\_ratesync\_configure\_lock\_raw()**](ratesync__core_8h.md#function-dp_ratesync_configure_lock_raw) sets other counts. 

**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

1 when locked, 0 otherwise (0 after create/reset). 





        

<hr>



### function dp\_ratesync\_get\_presence 

_The presence input as it stands._ 
```C++
int dp_ratesync_get_presence (
    const dp_ratesync_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

The value last set, non-zero when the owner has said the signal is there; 0 after create or reset. 





        

<hr>



### function dp\_ratesync\_get\_rate 

_Smoothed tracked samples per symbol. Departs from the nominal_ `sps` _by exactly the sample-clock offset being tracked, so it is the estimator a rate-disciplining caller reads._
```C++
double dp_ratesync_get_rate (
    const dp_ratesync_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Samples per symbol, sps / (1 + loop integrator), clamped to 0.5 sps to 1.5 sps; the nominal sps after create/reset. 





        

<hr>



### function dp\_ratesync\_get\_settle\_time 

_Symbols emitted before the timing has settled: where a steady-state measurement of the output may start._ 
```C++
double dp_ratesync_get_settle_time (
    const dp_ratesync_state_t * state
) 
```



The timing loop's own figure, [**dp\_ratesync\_loop\_settle\_time()**](ratesync__core_8h.md#function-dp_ratesync_loop_settle_time): the time the closed loop takes to bring a starting timing error under 2% of itself, from the bandwidth and damping in use, 2.59 / bn symbols at the default damping. It is known at construction and follows configure(). `locked` says a statistic crossed a threshold, which happens while the timing is still moving; this says when it has stopped. With the acquisition on ([**dp\_ratesync\_configure\_acquisition()**](ratesync__core_8h.md#function-dp_ratesync_configure_acquisition)) one attempt is added ahead of it: the estimator's lead-in, the block, and the two symbols a seed keeps the loop open.


It is a linear loop's figure, so it holds for a starting error inside the detector's near-linear range: on the RRC with Gardner, 0.25 symbol or less (measured at 4 samples a symbol, bn 0.01, damping 0.707), where the mean error over data patterns is under 2% of the start (plus five standard errors of that mean) from this symbol on (test\_ratesync\_core.c, `test_settle_time_from_an_offset`; the figures that follow are printed by native/validation/ratesync\_settle.c). Further out the S-curve, a sinusoid, has less gain and the loop takes longer: from this symbol on, up to 4.5% of a 0.37 symbol start is left, and up to 9.1% of 0.44. On the rectangle the Gardner loop settles more slowly than this figure, and is what the derivative detector replaces (docs/design/ratesync-timing.md section 9.2; doppler#3377 is the baseline). One run does not show any of this: Gardner's data jitter on a shaped stream is larger than 2%.


Example (Python): 
> 
> 
> from doppler.track import RateSync rs = RateSync(sps=8.0, bn=0.01) round(rs.settle\_time) 


259 
> 
> 
> rs.bn = 0.005 round(rs.settle\_time) 


519




**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Symbols, from the first one emitted; infinity at `bn = 0`. 





        

<hr>



### function dp\_ratesync\_get\_state 

_Serialize the mutable state into_ `blob` _._
```C++
void dp_ratesync_get_state (
    const dp_ratesync_state_t * state,
    void * blob
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `blob` Caller-owned buffer of at least [**dp\_ratesync\_state\_bytes()**](ratesync__core_8h.md#function-dp_ratesync_state_bytes) bytes. 




        

<hr>



### function dp\_ratesync\_get\_timing\_error 

_Last normalised TED error — the loop stress._ 
```C++
double dp_ratesync_get_timing_error (
    const dp_ratesync_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

The detector output scaled by its own slope (dimensionless); 0 before the first steered symbol. 





        

<hr>



### function dp\_ratesync\_get\_valid 

_Is the symbol just emitted valid?_ 
```C++
int dp_ratesync_get_valid (
    const dp_ratesync_state_t * state
) 
```



The lock flag, while the loop is tracking and no seed is in progress. With no acquisition configured it is [**dp\_ratesync\_get\_locked()**](ratesync__core_8h.md#function-dp_ratesync_get_locked). Every symbol period emits a symbol in every state, and this is the flag beside it: read it after each [**ratesync\_step()**](ratesync__core_8h.md#function-ratesync_step) that returns 1.




**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

1 or 0. 





        

<hr>



### function dp\_ratesync\_loop\_bind\_cascade 

_Read that geometry straight off a cascade._ 
```C++
void dp_ratesync_loop_bind_cascade (
    ratesync_loop_t * l,
    const dp_RateConverter_state_t * rc
) 
```



Walks `rc` to its terminal stage and forwards its rate and tap count to [**dp\_ratesync\_loop\_set\_cascade()**](ratesync__core_8h.md#function-dp_ratesync_loop_set_cascade). Every owner of this loop owns a `RateConverter` somewhere — RateSync directly, the receivers inside their DDC — so the walk lives here once rather than in each of them.




**Parameters:**


* `l` Must be non-NULL. 
* `rc` The cascade whose terminal stage the loop steers. 




        

<hr>



### function dp\_ratesync\_loop\_configure 

_Retune the loop; preserves the integrator (and so the lock)._ 
```C++
int dp_ratesync_loop_configure (
    ratesync_loop_t * l,
    double bn,
    double zeta
) 
```



A (`bn`, `zeta`) outside [**dp\_loop\_filter\_params\_ok()**](loop__filter__core_8h.md#function-dp_loop_filter_params_ok) is refused: nothing changes.




**Parameters:**


* `l` Must be non-NULL. 
* `bn` Loop noise bandwidth, normalized to the symbol rate. 
* `zeta` Damping factor. 



**Returns:**

DP\_OK, or DP\_ERR\_INVALID when (`bn`, `zeta`) is refused. 





        

<hr>



### function dp\_ratesync\_loop\_configure\_lock\_raw 

_Set the lock detector's geometry; see_ [_**dp\_ratesync\_configure\_lock\_raw()**_](ratesync__core_8h.md#function-dp_ratesync_configure_lock_raw) _, which forwards here._
```C++
void dp_ratesync_loop_configure_lock_raw (
    ratesync_loop_t * l,
    size_t avgs,
    double up_thresh,
    double down_thresh,
    uint32_t n_up,
    uint32_t n_down
) 
```





**Parameters:**


* `l` Must be non-NULL. 
* `avgs` Looks per decision; clamped &gt;= 1. 
* `up_thresh` Declare threshold on lock\_stat. 
* `down_thresh` Drop threshold; &lt;= up\_thresh for hysteresis. 
* `n_up` Consecutive above-threshold decisions to declare. 
* `n_down` Consecutive below-threshold decisions to drop. 




        

<hr>



### function dp\_ratesync\_loop\_decide 

_A lock decision's consequence for the state; called from_ [_**ratesync\_loop\_take\_output()**_](ratesync__core_8h.md#function-ratesync_loop_take_output) _on a loop whose owner acquires._
```C++
void dp_ratesync_loop_decide (
    ratesync_loop_t * l,
    int was,
    int now
) 
```





**Parameters:**


* `l` Must be non-NULL. 
* `was` The lock flag before the decision. 
* `now` The lock flag after it. 




        

<hr>



### function dp\_ratesync\_loop\_get\_state 

_Serialize the loop's mutable state into_ `blob` _._
```C++
void dp_ratesync_loop_get_state (
    const ratesync_loop_t * l,
    void * blob
) 
```





**Parameters:**


* `l` Must be non-NULL. 
* `blob` Caller-owned buffer of at least [**dp\_ratesync\_loop\_state\_bytes()**](ratesync__core_8h.md#function-dp_ratesync_loop_state_bytes) bytes. 




        

<hr>



### function dp\_ratesync\_loop\_init 

_Initialise a standalone timing loop._ 
```C++
void dp_ratesync_loop_init (
    ratesync_loop_t * l,
    double sps,
    size_t m,
    double bn,
    double zeta,
    int ted
) 
```



Sets the loop filter (update period = one symbol, so `bn` is normalised to the symbol rate) and the default lock-detector geometry, then seeds every running field. The caller must still describe the accumulator being steered with [**dp\_ratesync\_loop\_set\_cascade()**](ratesync__core_8h.md#function-dp_ratesync_loop_set_cascade) before pushing outputs through.




**Parameters:**


* `l` Loop to initialise. Must be non-NULL. 
* `sps` Nominal samples per symbol (any double). 
* `m` Terminal outputs per symbol; even, 2..RATESYNC\_MAX\_M. 
* `bn` Loop noise bandwidth, normalised to the symbol rate. 
* `zeta` Damping factor. 
* `ted` RATESYNC\_TED\_GARDNER or RATESYNC\_TED\_DTTL. 




        

<hr>



### function dp\_ratesync\_loop\_reset 

_Re-seed the loop: integrator, strobe ring, lock detector and the prime countdown. Configuration and cascade geometry are kept._ 
```C++
void dp_ratesync_loop_reset (
    ratesync_loop_t * l
) 
```





**Parameters:**


* `l` Must be non-NULL. 




        

<hr>



### function dp\_ratesync\_loop\_seed 

_Seed the loop: say where the symbol clock is and close on it._ 
```C++
int dp_ratesync_loop_seed (
    ratesync_loop_t * l,
    double ahead,
    double rate,
    int set_rate
) 
```



`ahead` is how far past the terminal stage's NEXT output an on-time instant sits, in output periods at the rate the loop holds, 0 or more and not whole. Its whole part picks the on-time outputs, that one and every `m`-th after it. Its fraction `r` is moved by one symbol of control: the terminal stage sets the interval to its next output as each output is emitted, `1 / (R + ctrl)` of its inputs, so the control that stretches `m` intervals by `r / m` of themselves is  held while `m` outputs are emitted. It is always a stretch. The loop enters tracking, its lock detector is reset, and it stays open for the `2 m` outputs the stretch and the gate behind it take; the symbols of those outputs are emitted and are not valid.




**Parameters:**


* `l` Must be non-NULL. A loop still in its prime countdown takes the seed too: the outputs the fill drops are counted, so the on-time outputs are the same ones. 
* `ahead` Output periods from the next output to an on-time instant; finite, 0 or more. 
* `rate` The loop filter's integrator to start from, symbols a symbol: `sps / true_sps - 1`. Read only with `set_rate`; magnitude under 0.5. 
* `set_rate` 0 keeps the rate the loop holds: zero after acquiring, the held rate after a coast. 



**Returns:**

DP\_OK, or DP\_ERR\_INVALID (a non-finite or negative `ahead`, a `rate` out of range) with nothing changed. 





        

<hr>



### function dp\_ratesync\_loop\_set\_acquisition 

_Say whether the loop's owner acquires for it._ 
```C++
void dp_ratesync_loop_set_acquisition (
    ratesync_loop_t * l,
    int on
) 
```



With `on` the loop has the three states of the design page: it is put in acquiring now unless its lock is declared, a reset returns it there, and tracking is left when a declared lock drops (to coasting with the presence input up, to acquiring without) or when `n_down` decisions in a row are not hits on a loop that has not declared. The owner runs its estimator while the state is not tracking and hands the result to [**dp\_ratesync\_loop\_seed()**](ratesync__core_8h.md#function-dp_ratesync_loop_seed). With `on` 0 the loop is in tracking and stays there, which is the loop every owner had before.




**Parameters:**


* `l` Must be non-NULL. 
* `on` Non-zero: the owner acquires. 




        

<hr>



### function dp\_ratesync\_loop\_set\_cascade 

_Tell the loop the geometry of the accumulator it steers._ 
```C++
void dp_ratesync_loop_set_cascade (
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



### function dp\_ratesync\_loop\_set\_filter\_telemetry 

_Attach (or detach) the timing loop-filter set: "&lt;prefix&gt;.prop" (kp \* e) and "&lt;prefix&gt;.integ" (the filter's integrator), once a recovered symbol, beside the six probes._ 
```C++
int dp_ratesync_loop_set_filter_telemetry (
    ratesync_loop_t * l,
    dp_tlm_t * tlm,
    const char * prefix,
    uint32_t decim
) 
```



".ctrl" is the filter's output times the terminal stage's rate and ".rate" is `sps / (1 + integ)`, clamped, so neither shows the two terms apart; this set does. It is emitted from [**dp\_ratesync\_loop\_tlm\_flush()**](ratesync__core_8h.md#function-dp_ratesync_loop_tlm_flush), so it reports only while the six-probe set is attached too. Its own context, so a composer that does not ask for it (MpskReceiver's own attach) registers nothing more. Observation only: no decision reads it. 

**Parameters:**


* `l` Must be non-NULL. 
* `tlm` Telemetry context (borrowed), or NULL to detach this set. 
* `prefix` Probe-name prefix; NULL means "sync". 
* `decim` Emit every decim-th symbol. 



**Returns:**

DP\_OK, or DP\_ERR\_INVALID (an overlong prefix or a full probe table), the set then detached. 





        

<hr>



### function dp\_ratesync\_loop\_set\_presence 

_The owner's presence input: is the signal known to be there by evidence other than the symbol lock?_ 
```C++
void dp_ratesync_loop_set_presence (
    ratesync_loop_t * l,
    int present
) 
```



A DSSS receiver raises it while its code lock and its carrier lock are both up. It decides where a lost symbol lock goes: with presence the loop coasts, held at its rate, and without it the loop acquires from a zero rate. Lowering it during a coast ends the coast in acquiring. It has no effect on a loop whose owner does not acquire.




**Parameters:**


* `l` Must be non-NULL. 
* `present` Non-zero: present. 




        

<hr>



### function dp\_ratesync\_loop\_set\_state 

_Restore the loop's mutable state from_ `blob` _._
```C++
int dp_ratesync_loop_set_state (
    ratesync_loop_t * l,
    const void * blob
) 
```



Config is not in the blob: restore into a loop built with the same geometry. A refusal changes nothing.




**Parameters:**


* `l` Must be non-NULL. 
* `blob` Blob from [**dp\_ratesync\_loop\_get\_state()**](ratesync__core_8h.md#function-dp_ratesync_loop_get_state). 



**Returns:**

DP\_OK, or DP\_ERR\_INVALID if any envelope rejects. 





        

<hr>



### function dp\_ratesync\_loop\_set\_telemetry 

_Register the six timing probes; see_ [_**dp\_ratesync\_set\_telemetry()**_](ratesync__core_8h.md#function-dp_ratesync_set_telemetry) _, which forwards here. NULL_`tlm` _detaches._
```C++
int dp_ratesync_loop_set_telemetry (
    ratesync_loop_t * l,
    dp_tlm_t * tlm,
    const char * prefix,
    uint32_t decim
) 
```





**Parameters:**


* `l` Must be non-NULL. 
* `tlm` Telemetry context to attach (borrowed), or NULL. 
* `prefix` Probe-name prefix; NULL means "sync". 
* `decim` Emit every decim-th symbol. 



**Returns:**

DP\_OK, or DP\_ERR\_INVALID if the prefix is too long or the probe table cannot take all six (nothing is attached). 





        

<hr>



### function dp\_ratesync\_loop\_settle\_time 

_Symbols the loop emits before its timing has settled._ 
```C++
double dp_ratesync_loop_settle_time (
    const ratesync_loop_t * l
) 
```



[**dp\_loop\_filter\_settle\_time()**](loop__filter__core_8h.md#function-dp_loop_filter_settle_time) of the loop's own `bn` and `zeta` at DP\_LOOP\_FILTER\_SETTLE\_EPS: the timing error left from the start is under 2% of it from that symbol on. An index into the symbols the loop emits, so the outputs dropped while the cascade fills are not in it. It follows [**dp\_ratesync\_loop\_configure()**](ratesync__core_8h.md#function-dp_ratesync_loop_configure). It is the linear loop's figure: the detector's slope at lock is taken as 1, and a clock offset's own transient is not counted. Where that holds, and where it does not, is stated at [**dp\_ratesync\_get\_settle\_time()**](ratesync__core_8h.md#function-dp_ratesync_get_settle_time). 

**Parameters:**


* `l` Must be non-NULL. 



**Returns:**

Symbols; infinity at `bn = 0`, a loop that does not steer. 





        

<hr>



### function dp\_ratesync\_loop\_state\_bytes 

_Bytes_ [_**dp\_ratesync\_loop\_get\_state()**_](ratesync__core_8h.md#function-dp_ratesync_loop_get_state) _writes (envelope + payload + the loop filter's child blob)._
```C++
size_t dp_ratesync_loop_state_bytes (
    const ratesync_loop_t * l
) 
```





**Parameters:**


* `l` Must be non-NULL. 



**Returns:**

Blob size in bytes. 





        

<hr>



### function dp\_ratesync\_loop\_tlm\_flush 

_Emit the timing loop's telemetry for the symbol just recovered._ 
```C++
void dp_ratesync_loop_tlm_flush (
    const ratesync_loop_t * l
) 
```



Out-of-line on purpose: the emit machinery must not inline into the per-sample hot loop (the same body-growth cost symsync measured). Callers gate on `l->tlm.ctx`, so the detached cost is one predicted-not-taken branch per symbol.




**Parameters:**


* `l` Loop with a non-NULL tlm.ctx (caller-checked). 




        

<hr>



### function dp\_ratesync\_m\_ok 

_The domain of_ `m` _, the terminal outputs per symbol: even, and_`2 <= m <= RATESYNC_MAX_M` _._
```C++
int dp_ratesync_m_ok (
    size_t m
) 
```



The ONE predicate. Gardner needs the half-symbol gate, so `m` is even and at least 2, and RATESYNC\_MAX\_M bounds the in-struct strobe ring. [**dp\_ratesync\_create()**](ratesync__core_8h.md#function-dp_ratesync_create) and [**dp\_mpsk\_receiver\_create()**](mpsk__receiver__core_8h.md#function-dp_mpsk_receiver_create) refuse outside it, and a composition that takes `m` from a caller and builds the chain later checks it first, so the build never meets the refusal. That is [**dp\_async\_dsss\_receiver\_configure\_chain\_raw()**](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_configure_chain_raw), whose rebuild runs under dp\_xnn (doppler#2421).




**Parameters:**


* `m` Terminal outputs per symbol. 



**Returns:**

1 inside the domain, 0 outside it. 





        

<hr>



### function dp\_ratesync\_reset 

_Reset to the post-create state: the cascade, the loop integrator, the lock detector, the strobe ring and the prime countdown._ 
```C++
void dp_ratesync_reset (
    dp_ratesync_state_t * state
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



### function dp\_ratesync\_seed 

_Seed the loop from a timing a caller already has._ 
```C++
int dp_ratesync_seed (
    dp_ratesync_state_t * state,
    double eye,
    double rate
) 
```



`eye` is an eye center on the input's timeline: the index, counted from create or reset and not whole, of a sample instant where a symbol is to be read. Other eye centers are whole symbols either side, so any one of them will do, past or future. The object finds how far the terminal stage's next output is from one (the stage's accumulator and the cascade's group delay say where that output reads) and seeds the loop with it, [**dp\_ratesync\_loop\_seed()**](ratesync__core_8h.md#function-dp_ratesync_loop_seed). The built-in acquisition makes this call with its estimate.




**Parameters:**


* `state` Must be non-NULL. 
* `eye` An eye center, input samples since create or reset. 
* `rate` The symbol clock's offset to start the loop's rate state from, symbols a symbol: `sps / true_sps - 1`, 0 for the nominal clock. Magnitude under 0.5. 



**Returns:**

DP\_OK, or DP\_ERR\_INVALID with nothing changed (a non-finite `eye`, a `rate` out of range). 
```C++
>>> import numpy as np
>>> from doppler.track import RateSync
>>> syms = np.where(np.random.default_rng(3).integers(0, 2, 3000) > 0,
...                 1.0, -1.0)
>>> x = np.repeat(syms, 8)[5:].astype(np.complex64)  # edges at 3, 11, ...
>>> rs = RateSync(sps=8.0, pulse="iandd", m=4, bn=0.0)  # does not steer
>>> rs.seed(6.5)                         # a symbol's center
>>> v = np.array(rs.steps(x)).real[4:-4]
>>> bool(np.mean(np.abs(v)) ** 2 / np.mean(v ** 2) > 0.99)  # in the eye
True
```
 





        

<hr>



### function dp\_ratesync\_set\_bn 

_Retune bn at the current zeta, through_ [_**dp\_ratesync\_configure()**_](ratesync__core_8h.md#function-dp_ratesync_configure) _._
```C++
int dp_ratesync_set_bn (
    dp_ratesync_state_t * state,
    double val
) 
```



An out-of-domain value is refused and nothing changes, so [**dp\_ratesync\_get\_bn()**](ratesync__core_8h.md#function-dp_ratesync_get_bn) keeps reporting the bandwidth in use (Python raises ValueError).




**Parameters:**


* `state` Must be non-NULL. 
* `val` New loop noise bandwidth, normalized to the symbol rate. 



**Returns:**

DP\_OK, or DP\_ERR\_INVALID when `val` is refused. 





        

<hr>



### function dp\_ratesync\_set\_presence 

_Set the presence input:_ [_**dp\_ratesync\_loop\_set\_presence()**_](ratesync__core_8h.md#function-dp_ratesync_loop_set_presence) _on this object's loop._
```C++
void dp_ratesync_set_presence (
    dp_ratesync_state_t * state,
    int val
) 
```



Is the signal known to be there by evidence other than the symbol lock (a code lock and a carrier lock, in a DSSS receiver)? It decides where a lost symbol lock goes: coasting at the held rate with it, acquiring from a zero rate without. Lowering it during a coast ends the coast in acquiring.




**Parameters:**


* `state` Must be non-NULL. 
* `val` Non-zero: present. 




        

<hr>



### function dp\_ratesync\_set\_state 

_Restore mutable state from_ `blob` _into an identically built instance._
```C++
int dp_ratesync_set_state (
    dp_ratesync_state_t * state,
    const void * blob
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `blob` Blob from [**dp\_ratesync\_get\_state()**](ratesync__core_8h.md#function-dp_ratesync_get_state). 



**Returns:**

DP\_OK, or DP\_ERR\_INVALID if any envelope rejects. 





        

<hr>



### function dp\_ratesync\_set\_telemetry 

_Attach (or detach) a telemetry context and register the probes._ 
```C++
int dp_ratesync_set_telemetry (
    dp_ratesync_state_t * state,
    dp_tlm_t * tlm,
    const char * prefix,
    uint32_t decim
) 
```



Registers six probes, emitted once per recovered symbol and further thinned by `decim:` "&lt;prefix&gt;.e" (normalised TED error), "&lt;prefix&gt;.ctrl" (the per-input control steering the strobe), "&lt;prefix&gt;.rate" (tracked samples/symbol), "&lt;prefix&gt;.lock" (last block-averaged lock\_signal), "&lt;prefix&gt;.locked" (0/1) and "&lt;prefix&gt;.mu" (the timing NCO's fractional phase — see [**dp\_resamp\_get\_ctrl\_acc()**](resamp__core_8h.md#function-dp_resamp_get_ctrl_acc)). Passing NULL detaches. Setup path, never hot: the context is borrowed and must outlive the attachment (SPSC rules in [**dp\_tlm/dp\_tlm\_core.h**](dp__tlm__core_8h.md)).


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



### function dp\_ratesync\_state\_bytes 

_Bytes_ [_**dp\_ratesync\_get\_state()**_](ratesync__core_8h.md#function-dp_ratesync_get_state) _writes (envelope + payload + child)._
```C++
size_t dp_ratesync_state_bytes (
    const dp_ratesync_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Blob size in bytes: envelope + RateConverter blob + loop blob. 





        

<hr>



### function dp\_ratesync\_steps 

_Recover symbols from a block of oversampled cf32 baseband._ 
```C++
size_t dp_ratesync_steps (
    dp_ratesync_state_t * state,
    const float _Complex * x,
    size_t x_len,
    float _Complex * out,
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



### function dp\_ratesync\_steps\_max\_out 

_Output-buffer hint for the generated binding; 0 means "the input
length is already a safe bound" — with_ `sps >= m >= 2` _a block can never yield more symbols than it has samples (mirrors symsync)._
```C++
size_t dp_ratesync_steps_max_out (
    dp_ratesync_state_t * state
) 
```





**Parameters:**


* `state` Unused. 



**Returns:**

0. 





        

<hr>



### function ratesync\_loop\_take\_output 

_Fold one terminal-stage output into the timing loop._ 
```C++
JM_FORCEINLINE  JM_HOT int ratesync_loop_take_output (
    ratesync_loop_t * s,
    float _Complex y,
    float _Complex * y_out,
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



### function ratesync\_step 

_Per-input timing step (the inline composition API)._ 
```C++
JM_FORCEINLINE  JM_HOT int ratesync_step (
    dp_ratesync_state_t * s,
    float _Complex x,
    float _Complex * y_out
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
    dp_ratesync_state_t * s,
    float _Complex x,
    float _Complex * y_out,
    int ted
) 
```



The workhorse behind [**ratesync\_step()**](ratesync__core_8h.md#function-ratesync_step)/dp\_ratesync\_steps(). Pushes one input through the cascade at the current control deviation, which emits **up to two** terminal-stage outputs for that one input: `rate = m/sps <= 1` bounds it at two, and a terminal rate at or near 1.0 — what an integer `sps` plans — reaches that bound whenever the control has pushed the accumulator over. Taking only the first would permanently shift the strobe parity, which is why the output buffer holds several and this function drains all of them.


Every m-th output is an on-time strobe and the output m/2 back is the transition gate. On a strobe the TED compares the two, the PI loop steers the next control, and the on-time sample is the recovered symbol. With m &gt;= 2 at most one strobe can fall among a single input's outputs, so returning one symbol per input is still correct.


Passing a literal `ted` lets the force-inlined body constant-fold the detector branch away, exactly as [**symsync\_step\_ted()**](symsync__core_8h.md#function-symsync_step_ted) does.




**Parameters:**


* `s` State. Must be non-NULL. 
* `x` One input sample. 
* `y_out` Receives the symbol when the return is 1. 
* `ted` RATESYNC\_TED\_GARDNER or RATESYNC\_TED\_DTTL — pass a literal for a specialised (branch-free) instantiation. 



**Returns:**

1 if a symbol was emitted (into `y_out`), 0 otherwise. 





        

<hr>
## Public Static Functions Documentation




### function dp\_ratesync\_loop\_valid 

_Is the symbol just emitted valid: the loop tracking, no seed in progress and the lock declared. On a loop whose owner does not acquire it is the lock flag._ 
```C++
static inline int dp_ratesync_loop_valid (
    const ratesync_loop_t * l
) 
```





**Parameters:**


* `l` Must be non-NULL. 



**Returns:**

1 or 0. 





        

<hr>



### function ratesync\_rate\_bound 

_The steered-rate bound a timing loop's owner sets on its converter (RateConverter.ctrl\_rate\_max, doppler#3289)._ 
```C++
static inline double ratesync_rate_bound (
    size_t m
) 
```



The largest power of two at or under both `m` and RATESYNC\_FOLD\_MAX:
* at or under `m`: a strobe is every m-th output, so two need `m + 1` outputs from one input, and one input yields at most `ceil(rate + ctrl)`;
* at or under the fold: no input's outputs overflow the call's buffer into the converter's debt, so each comes out on its own input;
* a power of two: the resampler steps by `floor(2^32 / delta)`, which is exact only where `2^32 / delta` is an integer, so only there does a steer AT the bound yield exactly that many outputs and never one more (at 6, `6 floor(2^32/6) = 2^32 - 4`, and the phase walks down until one input yields 7).






**Parameters:**


* `m` Terminal outputs per symbol, &gt;= 2. 



**Returns:**

2 or 4. 





        

<hr>
## Macro Definition Documentation





### define RATESYNC\_FOLD\_MAX 

_Terminal outputs one input's call folds: the rest wait as the converter's debt (_ `ctrl_ahead` _) and come out on a later input. The buffer RateSync's and MpskReceiver's steps fold into._
```C++
#define RATESYNC_FOLD_MAX `4u`
```




<hr>



### define RATESYNC\_LOCK\_BUDGET 

_The per-decision false-transition budget the default verify counts are derived from: n\_up = dp\_det\_verify\_count(RATESYNC\_LOCK\_PFA,
budget), n\_down = dp\_det\_verify\_count(1 - RATESYNC\_LOCK\_PD, budget). 1e-6 is the library's convention for a detector's own default, not a new number: the Dll's declare budget at its default pfa, the async receiver's code-lock drop, and symsync's SYMSYNC\_LOCK\_BUDGET._ 
```C++
#define RATESYNC_LOCK_BUDGET `1e-6`
```




<hr>



### define RATESYNC\_LOCK\_EPS 

_Numerical guard on the on-time+mid energy sum feeding the lock statistic (not tunable) — mirrors SYMSYNC\_LOCK\_EPS._ 
```C++
#define RATESYNC_LOCK_EPS `1e-12`
```




<hr>



### define RATESYNC\_LOCK\_PD 

_See RATESYNC\_LOCK\_PFA._ 
```C++
#define RATESYNC_LOCK_PD `0.9`
```




<hr>



### define RATESYNC\_LOCK\_PFA 

_The lock detector's sizing rates per decision: the false-alarm probability and detection probability the 0.311 threshold is sized for (symsync's sizing, rolloff 0.35, Es/N0 10 dB)._ 
```C++
#define RATESYNC_LOCK_PFA `1e-3`
```




<hr>



### define RATESYNC\_LOOP\_STATE\_MAGIC 

_State-blob magic for the timing loop (FourCC "RSLP")._ 
```C++
#define RATESYNC_LOOP_STATE_MAGIC `DP_FOURCC ('R', 'S', 'L', 'P')`
```




<hr>



### define RATESYNC\_LOOP\_STATE\_VERSION 

_Timing-loop blob layout version._ 
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

_State-blob magic for RateSync (FourCC "RATS")._ 
```C++
#define RATESYNC_STATE_MAGIC `DP_FOURCC ('R', 'A', 'T', 'S')`
```




<hr>



### define RATESYNC\_STATE\_VERSION 

_RateSync blob layout version._ 
```C++
#define RATESYNC_STATE_VERSION `3u /* v3: the acquisition (its two settings, the stream counts, the estimator's child blob); v2: running state moved into the loop */`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/ratesync/ratesync_core.h`

