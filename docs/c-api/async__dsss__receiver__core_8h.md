

# File async\_dsss\_receiver\_core.h



[**FileList**](files.md) **>** [**async\_dsss\_receiver**](dir_385ab33ef0b6337dfa5d36daa80c4b8c.md) **>** [**async\_dsss\_receiver\_core.h**](async__dsss__receiver__core_8h.md)

[Go to the source code of this file](async__dsss__receiver__core_8h_source.md)

_Composed continuous DSSS receiver: Acquisition -&gt; handoff -&gt; CarrierAcquisition refine -&gt; Costas/Dll/RateConverter/ MpskReceiver track, one object._ [More...](#detailed-description)

* `#include "RateConverter/RateConverter_core.h"`
* `#include "acq/acq_core.h"`
* `#include "carrier_acq/carrier_acq_core.h"`
* `#include "cic/cic_core.h"`
* `#include "costas/costas_core.h"`
* `#include "dll/dll_core.h"`
* `#include "dp_state.h"`
* `#include "hbdecim/hbdecim_core.h"`
* `#include "lockdet/lockdet_core.h"`
* `#include "mpsk_receiver/mpsk_receiver_core.h"`
* `#include "resamp/resamp_core.h"`
* `#include "resample/resample_core.h"`
* `#include <complex.h>`
* `#include <stdbool.h>`
* `#include <stddef.h>`
* `#include "psd/psd_core.h"`
* `#include "detector/detector_core.h"`
* `#include "detection/detection_core.h"`
* `#include "spectral/spectral_core.h"`
* `#include "corr/corr_core.h"`
* `#include "fft/fft_core.h"`
* `#include "acc_trace/acc_trace_core.h"`
* `#include "ber/ber_core.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**async\_dsss\_receiver\_extra\_t**](structasync__dsss__receiver__extra__t.md) <br> |
| struct | [**async\_dsss\_receiver\_state\_t**](structasync__dsss__receiver__state__t.md) <br>_Composed receiver state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  int | [**async\_dsss\_receiver\_configure\_chain\_raw**](#function-async_dsss_receiver_configure_chain_raw) ([**async\_dsss\_receiver\_state\_t**](structasync__dsss__receiver__state__t.md) \* state, size\_t segments, size\_t sps, int n) <br>_Pin the live-tracking despread/resample/demod grid directly. The escape hatch for the composition-specific knob:_ `segments` _(the live Dll's tracking parameter) and_`sps` _/_`n` _(MpskReceiver's sample-rate/carrier-arm parameters) are independently overridable, still bridged by a freshly-sized_`RateConverter` _and never coupled to each other. While searching/refining it re-pins the grid used to build the next tracking chain; once tracking it rebuilds_`dll` _/_`rc` _/_`rx` _in place, allocating every replacement before adopting it so a failed pin leaves the receiver usable on its prior grid._ |
|  void | [**async\_dsss\_receiver\_configure\_lock\_raw**](#function-async_dsss_receiver_configure_lock_raw) ([**async\_dsss\_receiver\_state\_t**](structasync__dsss__receiver__state__t.md) \* state, double up\_thresh, double down\_thresh, size\_t n\_looks, double alpha, uint32\_t n\_up, uint32\_t n\_down) <br>_Re-tune the live-tracking Dll's code-lock detector directly. Forwards to_ `dll_configure_lock_raw()` _on the live tracking Dll (the one behind_`get_code_locked()` _), NOT the refine-stage collection Dll. Only meaningful once tracking has begun; a no-op while searching or refining. The detector is the hysteretic lockdet over the DLL's per-N-look CFAR statistic — the levels and verify counts trade declare latency against false-alarm rate._ |
|  int | [**async\_dsss\_receiver\_configure\_search\_raw**](#function-async_dsss_receiver_configure_search_raw) ([**async\_dsss\_receiver\_state\_t**](structasync__dsss__receiver__state__t.md) \* state, size\_t doppler\_bins, size\_t n\_noncoh) <br>_Pin the embedded Acquisition's search grid directly. Forwards to_ `acq_configure_search_raw()` _— the escape hatch under this object's_`symbol_rate` _-driven auto-sizing, for a power user who wants a specific_`(doppler_bins, n_noncoh)` _. Only meaningful while searching; the acquisition search does not run again until the next_`reset()` _._ |
|  [**async\_dsss\_receiver\_state\_t**](structasync__dsss__receiver__state__t.md) \* | [**async\_dsss\_receiver\_create**](#function-async_dsss_receiver_create) (const uint8\_t \* code, size\_t code\_len, double chip\_rate, double symbol\_rate, size\_t spc, int m, double cn0\_dbhz, double pfa, double pd, double doppler\_uncertainty, size\_t segments, size\_t sps, int differential, double refine\_max\_error\_db, size\_t refine\_samples\_per\_symbol, double refine\_design\_margin\_db, size\_t refine\_n\_fft, size\_t refine\_zero\_pad, bool refine\_sequential, size\_t refine\_max\_n\_blocks, double carrier\_freq\_hz, double lost\_confirm\_s) <br>_Create an AsyncDsssReceiver in the searching state._  |
|  [**async\_dsss\_receiver\_state\_t**](structasync__dsss__receiver__state__t.md) \* | [**async\_dsss\_receiver\_create\_handoff**](#function-async_dsss_receiver_create_handoff) (const uint8\_t \* code, size\_t code\_len, double chip\_rate, double symbol\_rate, size\_t spc, int m, double cn0\_dbhz, double pfa, double pd, size\_t segments, size\_t sps, int differential, double refine\_max\_error\_db, size\_t refine\_samples\_per\_symbol, double refine\_design\_margin\_db, size\_t refine\_n\_fft, size\_t refine\_zero\_pad, bool refine\_sequential, size\_t refine\_max\_n\_blocks, double carrier\_freq\_hz, double lost\_confirm\_s) <br>_Create a receiver in hand-off mode: idle, with no search of its own._  |
|  void | [**async\_dsss\_receiver\_destroy**](#function-async_dsss_receiver_destroy) ([**async\_dsss\_receiver\_state\_t**](structasync__dsss__receiver__state__t.md) \* state) <br>_Destroy a receiver and release every child._  |
|  double | [**async\_dsss\_receiver\_get\_car\_last\_error**](#function-async_dsss_receiver_get_car_last_error) (const [**async\_dsss\_receiver\_state\_t**](structasync__dsss__receiver__state__t.md) \* state) <br>_Pre-despread Costas phase discriminator (rad): the residual carrier phase LOOP 1 (which de-rotates before the Dll) is not nulling._  |
|  double | [**async\_dsss\_receiver\_get\_car\_nco\_freq**](#function-async_dsss_receiver_get_car_nco_freq) (const [**async\_dsss\_receiver\_state\_t**](structasync__dsss__receiver__state__t.md) \* state) <br>_LOOP 1 (pre-despread Costas) loop-filter output = NCO frequency command, cycles/sample of the front-end (chip\_rate\*spc) rate._  |
|  double | [**async\_dsss\_receiver\_get\_chip\_phase**](#function-async_dsss_receiver_get_chip_phase) (const [**async\_dsss\_receiver\_state\_t**](structasync__dsss__receiver__state__t.md) \* state) <br> |
|  double | [**async\_dsss\_receiver\_get\_cn0\_dbhz\_est**](#function-async_dsss_receiver_get_cn0_dbhz_est) (const [**async\_dsss\_receiver\_state\_t**](structasync__dsss__receiver__state__t.md) \* state) <br> |
|  int | [**async\_dsss\_receiver\_get\_code\_locked**](#function-async_dsss_receiver_get_code_locked) (const [**async\_dsss\_receiver\_state\_t**](structasync__dsss__receiver__state__t.md) \* state) <br>_Binary code-lock flag from the live tracking Dll's own verify-counted (pfa-tuned) lock detector — the fundamental DSSS "am I
despreading" lock, de-chattered by up/down hysteresis._  |
|  double | [**async\_dsss\_receiver\_get\_code\_rate**](#function-async_dsss_receiver_get_code_rate) (const [**async\_dsss\_receiver\_state\_t**](structasync__dsss__receiver__state__t.md) \* state) <br> |
|  double | [**async\_dsss\_receiver\_get\_doppler\_hz**](#function-async_dsss_receiver_get_doppler_hz) (const [**async\_dsss\_receiver\_state\_t**](structasync__dsss__receiver__state__t.md) \* state) <br> |
|  int | [**async\_dsss\_receiver\_get\_idle**](#function-async_dsss_receiver_get_idle) (const [**async\_dsss\_receiver\_state\_t**](structasync__dsss__receiver__state__t.md) \* state) <br>_1 while waiting for a seed (hand-off mode, before seed() or after reset()); 0 in every other state._  |
|  double | [**async\_dsss\_receiver\_get\_lock**](#function-async_dsss_receiver_get_lock) (const [**async\_dsss\_receiver\_state\_t**](structasync__dsss__receiver__state__t.md) \* state) <br> |
|  double | [**async\_dsss\_receiver\_get\_lock\_metric**](#function-async_dsss_receiver_get_lock_metric) (const [**async\_dsss\_receiver\_state\_t**](structasync__dsss__receiver__state__t.md) \* state) <br>_Symbol-lock metric = SNR-weighted EMA of (I^2-Q^2)/(I^2+Q^2) = cos(2\*phi) over the emitted symbols (locked -&gt; ~+1). Drives_ `locked` _._ |
|  double | [**async\_dsss\_receiver\_get\_lock\_threshold**](#function-async_dsss_receiver_get_lock_threshold) (const [**async\_dsss\_receiver\_state\_t**](structasync__dsss__receiver__state__t.md) \* state) <br>_The lock-metric declare threshold_ `locked` _latches above (the lockdet up\_thresh); exposed alongside lock\_metric for engineering debug._ |
|  int | [**async\_dsss\_receiver\_get\_locked**](#function-async_dsss_receiver_get_locked) (const [**async\_dsss\_receiver\_state\_t**](structasync__dsss__receiver__state__t.md) \* state) <br>_Binary carrier-lock flag from the loop's hysteretic (up/down verify-counted) lock detector — the de-chattered lock indicator, unlike the raw_ `lock` _metric._ |
|  int | [**async\_dsss\_receiver\_get\_lost**](#function-async_dsss_receiver_get_lost) (const [**async\_dsss\_receiver\_state\_t**](structasync__dsss__receiver__state__t.md) \* state) <br>_1 once the release rule has fired: both lock flags were down, continuously, for longer than_ `lost_confirm_s` _while tracking_ _an emitter that left, or a seed that never locked. The loops have stopped; the holder releases the assignment and calls reset(). 0 in every other state, and always 0 with_`lost_confirm_s = 0` _._ |
|  double | [**async\_dsss\_receiver\_get\_mpsk\_last\_error**](#function-async_dsss_receiver_get_mpsk_last_error) (const [**async\_dsss\_receiver\_state\_t**](structasync__dsss__receiver__state__t.md) \* state) <br>_MpskReceiver carrier phase discriminator (rad): the residual carrier phase LOOP 2 (post-despread) is not nulling._  |
|  int | [**async\_dsss\_receiver\_get\_n**](#function-async_dsss_receiver_get_n) (const [**async\_dsss\_receiver\_state\_t**](structasync__dsss__receiver__state__t.md) \* state) <br> |
|  double | [**async\_dsss\_receiver\_get\_nco\_freq**](#function-async_dsss_receiver_get_nco_freq) (const [**async\_dsss\_receiver\_state\_t**](structasync__dsss__receiver__state__t.md) \* state) <br>_Live carrier loop-filter output = NCO frequency command (cycles/sample of the MpskReceiver output rate). Its mean tracks a Doppler ramp with no lag (unlike get\_norm\_freq's integrator estimate); its variance is the carrier loop stress._  |
|  double | [**async\_dsss\_receiver\_get\_norm\_freq**](#function-async_dsss_receiver_get_norm_freq) (const [**async\_dsss\_receiver\_state\_t**](structasync__dsss__receiver__state__t.md) \* state) <br> |
|  int | [**async\_dsss\_receiver\_get\_refining**](#function-async_dsss_receiver_get_refining) (const [**async\_dsss\_receiver\_state\_t**](structasync__dsss__receiver__state__t.md) \* state) <br> |
|  size\_t | [**async\_dsss\_receiver\_get\_segments**](#function-async_dsss_receiver_get_segments) (const [**async\_dsss\_receiver\_state\_t**](structasync__dsss__receiver__state__t.md) \* state) <br> |
|  size\_t | [**async\_dsss\_receiver\_get\_sps**](#function-async_dsss_receiver_get_sps) (const [**async\_dsss\_receiver\_state\_t**](structasync__dsss__receiver__state__t.md) \* state) <br> |
|  void | [**async\_dsss\_receiver\_get\_state**](#function-async_dsss_receiver_get_state) (const [**async\_dsss\_receiver\_state\_t**](structasync__dsss__receiver__state__t.md) \* state, void \* blob) <br> |
|  int | [**async\_dsss\_receiver\_get\_tracking**](#function-async_dsss_receiver_get_tracking) (const [**async\_dsss\_receiver\_state\_t**](structasync__dsss__receiver__state__t.md) \* state) <br> |
|  void | [**async\_dsss\_receiver\_reset**](#function-async_dsss_receiver_reset) ([**async\_dsss\_receiver\_state\_t**](structasync__dsss__receiver__state__t.md) \* state) <br>_Return to the searching state_  _or, in hand-off mode, to idle. Resets the embedded Acquisition (if any) and rebuilds both the refine-stage and live-tracking chains back to their placeholder seed (phase 0, no Doppler). A receiver that has locked cannot be "reset back
to tracking the same signal," only back to searching — matching every other object's reset() semantics in this codebase. In hand-off mode there is no search to return to, so this is how the holder of a pool releases a lost receiver for its next seed, with no reallocation._ |
|  int | [**async\_dsss\_receiver\_seed**](#function-async_dsss_receiver_seed) ([**async\_dsss\_receiver\_state\_t**](structasync__dsss__receiver__state__t.md) \* state, double chip\_phase, double doppler\_hz\_est, double cn0\_dbhz\_est) <br>_Take a detection from outside and start refining from it._  |
|  int | [**async\_dsss\_receiver\_set\_state**](#function-async_dsss_receiver_set_state) ([**async\_dsss\_receiver\_state\_t**](structasync__dsss__receiver__state__t.md) \* state, const void \* blob) <br> |
|  size\_t | [**async\_dsss\_receiver\_state\_bytes**](#function-async_dsss_receiver_state_bytes) (const [**async\_dsss\_receiver\_state\_t**](structasync__dsss__receiver__state__t.md) \* state) <br> |
|  size\_t | [**async\_dsss\_receiver\_steps**](#function-async_dsss_receiver_steps) ([**async\_dsss\_receiver\_state\_t**](structasync__dsss__receiver__state__t.md) \* state, const float complex \* x, size\_t x\_len, float complex \* out, size\_t max\_out) <br>_Stream raw cf32 samples; emit demodulated symbols once tracking._  |
|  size\_t | [**async\_dsss\_receiver\_steps\_max\_out**](#function-async_dsss_receiver_steps_max_out) ([**async\_dsss\_receiver\_state\_t**](structasync__dsss__receiver__state__t.md) \* state) <br> |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**ASYNC\_DSSS\_RECEIVER\_STATE\_MAGIC**](async__dsss__receiver__core_8h.md#define-async_dsss_receiver_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('A', 'D', 'R', 'X')`<br> |
| define  | [**ASYNC\_DSSS\_RECEIVER\_STATE\_VERSION**](async__dsss__receiver__core_8h.md#define-async_dsss_receiver_state_version)  `3u`<br> |
| define  | [**ASYNC\_DSSS\_RX\_BN\_CARRIER**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_bn_carrier)  `0.04`<br> |
| define  | [**ASYNC\_DSSS\_RX\_DLL\_BN**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_dll_bn)  `0.002`<br> |
| define  | [**ASYNC\_DSSS\_RX\_IDLE**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_idle)  `3`<br> |
| define  | [**ASYNC\_DSSS\_RX\_LOCK\_DOWN**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_lock_down)  `0.3`<br> |
| define  | [**ASYNC\_DSSS\_RX\_LOCK\_DWELL**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_lock_dwell)  `30u`<br> |
| define  | [**ASYNC\_DSSS\_RX\_LOCK\_N\_DOWN**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_lock_n_down)  `15u`<br> |
| define  | [**ASYNC\_DSSS\_RX\_LOCK\_N\_UP**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_lock_n_up)  `30u`<br> |
| define  | [**ASYNC\_DSSS\_RX\_LOCK\_UP**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_lock_up)  `0.5`<br> |
| define  | [**ASYNC\_DSSS\_RX\_LOST**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_lost)  `4`<br> |
| define  | [**ASYNC\_DSSS\_RX\_REFINING**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_refining)  `1`<br> |
| define  | [**ASYNC\_DSSS\_RX\_SEARCHING**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_searching)  `0`<br> |
| define  | [**ASYNC\_DSSS\_RX\_TRACKING**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_tracking)  `2`<br> |

## Detailed Description


The production C port of the validated Python prototype's own search -&gt; refine -&gt; track pipeline (validated in the coupled-despreader, freq-refine, and end-to-end acquisition prototypes). Unlike `DsssReceiver` (which goes straight from an acquisition hit to tracking with the hit's own coarse Doppler estimate), this object inserts a REFINING stage between the two, closing a low-Es/N0 pull-in gap the coarse-only estimate leaves at large static Doppler offsets:



* **searching** (`get_tracking() == 0 && get_refining() == 0`): samples feed the embedded `Acquisition`. On a hit, `acq_build_handoff()` seeds the refine-stage chain (a FROZEN carrier derotation  `costas_wipeoff()` at the coarse estimate, `costas_update()` never called, the direct C equivalent of the Python prototype's `freeze_carrier=True`  feeding a collection `Dll` whose `dll_lookback_segments(refine_max_error_db)` windows OVERSAMPLE each epoch with coherent integrate-and-dump dumps  the asynchronous data's residual carrier rides a ~symbol\_rate-wide spectrum that a single per-epoch dump would undersample and alias (see the `refine_max_error_db` doc comment on `async_dsss_receiver_create()`), then a `RateConverter` to `CarrierAcquisition`'s own operating rate, then `CarrierAcquisition` itself), and the unconsumed tail of the same call is handed straight to it.
* **refining** (`get_refining() == 1`): samples feed the refine-stage chain. Every call, `CarrierAcquisition`'s own `ready`/give-up state is checked; once either fires, the live tracking chain is built FRESH (mirroring the already-learned "rebuild fresh, don't nudge in
    place" lesson)  seeded from the ORIGINAL handoff chip phase (not wherever the refine-stage `Dll` drifted to) and the refined (or, on a give-up, unrefined) Doppler estimate  and the object transitions to tracking.
* **tracking** (`get_tracking() == 1`): the refined carrier estimate is UNFROZEN into a live pre-despread carrier loop (`costas_wipeoff`/`costas_update`) -&gt; `Dll` -&gt; `RateConverter` -&gt; `MpskReceiver`  the "track" leg of coarse -&gt; freeze -&gt; refine -&gt; unfreeze/track. `costas_update()` runs once per code period, driven by a NON-DATA-AIDED (squaring) discriminator over that period's coherent- I&D partials (`adr_track_period()`): a code period spans ~0.9 data symbols at SPEC's async ratio, so a transition lands inside nearly every period, and squaring is what makes the carrier error transition-robust (a decision-directed sign-aligned combine, tried first, thrashed +/-57deg and averaged to zero, so loop 1 never tracked and the post-despread MpskReceiver loop silently inherited the whole carrier + its Type-II ramp phase error). With that clean error and a bandwidth wide enough to pull the refined seed in and ride the ramp (`ASYNC_DSSS_RX_BN_CARRIER`), the pre-despread loop removes the FULL coupled Doppler (offset AND 500 Hz/s ramp), so despreading is coherent and MpskReceiver is left only a small residual. (Pure PLL  no FLL anywhere, see the `ASYNC_DSSS_RX_BN_CARRIER` comment.)
* **idle** (`get_idle() == 1`, hand-off mode only): waiting for a seed. Samples are consumed and discarded, so a feeding loop needs no special case.
* **lost** (`get_lost() == 1`): the emitter is gone. Entered from tracking when BOTH lock flags have been down, without a break, for longer than `lost_confirm_s` (docs/design/async-dsss-receiver.md section 11.2); the loops stop updating and samples are discarded until `reset()`. One flag down is a degrade, reported by the flags and not acted on. `lost_confirm_s = 0` (the searching flavor's default) never enters it.




**Hand-off mode** (`async_dsss_receiver_create_handoff()`) is the same object with NO embedded `Acquisition`: the search is somebody else's  a searcher covering one channel for every emitter on it  and the receiver takes its detection from outside through `async_dsss_receiver_seed()`, exactly the record its own hit would have produced. It starts idle, `reset()` returns it to idle, and the searching branch of `steps()` is unreachable. `seed()` is a method of BOTH flavors (a hit is a seed the object made for itself), and it refuses on a receiver that already holds one: "assigned once" is enforced here, not by the caller's discipline.


Both the refine and track stages share ONE carrier-wipe scratch/carry buffer set (`car_wiped_buf`/`car_carry_buf`/`car_carry_len`, sized `tsamps = code_len*spc`) since they never run concurrently.



```C++
async_dsss_receiver_state_t *rx = async_dsss_receiver_create(
    code, code_len, 3.0e6, 2100.0,   // chip_rate, symbol_rate
    2, 2,                            // spc, m (BPSK)
    55.0, 1e-3, 0.9, 100.0,          // cn0_dbhz, pfa, pd,
                                     // doppler_uncertainty
    4, 8, 0,                         // segments, sps, differential
    0.5, 4, 14.0, 64, 8, false, 100000,  // refine_* tuning
    0.0,                             // carrier_freq_hz (0 = aiding off)
    0.0);                            // lost_confirm_s (0 = never lost)
float complex syms[4096];
size_t n = async_dsss_receiver_steps(rx, x, x_len, syms, 4096);
async_dsss_receiver_destroy(rx);
```
 


    
## Public Functions Documentation




### function async\_dsss\_receiver\_configure\_chain\_raw 

_Pin the live-tracking despread/resample/demod grid directly. The escape hatch for the composition-specific knob:_ `segments` _(the live Dll's tracking parameter) and_`sps` _/_`n` _(MpskReceiver's sample-rate/carrier-arm parameters) are independently overridable, still bridged by a freshly-sized_`RateConverter` _and never coupled to each other. While searching/refining it re-pins the grid used to build the next tracking chain; once tracking it rebuilds_`dll` _/_`rc` _/_`rx` _in place, allocating every replacement before adopting it so a failed pin leaves the receiver usable on its prior grid._
```C++
int async_dsss_receiver_configure_chain_raw (
    async_dsss_receiver_state_t * state,
    size_t segments,
    size_t sps,
    int n
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `segments` Live-tracking Dll segments per code period. 
* `sps` MpskReceiver samples per symbol (the resample target). 
* `n` MpskReceiver's carrier-arm count; must divide `sps`. 



**Returns:**

0 on success, -1 on invalid grid or an allocation failure (the receiver is left usable at its prior grid on failure). 
```C++
>>> import numpy as np
>>> from doppler.dsss import AsyncDsssReceiver
>>> from doppler.wfm import Gold
>>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
>>> rx = AsyncDsssReceiver(code, chip_rate=3.069e6, symbol_rate=2700.0,
...                        spc=2, doppler_uncertainty=500.0)
>>> rx.configure_chain_raw(segments=6, sps=8, n=8)  # re-pin the chain
>>> rx.segments                       # tracking grid updated in place
6
```
 





        

<hr>



### function async\_dsss\_receiver\_configure\_lock\_raw 

_Re-tune the live-tracking Dll's code-lock detector directly. Forwards to_ `dll_configure_lock_raw()` _on the live tracking Dll (the one behind_`get_code_locked()` _), NOT the refine-stage collection Dll. Only meaningful once tracking has begun; a no-op while searching or refining. The detector is the hysteretic lockdet over the DLL's per-N-look CFAR statistic — the levels and verify counts trade declare latency against false-alarm rate._
```C++
void async_dsss_receiver_configure_lock_raw (
    async_dsss_receiver_state_t * state,
    double up_thresh,
    double down_thresh,
    size_t n_looks,
    double alpha,
    uint32_t n_up,
    uint32_t n_down
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `up_thresh` CFAR-statistic level to declare code lock (hit when the statistic exceeds it). 
* `down_thresh` Level below which a look is a miss; choose &lt;= `up_thresh` for level hysteresis. 
* `n_looks` Looks per decision — the DLL's non-coherent integration depth feeding one statistic. 
* `alpha` EMA smoothing coefficient on the lock statistic (0..1); smaller is smoother/slower. 
* `n_up` Consecutive hits required to declare lock. 
* `n_down` Consecutive misses required to drop lock. 
```C++
>>> import numpy as np
>>> from doppler.dsss import AsyncDsssReceiver
>>> from doppler.wfm import Gold
>>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
>>> rx = AsyncDsssReceiver(code, chip_rate=3.069e6, symbol_rate=2700.0,
...                        spc=2, doppler_uncertainty=500.0)
>>> rx.configure_lock_raw(up_thresh=0.4, down_thresh=0.2, n_looks=20,
...                       alpha=0.1, n_up=5, n_down=3)
>>> rx.tracking                       # a no-op until tracking begins
0
```
 




        

<hr>



### function async\_dsss\_receiver\_configure\_search\_raw 

_Pin the embedded Acquisition's search grid directly. Forwards to_ `acq_configure_search_raw()` _— the escape hatch under this object's_`symbol_rate` _-driven auto-sizing, for a power user who wants a specific_`(doppler_bins, n_noncoh)` _. Only meaningful while searching; the acquisition search does not run again until the next_`reset()` _._
```C++
int async_dsss_receiver_configure_search_raw (
    async_dsss_receiver_state_t * state,
    size_t doppler_bins,
    size_t n_noncoh
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `doppler_bins` Number of Doppler window tiles to search (&gt;= 1); capped by the create-time `doppler_uncertainty` span (one tile per code-epoch Doppler bin width). 
* `n_noncoh` Non-coherent looks accumulated per grid cell (1..256); more looks buys sensitivity at the cost of dwell, replacing the auto-sized count. 



**Returns:**

0 on success, -1 on invalid grid (see acq\_configure\_search\_raw) or in hand-off mode, which has no search to pin. 
```C++
>>> import numpy as np
>>> from doppler.dsss import AsyncDsssReceiver
>>> from doppler.wfm import Gold
>>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
>>> rx = AsyncDsssReceiver(code, chip_rate=3.069e6, symbol_rate=2700.0,
...                        spc=2, doppler_uncertainty=500.0)
>>> rx.configure_search_raw(doppler_bins=1, n_noncoh=16)  # pin it
>>> rx.refining                # still searching, on the pinned grid
0
```
 





        

<hr>



### function async\_dsss\_receiver\_create 

_Create an AsyncDsssReceiver in the searching state._ 
```C++
async_dsss_receiver_state_t * async_dsss_receiver_create (
    const uint8_t * code,
    size_t code_len,
    double chip_rate,
    double symbol_rate,
    size_t spc,
    int m,
    double cn0_dbhz,
    double pfa,
    double pd,
    double doppler_uncertainty,
    size_t segments,
    size_t sps,
    int differential,
    double refine_max_error_db,
    size_t refine_samples_per_symbol,
    double refine_design_margin_db,
    size_t refine_n_fft,
    size_t refine_zero_pad,
    bool refine_sequential,
    size_t refine_max_n_blocks,
    double carrier_freq_hz,
    double lost_confirm_s
) 
```



Only `code`/`chip_rate`/`symbol_rate` describe the signal itself. `refine_*` parameters mirror `freq_refine.refine_seed_carrier_acq()`'s own already-validated defaults (see `objects/async_dsss_receiver.toml` for the rationale behind each one)  a power user can override, but the defaults are sized to close SPEC's own 4-5dB/500Hz-s combined gating scenario as-is.




**Parameters:**


* `code` Spreading code, one 0/1 chip per element (0 -&gt; +1, 1 -&gt; -1 BPSK; only the low bit is used, so pass 0/1, not +/-1). 
* `code_len` Chips in `code`. 
* `chip_rate` Chip rate, Hz. Required. 
* `symbol_rate` Data-symbol rate, Hz. Required. 
* `spc` Samples/chip; default 2. 
* `m` PSK order, 2/4/8; default 2 (BPSK). 
* `cn0_dbhz` Design C/N0, dB-Hz; default 55.0  feeds BOTH the embedded Acquisition's own sizing AND (derated by `refine_design_margin_db`) CarrierAcquisition's `design_snr`. 
* `pfa` Acquisition false-alarm target; default 1e-3. Also CarrierAcquisition's own `pfa`. 
* `pd` Acquisition detection-probability target; default 0.9. Also CarrierAcquisition's own `pd`. 
* `doppler_uncertainty` One-sided Doppler search half-range, Hz; default 100.0. 
* `segments` Live-tracking Dll's own segments; default 4. 
* `sps` MpskReceiver's samples/symbol; default 8. 
* `differential` MpskReceiver's differential demap; default 0 (coherent). 
* `refine_max_error_db` Max tolerable async-lookback correlation-power loss driving the refine-stage collection Dll's coherent-I&D window count via [**dll\_lookback\_segments()**](dll__core_8h.md#function-dll_lookback_segments). Oversampling the epoch is required for the asynchronous data: the residual carrier rides a ~symbol\_rate-wide data-modulated spectrum, so segments&gt;1 (default yields 11 at tsamps=2046) samples it above Nyquist; segments=1 undersamples and aliases it. Default 0.5. 
* `refine_samples_per_symbol` CarrierAcquisition's own operating rate = this \* symbol\_rate; default 4. 
* `refine_design_margin_db` Empirical derating of cn0\_dbhz before CarrierAcquisition's design\_snr; default 14.0. 
* `refine_n_fft` CarrierAcquisition's own block size; default 64. 
* `refine_zero_pad` CarrierAcquisition's own zero\_pad; default 8. 
* `refine_sequential` CarrierAcquisition's own sequential mode; default false  sequential mode's early per-block test fires on far too little averaging at SPEC's own Es/N0 floor (confirmed: as few as 4 blocks, 150-200+ Hz off); false waits the full design\_snr-derived dwell\_target, matching freq\_refine.refine\_seed\_ carrier\_acq()'s own validated default. 
* `refine_max_n_blocks` CarrierAcquisition's own give-up cap in sequential mode; default 100000. 
* `carrier_freq_hz` Nominal RF carrier frequency, Hz, enabling carrier-&gt;code aiding; 0.0 (default) = off. When &gt; 0, the coupled code-rate Doppler (carrier\_offset/carrier\_freq) is fed to the tracking Dll via [**dll\_set\_rate\_aid()**](dll__core_8h.md#function-dll_set_rate_aid) so the code loop rides a dilated clock the discriminator alone can't pull in at low SNR. Set to the receiver's own downlink RF frequency for a physically-coupled Doppler capture. 
* `lost_confirm_s` Release rule: both lock flags down, continuously, for longer than this many seconds puts the receiver in the lost state (see get\_lost()). Size it past the longest fade the link must ride. The clock also runs from the first tracking sample, when neither flag is up yet, so a hand-off that never locks within the interval is released the same way as an emitter that leaves. Default 0.0 = never  the searching flavor's exit is reset(), as before. 
```C++
>>> import numpy as np
>>> from doppler.dsss import AsyncDsssReceiver
>>> from doppler.wfm import Gold
>>> sf, chip, sym, spc = 1023, 3.069e6, 2700.0, 2
>>> fs, te, tsym = chip * spc, sf * spc, chip * spc / sym
>>> code = np.asarray(Gold().generate(sf)).astype(np.uint8)
>>> csign = np.where(code & 1, -1.0, 1.0)
>>> rng = np.random.default_rng(21)
>>> n = int(600 * tsym) + 4 * te            # 600 async BPSK symbols
>>> idx = np.arange(n)
>>> data = (rng.integers(0, 2, 604) * 2 - 1).astype(float)
>>> si = np.clip((idx / tsym).astype(int), 0, 603)
>>> t = idx / fs

DSSS chips on a carrier sweeping at 500 Hz/s — the ramp the async
receiver has to track:

>>> sig = (data[si] * csign[(idx // spc) % sf]
...        * np.exp(1j * 2 * np.pi * 0.5 * 500.0 * t * t))
>>> cn0 = 20.0 + 10 * np.log10(sym)         # Es/N0 = 20 dB
>>> sigma = np.sqrt(fs / 10 ** (cn0 / 10))
>>> pre = 5 * te                            # noise-only lead-in
>>> noise = (sigma / np.sqrt(2)) * (rng.standard_normal(pre + n)
...          + 1j * rng.standard_normal(pre + n))
>>> x = (np.concatenate([np.zeros(pre), sig]).astype(np.complex64)
...      + noise.astype(np.complex64))
>>> rx = AsyncDsssReceiver(
...     code, chip_rate=chip, symbol_rate=sym, spc=spc,
...     cn0_dbhz=cn0, doppler_uncertainty=500.0)
>>> syms = [rx.steps(x[p:p + te]) for p in range(0, len(x) - te, te)]
>>> syms = np.concatenate([s for s in syms if len(s)])
>>> rx.tracking                  # searched, refined, now tracking
1
>>> len(syms) > 300              # symbols recovered under the ramp
True

Nearly all the energy lands on I, so the BPSK phase is resolved:

>>> bool(np.mean(syms.real**2) > 10 * np.mean(syms.imag**2))
True
```
 




        

<hr>



### function async\_dsss\_receiver\_create\_handoff 

_Create a receiver in hand-off mode: idle, with no search of its own._ 
```C++
async_dsss_receiver_state_t * async_dsss_receiver_create_handoff (
    const uint8_t * code,
    size_t code_len,
    double chip_rate,
    double symbol_rate,
    size_t spc,
    int m,
    double cn0_dbhz,
    double pfa,
    double pd,
    size_t segments,
    size_t sps,
    int differential,
    double refine_max_error_db,
    size_t refine_samples_per_symbol,
    double refine_design_margin_db,
    size_t refine_n_fft,
    size_t refine_zero_pad,
    bool refine_sequential,
    size_t refine_max_n_blocks,
    double carrier_freq_hz,
    double lost_confirm_s
) 
```



The pool shape of docs/design/async-dsss-receiver.md section 11.1: one searcher finds every emitter on the channel, and one of these per emitter tracks it from the searcher's detection. No `Acquisition` is built (a 20-to-50-tile engine per receiver, a dozen times over, is memory and work nothing would use), so there is no `doppler_uncertainty` and `configure_search_raw()` returns -1. The receiver starts idle and consumes samples without effect until [**async\_dsss\_receiver\_seed()**](async__dsss__receiver__core_8h.md#function-async_dsss_receiver_seed) gives it a detection, after which the refine -&gt; track chain is the searching flavor's, verbatim.


Every parameter is [**async\_dsss\_receiver\_create()**](async__dsss__receiver__core_8h.md#function-async_dsss_receiver_create)'s, minus the search half-range; `pfa`/`pd` still size `CarrierAcquisition`. The one default that differs is `lost_confirm_s`: 2.0 s, so an emitter that leaves is reported gone (get\_lost()) and the holder can release the receiver  against 5-to-15-minute on-times, two seconds past the measured fades costs nothing (section 12.3).




**Parameters:**


* `code` Spreading code, 0/1 chips (see [**async\_dsss\_receiver\_create()**](async__dsss__receiver__core_8h.md#function-async_dsss_receiver_create)). 
* `code_len` Chips in `code`. 
* `chip_rate` Chip rate, Hz. Required. 
* `symbol_rate` Data-symbol rate, Hz. Required. 
* `spc` Samples/chip; default 2. 
* `m` PSK order, 2/4/8; default 2. 
* `cn0_dbhz` Design C/N0, dB-Hz; default 55.0 (derated by `refine_design_margin_db` into CarrierAcquisition's design\_snr). 
* `pfa` CarrierAcquisition's false-alarm target; default 1e-3. 
* `pd` CarrierAcquisition's detection target; default 0.9. 
* `segments` Live-tracking Dll's segments; default 4. 
* `sps` MpskReceiver's samples/symbol; default 8. 
* `differential` MpskReceiver's differential demap; default 0. 
* `refine_max_error_db` As [**async\_dsss\_receiver\_create()**](async__dsss__receiver__core_8h.md#function-async_dsss_receiver_create). 
* `refine_samples_per_symbol` As [**async\_dsss\_receiver\_create()**](async__dsss__receiver__core_8h.md#function-async_dsss_receiver_create). 
* `refine_design_margin_db` As [**async\_dsss\_receiver\_create()**](async__dsss__receiver__core_8h.md#function-async_dsss_receiver_create). 
* `refine_n_fft` As [**async\_dsss\_receiver\_create()**](async__dsss__receiver__core_8h.md#function-async_dsss_receiver_create). 
* `refine_zero_pad` As [**async\_dsss\_receiver\_create()**](async__dsss__receiver__core_8h.md#function-async_dsss_receiver_create). 
* `refine_sequential` As [**async\_dsss\_receiver\_create()**](async__dsss__receiver__core_8h.md#function-async_dsss_receiver_create). 
* `refine_max_n_blocks` As [**async\_dsss\_receiver\_create()**](async__dsss__receiver__core_8h.md#function-async_dsss_receiver_create). 
* `carrier_freq_hz` Nominal RF carrier for carrier-&gt;code aiding; 0.0 (default) = off. 
* `lost_confirm_s` Release rule, seconds of both flags down; default 2.0. 0 = never lost. 
```C++
>>> import numpy as np
>>> from doppler.dsss import Acquisition, HandoffAsyncDsssReceiver
>>> from doppler.dsss import bin_to_signed
>>> from doppler.dsss.handoff import dll_init_chip_from_acq
>>> from doppler.wfm import Gold
>>> sf, chip, sym, spc = 1023, 3.069e6, 2700.0, 2
>>> fs, te, tsym = chip * spc, sf * spc, chip * spc / sym
>>> code = np.asarray(Gold().generate(sf)).astype(np.uint8)
>>> csign = np.where(code & 1, -1.0, 1.0)
>>> rng = np.random.default_rng(21)
>>> n = int(600 * tsym) + 4 * te            # 600 async BPSK symbols
>>> idx = np.arange(n)
>>> data = (rng.integers(0, 2, 604) * 2 - 1).astype(float)
>>> si = np.clip((idx / tsym).astype(int), 0, 603)
>>> t = idx / fs
>>> sig = (data[si] * csign[(idx // spc) % sf]
...        * np.exp(1j * 2 * np.pi * 0.5 * 500.0 * t * t))
>>> cn0 = 20.0 + 10 * np.log10(sym)         # Es/N0 = 20 dB
>>> sigma = np.sqrt(fs / 10 ** (cn0 / 10))
>>> pre = 5 * te                            # noise-only lead-in
>>> noise = (sigma / np.sqrt(2)) * (rng.standard_normal(pre + n)
...          + 1j * rng.standard_normal(pre + n))
>>> x = (np.concatenate([np.zeros(pre), sig]).astype(np.complex64)
...      + noise.astype(np.complex64))

The search is a separate object -- in a pool, one searcher per
channel serves every receiver on it. Its hit is a correlation lag and
a Doppler bin; the two documented helpers turn those into the seed:

>>> acq = Acquisition(code, spc=spc, chip_rate=chip, symbol_rate=sym,
...                   cn0_dbhz=cn0, doppler_uncertainty=500.0)
>>> for p in range(0, len(x) - te, te):
...     hits = acq.push(x[p:p + te])
...     if hits:
...         break
>>> d_bin, lag, _, _, _, cn0_est, consumed = hits[0]
>>> chip_phase = dll_init_chip_from_acq(lag, spc, sf)
>>> res_hz = acq.doppler_res_hz
>>> doppler_hz = bin_to_signed(d_bin, acq.doppler_bins) * res_hz

The receiver never searched: it waits idle, takes the seed, and the
samples from the hit onwards go to it.

>>> rx = HandoffAsyncDsssReceiver(
...     code, chip_rate=chip, symbol_rate=sym, spc=spc, cn0_dbhz=cn0)
>>> rx.idle
1
>>> rx.seed(chip_phase, doppler_hz, cn0_est)
>>> (rx.idle, rx.refining)
(0, 1)
>>> syms = [rx.steps(x[p:p + te])
...         for p in range(int(consumed), len(x) - te, te)]
>>> syms = np.concatenate([s for s in syms if len(s)])
>>> rx.tracking                  # refined and tracking, no search
1
>>> len(syms) > 300
True
>>> bool(np.mean(syms.real**2) > 10 * np.mean(syms.imag**2))
True

Assigned once: a second seed is refused until reset(), which in this
mode returns to idle, not to searching.

>>> rx.seed(0.0, 0.0, cn0)  # doctest: +ELLIPSIS
Traceback (most recent call last):
    ...
ValueError: seed refused: ...
>>> rx.reset()
>>> rx.idle
1
```
 




        

<hr>



### function async\_dsss\_receiver\_destroy 

_Destroy a receiver and release every child._ 
```C++
void async_dsss_receiver_destroy (
    async_dsss_receiver_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function async\_dsss\_receiver\_get\_car\_last\_error 

_Pre-despread Costas phase discriminator (rad): the residual carrier phase LOOP 1 (which de-rotates before the Dll) is not nulling._ 
```C++
double async_dsss_receiver_get_car_last_error (
    const async_dsss_receiver_state_t * state
) 
```




<hr>



### function async\_dsss\_receiver\_get\_car\_nco\_freq 

_LOOP 1 (pre-despread Costas) loop-filter output = NCO frequency command, cycles/sample of the front-end (chip\_rate\*spc) rate._ 
```C++
double async_dsss_receiver_get_car_nco_freq (
    const async_dsss_receiver_state_t * state
) 
```




<hr>



### function async\_dsss\_receiver\_get\_chip\_phase 

```C++
double async_dsss_receiver_get_chip_phase (
    const async_dsss_receiver_state_t * state
) 
```




<hr>



### function async\_dsss\_receiver\_get\_cn0\_dbhz\_est 

```C++
double async_dsss_receiver_get_cn0_dbhz_est (
    const async_dsss_receiver_state_t * state
) 
```




<hr>



### function async\_dsss\_receiver\_get\_code\_locked 

_Binary code-lock flag from the live tracking Dll's own verify-counted (pfa-tuned) lock detector — the fundamental DSSS "am I
despreading" lock, de-chattered by up/down hysteresis._ 
```C++
int async_dsss_receiver_get_code_locked (
    const async_dsss_receiver_state_t * state
) 
```




<hr>



### function async\_dsss\_receiver\_get\_code\_rate 

```C++
double async_dsss_receiver_get_code_rate (
    const async_dsss_receiver_state_t * state
) 
```




<hr>



### function async\_dsss\_receiver\_get\_doppler\_hz 

```C++
double async_dsss_receiver_get_doppler_hz (
    const async_dsss_receiver_state_t * state
) 
```




<hr>



### function async\_dsss\_receiver\_get\_idle 

_1 while waiting for a seed (hand-off mode, before seed() or after reset()); 0 in every other state._ 
```C++
int async_dsss_receiver_get_idle (
    const async_dsss_receiver_state_t * state
) 
```




<hr>



### function async\_dsss\_receiver\_get\_lock 

```C++
double async_dsss_receiver_get_lock (
    const async_dsss_receiver_state_t * state
) 
```




<hr>



### function async\_dsss\_receiver\_get\_lock\_metric 

_Symbol-lock metric = SNR-weighted EMA of (I^2-Q^2)/(I^2+Q^2) = cos(2\*phi) over the emitted symbols (locked -&gt; ~+1). Drives_ `locked` _._
```C++
double async_dsss_receiver_get_lock_metric (
    const async_dsss_receiver_state_t * state
) 
```




<hr>



### function async\_dsss\_receiver\_get\_lock\_threshold 

_The lock-metric declare threshold_ `locked` _latches above (the lockdet up\_thresh); exposed alongside lock\_metric for engineering debug._
```C++
double async_dsss_receiver_get_lock_threshold (
    const async_dsss_receiver_state_t * state
) 
```




<hr>



### function async\_dsss\_receiver\_get\_locked 

_Binary carrier-lock flag from the loop's hysteretic (up/down verify-counted) lock detector — the de-chattered lock indicator, unlike the raw_ `lock` _metric._
```C++
int async_dsss_receiver_get_locked (
    const async_dsss_receiver_state_t * state
) 
```




<hr>



### function async\_dsss\_receiver\_get\_lost 

_1 once the release rule has fired: both lock flags were down, continuously, for longer than_ `lost_confirm_s` _while tracking_ _an emitter that left, or a seed that never locked. The loops have stopped; the holder releases the assignment and calls reset(). 0 in every other state, and always 0 with_`lost_confirm_s = 0` _._
```C++
int async_dsss_receiver_get_lost (
    const async_dsss_receiver_state_t * state
) 
```




<hr>



### function async\_dsss\_receiver\_get\_mpsk\_last\_error 

_MpskReceiver carrier phase discriminator (rad): the residual carrier phase LOOP 2 (post-despread) is not nulling._ 
```C++
double async_dsss_receiver_get_mpsk_last_error (
    const async_dsss_receiver_state_t * state
) 
```




<hr>



### function async\_dsss\_receiver\_get\_n 

```C++
int async_dsss_receiver_get_n (
    const async_dsss_receiver_state_t * state
) 
```




<hr>



### function async\_dsss\_receiver\_get\_nco\_freq 

_Live carrier loop-filter output = NCO frequency command (cycles/sample of the MpskReceiver output rate). Its mean tracks a Doppler ramp with no lag (unlike get\_norm\_freq's integrator estimate); its variance is the carrier loop stress._ 
```C++
double async_dsss_receiver_get_nco_freq (
    const async_dsss_receiver_state_t * state
) 
```




<hr>



### function async\_dsss\_receiver\_get\_norm\_freq 

```C++
double async_dsss_receiver_get_norm_freq (
    const async_dsss_receiver_state_t * state
) 
```




<hr>



### function async\_dsss\_receiver\_get\_refining 

```C++
int async_dsss_receiver_get_refining (
    const async_dsss_receiver_state_t * state
) 
```




<hr>



### function async\_dsss\_receiver\_get\_segments 

```C++
size_t async_dsss_receiver_get_segments (
    const async_dsss_receiver_state_t * state
) 
```




<hr>



### function async\_dsss\_receiver\_get\_sps 

```C++
size_t async_dsss_receiver_get_sps (
    const async_dsss_receiver_state_t * state
) 
```




<hr>



### function async\_dsss\_receiver\_get\_state 

```C++
void async_dsss_receiver_get_state (
    const async_dsss_receiver_state_t * state,
    void * blob
) 
```




<hr>



### function async\_dsss\_receiver\_get\_tracking 

```C++
int async_dsss_receiver_get_tracking (
    const async_dsss_receiver_state_t * state
) 
```




<hr>



### function async\_dsss\_receiver\_reset 

_Return to the searching state_  _or, in hand-off mode, to idle. Resets the embedded Acquisition (if any) and rebuilds both the refine-stage and live-tracking chains back to their placeholder seed (phase 0, no Doppler). A receiver that has locked cannot be "reset back
to tracking the same signal," only back to searching — matching every other object's reset() semantics in this codebase. In hand-off mode there is no search to return to, so this is how the holder of a pool releases a lost receiver for its next seed, with no reallocation._
```C++
void async_dsss_receiver_reset (
    async_dsss_receiver_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 
```C++
>>> import numpy as np
>>> from doppler.dsss import AsyncDsssReceiver
>>> from doppler.wfm import Gold
>>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
>>> rx = AsyncDsssReceiver(code, chip_rate=3.069e6, symbol_rate=2700.0,
...                        spc=2, doppler_uncertainty=500.0)
>>> rx.reset()                 # abort any lock, hunt from scratch
>>> (rx.tracking, rx.refining, rx.chip_phase)   # all cleared
(0, 0, 0.0)
```
 




        

<hr>



### function async\_dsss\_receiver\_seed 

_Take a detection from outside and start refining from it._ 
```C++
int async_dsss_receiver_seed (
    async_dsss_receiver_state_t * state,
    double chip_phase,
    double doppler_hz_est,
    double cn0_dbhz_est
) 
```



The hand-off of docs/design/async-dsss-receiver.md section 11.1: the three numbers a searcher's hit carries that this receiver uses  `acq_handoff_t`'s `chip_phase`, `doppler_hz_est` and `cn0_dbhz_est`  exactly as its own hit would have produced them (the searching flavor's `steps()` calls this on its own hit). `chip_phase` is the code's instantaneous phase in chips, Dll's convention, at the FIRST sample of the next `steps()` call; the Python-side conversion from a lag is `doppler.dsss.handoff`. The refine chain is rebuilt from the seed and the state becomes refining; the unconsumed tail is the caller's to feed.


Refused (`DP_ERR_INVALID`, nothing changes) on a receiver that is not waiting for one  refining, tracking or lost  because "assigned once" is a property of the object, not of the caller's bookkeeping; `reset()` releases it. Accepted while idle (hand-off mode) or searching (the searching flavor: an outside hit simply beats its own). Also refused for a `chip_phase` outside `[0, code_len)` or a non-finite value.




**Parameters:**


* `state` Must be non-NULL. 
* `chip_phase` Code phase at the next sample, chips, in `[0, code_len)`. 
* `doppler_hz_est` Coarse Doppler estimate, Hz (the refine stage sharpens it). 
* `cn0_dbhz_est` The hit's C/N0 estimate, dB-Hz; reported back by get\_cn0\_dbhz\_est() until tracking refreshes it. 



**Returns:**

`DP_OK`, or `DP_ERR_INVALID` when refused. 
```C++
>>> import numpy as np
>>> from doppler.dsss import HandoffAsyncDsssReceiver
>>> from doppler.wfm import Gold
>>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
>>> rx = HandoffAsyncDsssReceiver(code, chip_rate=3.069e6,
...                               symbol_rate=2700.0, spc=2)
>>> rx.seed(chip_phase=512.25, doppler_hz_est=-1500.0,
...         cn0_dbhz_est=48.0)
>>> (rx.idle, rx.refining, rx.doppler_hz, rx.cn0_dbhz_est)
(0, 1, -1500.0, 48.0)

Already assigned -- refused until reset():

>>> rx.seed(0.0, 0.0, 48.0)      # doctest: +ELLIPSIS
Traceback (most recent call last):
    ...
ValueError: seed refused: ...
>>> rx.reset()

A chip phase must be inside the code, `[0, code_len)`:

>>> rx.seed(1023.0, 0.0, 48.0)   # doctest: +ELLIPSIS
Traceback (most recent call last):
    ...
ValueError: seed refused: ...
```
 





        

<hr>



### function async\_dsss\_receiver\_set\_state 

```C++
int async_dsss_receiver_set_state (
    async_dsss_receiver_state_t * state,
    const void * blob
) 
```




<hr>



### function async\_dsss\_receiver\_state\_bytes 

```C++
size_t async_dsss_receiver_state_bytes (
    const async_dsss_receiver_state_t * state
) 
```




<hr>



### function async\_dsss\_receiver\_steps 

_Stream raw cf32 samples; emit demodulated symbols once tracking._ 
```C++
size_t async_dsss_receiver_steps (
    async_dsss_receiver_state_t * state,
    const float complex * x,
    size_t x_len,
    float complex * out,
    size_t max_out
) 
```



Drives the search -&gt; refine -&gt; track state machine. While searching or refining, nothing is emitted (an empty return is normal, not an error): a hit seeds the frozen-carrier refine chain, `CarrierAcquisition` sharpens the coarse Doppler estimate, and only once it is ready (or gives up) is the live tracking chain built and demodulation begins. Accepts any block size; state carries across calls, so a capture can be fed in frames of any length with no seam. Idle (hand-off mode, before a seed) and lost (after the release rule fires) consume the samples and emit nothing, so the feeding loop is the same in every state; while tracking, the release clock runs on the two lock flags after every call (see `lost_confirm_s`). Under SPEC's coupled offset + 500 Hz/s Doppler ramp the pre-despread Costas removes the full carrier dynamics before the code loop, so the recovered constellation lands cleanly on the BPSK real axis.




**Parameters:**


* `state` Must be non-NULL. 
* `x` Input cf32 samples. 
* `x_len` Number of input samples. 
* `out` Output symbols; caller provides `max_out` capacity. 
* `max_out` Output capacity. 



**Returns:**

Number of symbols written (0 while searching/refining, or while tracking with not yet a full symbol's worth of input). 
```C++
>>> import numpy as np
>>> from doppler.dsss import AsyncDsssReceiver
>>> from doppler.wfm import Gold
>>> sf, chip, sym, spc = 1023, 3.069e6, 2700.0, 2
>>> fs, te, tsym = chip * spc, sf * spc, chip * spc / sym
>>> code = np.asarray(Gold().generate(sf)).astype(np.uint8)
>>> csign = np.where(code & 1, -1.0, 1.0)
>>> rng = np.random.default_rng(21)
>>> n = int(600 * tsym) + 4 * te            # 600 async BPSK symbols
>>> idx = np.arange(n)
>>> data = (rng.integers(0, 2, 604) * 2 - 1).astype(float)
>>> si = np.clip((idx / tsym).astype(int), 0, 603)
>>> t = idx / fs

DSSS chips on a carrier sweeping at 500 Hz/s — the ramp the async
receiver has to track:

>>> sig = (data[si] * csign[(idx // spc) % sf]
...        * np.exp(1j * 2 * np.pi * 0.5 * 500.0 * t * t))
>>> cn0 = 20.0 + 10 * np.log10(sym)         # Es/N0 = 20 dB
>>> sigma = np.sqrt(fs / 10 ** (cn0 / 10))
>>> pre = 5 * te                            # noise-only lead-in
>>> noise = (sigma / np.sqrt(2)) * (rng.standard_normal(pre + n)
...          + 1j * rng.standard_normal(pre + n))
>>> x = (np.concatenate([np.zeros(pre), sig]).astype(np.complex64)
...      + noise.astype(np.complex64))
>>> rx = AsyncDsssReceiver(
...     code, chip_rate=chip, symbol_rate=sym, spc=spc,
...     cn0_dbhz=cn0, doppler_uncertainty=500.0)
>>> syms = [rx.steps(x[p:p + te]) for p in range(0, len(x) - te, te)]
>>> syms = np.concatenate([s for s in syms if len(s)])
>>> rx.tracking                  # searched, refined, now tracking
1
>>> len(syms) > 300              # symbols recovered under the ramp
True

Nearly all the energy lands on I, so the BPSK phase is resolved:

>>> bool(np.mean(syms.real**2) > 10 * np.mean(syms.imag**2))
True
```
 





        

<hr>



### function async\_dsss\_receiver\_steps\_max\_out 

```C++
size_t async_dsss_receiver_steps_max_out (
    async_dsss_receiver_state_t * state
) 
```




<hr>
## Macro Definition Documentation





### define ASYNC\_DSSS\_RECEIVER\_STATE\_MAGIC 

```C++
#define ASYNC_DSSS_RECEIVER_STATE_MAGIC `DP_FOURCC ('A', 'D', 'R', 'X')`
```




<hr>



### define ASYNC\_DSSS\_RECEIVER\_STATE\_VERSION 

```C++
#define ASYNC_DSSS_RECEIVER_STATE_VERSION `3u`
```




<hr>



### define ASYNC\_DSSS\_RX\_BN\_CARRIER 

```C++
#define ASYNC_DSSS_RX_BN_CARRIER `0.04`
```




<hr>



### define ASYNC\_DSSS\_RX\_DLL\_BN 

```C++
#define ASYNC_DSSS_RX_DLL_BN `0.002`
```




<hr>



### define ASYNC\_DSSS\_RX\_IDLE 

```C++
#define ASYNC_DSSS_RX_IDLE `3`
```




<hr>



### define ASYNC\_DSSS\_RX\_LOCK\_DOWN 

```C++
#define ASYNC_DSSS_RX_LOCK_DOWN `0.3`
```




<hr>



### define ASYNC\_DSSS\_RX\_LOCK\_DWELL 

```C++
#define ASYNC_DSSS_RX_LOCK_DWELL `30u`
```




<hr>



### define ASYNC\_DSSS\_RX\_LOCK\_N\_DOWN 

```C++
#define ASYNC_DSSS_RX_LOCK_N_DOWN `15u`
```




<hr>



### define ASYNC\_DSSS\_RX\_LOCK\_N\_UP 

```C++
#define ASYNC_DSSS_RX_LOCK_N_UP `30u`
```




<hr>



### define ASYNC\_DSSS\_RX\_LOCK\_UP 

```C++
#define ASYNC_DSSS_RX_LOCK_UP `0.5`
```




<hr>



### define ASYNC\_DSSS\_RX\_LOST 

```C++
#define ASYNC_DSSS_RX_LOST `4`
```




<hr>



### define ASYNC\_DSSS\_RX\_REFINING 

```C++
#define ASYNC_DSSS_RX_REFINING `1`
```




<hr>



### define ASYNC\_DSSS\_RX\_SEARCHING 

```C++
#define ASYNC_DSSS_RX_SEARCHING `0`
```




<hr>



### define ASYNC\_DSSS\_RX\_TRACKING 

```C++
#define ASYNC_DSSS_RX_TRACKING `2`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/async_dsss_receiver/async_dsss_receiver_core.h`

