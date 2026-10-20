

# File async\_dsss\_receiver\_core.h



[**FileList**](files.md) **>** [**async\_dsss\_receiver**](dir_565c73e0c8995e904663dd9fb6485ceb.md) **>** [**async\_dsss\_receiver\_core.h**](async__dsss__receiver__core_8h.md)

[Go to the source code of this file](async__dsss__receiver__core_8h_source.md)

_Composed continuous DSSS receiver: Acquisition -&gt; hand-off -&gt; CarrierAcquisition refine -&gt; Costas/Dll/RateConverter/ MpskReceiver track, one object._ [More...](#detailed-description)

* `#include "doppler/RateConverter/RateConverter_core.h"`
* `#include "doppler/acq/acq_core.h"`
* `#include "doppler/carrier_acq/carrier_acq_core.h"`
* `#include "doppler/cic/cic_core.h"`
* `#include "doppler/costas/costas_core.h"`
* `#include "doppler/dll/dll_core.h"`
* `#include "doppler/dp_state.h"`
* `#include "doppler/hbdecim/hbdecim_core.h"`
* `#include "doppler/lockdet/lockdet_core.h"`
* `#include "doppler/mpsk_receiver/mpsk_receiver_core.h"`
* `#include "doppler/resamp/resamp_core.h"`
* `#include "doppler/resample/resample_core.h"`
* `#include "doppler/dp_complex.h"`
* `#include <stdbool.h>`
* `#include <stddef.h>`
* `#include "doppler/psd/psd_core.h"`
* `#include "doppler/detector/detector_core.h"`
* `#include "doppler/detection/detection_core.h"`
* `#include "doppler/spectral/spectral_core.h"`
* `#include "doppler/corr/corr_core.h"`
* `#include "doppler/fft/fft_core.h"`
* `#include "doppler/acc_trace/acc_trace_core.h"`
* `#include "doppler/ber/ber_core.h"`
* `#include "doppler/doppler_channel/doppler_channel_core.h"`
* `#include "doppler/dp_tlm/dp_tlm_core.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**async\_dsss\_receiver\_extra\_t**](structasync__dsss__receiver__extra__t.md) <br> |
| struct | [**async\_dsss\_receiver\_status\_t**](structasync__dsss__receiver__status__t.md) <br>_One consistent picture of what the receiver is doing, by value._  |
| struct | [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) <br>_Composed receiver state._  |


## Public Types

| Type | Name |
| ---: | :--- |
| enum  | [**async\_dsss\_rx\_state\_t**](#enum-async_dsss_rx_state_t)  <br> |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  int | [**dp\_async\_dsss\_receiver\_configure\_chain\_raw**](#function-dp_async_dsss_receiver_configure_chain_raw) ([**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state, size\_t segments, size\_t sps, int n) <br>_Pin the live-tracking despread/resample/demod grid directly. The escape hatch for the composition-specific knob:_ `segments` _(the live Dll's tracking parameter) and_`sps` _/_`n` _(MpskReceiver's samples and terminal outputs per symbol) are independently overridable, still bridged by a freshly-sized_`RateConverter` _and never coupled to each other. While searching/refining it re-pins the grid used to build the next tracking chain; once tracking it rebuilds_`dll` _/_`rc` _/_`rx` _in place, allocating every replacement before adopting it, and re-seeds the rebuilt chain from the whole carrier_ _loop 1 plus loop 2's integrator, the sum status() reports_ _so a retune keeps the signal it was tracking._ |
|  void | [**dp\_async\_dsss\_receiver\_configure\_lock\_raw**](#function-dp_async_dsss_receiver_configure_lock_raw) ([**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state, double up\_thresh, double down\_thresh, size\_t n\_looks, double alpha, uint32\_t n\_up, uint32\_t n\_down) <br>_Re-tune the live-tracking Dll's code-lock detector directly. Forwards to_ `dp_dll_configure_lock_raw()` _on the live tracking Dll (the one behind_`get_code_locked()` _), NOT the refine-stage collection Dll. It retunes the Dll the receiver holds now; every chain build (a seed, the hand-over, reset() or configure\_chain\_raw()) replaces that Dll with a freshly sized one, so call it once tracking. The detector is the hysteretic lockdet over the DLL's per-N-look CFAR statistic — the levels and verify counts trade declare latency against false-alarm rate._ |
|  int | [**dp\_async\_dsss\_receiver\_configure\_search\_raw**](#function-dp_async_dsss_receiver_configure_search_raw) ([**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state, size\_t doppler\_bins, size\_t n\_noncoh) <br>_Pin the embedded Acquisition's search grid directly. Forwards to_ `dp_acq_configure_search_raw()` __ _the escape hatch under this object's_`symbol_rate` _-driven auto-sizing, for a power user who wants a specific_`(doppler_bins, n_noncoh)` _. The pin replaces the whole auto-sized grid, the window-tiled search included. Only meaningful while searching; the search does not run again until the next_`reset()` _._ |
|  [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* | [**dp\_async\_dsss\_receiver\_create**](#function-dp_async_dsss_receiver_create) (const uint8\_t \* code, size\_t code\_len, double chip\_rate, double symbol\_rate, size\_t spc, int m, double cn0\_dbhz, double pfa, double pd, double doppler\_uncertainty, size\_t segments, size\_t sps, int differential, double refine\_max\_error\_db, size\_t refine\_samples\_per\_symbol, double refine\_design\_margin\_db, size\_t refine\_n\_fft, size\_t refine\_zero\_pad, bool refine\_sequential, size\_t refine\_max\_n\_blocks, double carrier\_freq\_hz, double lost\_confirm\_s, bool reacquire, double code\_only\_s, double doppler\_rate) <br>_Create an AsyncDsssReceiver in the searching state._  |
|  void | [**dp\_async\_dsss\_receiver\_destroy**](#function-dp_async_dsss_receiver_destroy) ([**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state) <br>_Destroy a receiver and release every child._  |
|  double | [**dp\_async\_dsss\_receiver\_get\_car\_last\_error**](#function-dp_async_dsss_receiver_get_car_last_error) (const [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state) <br>_Pre-despread Costas phase discriminator (rad): the residual carrier phase LOOP 1 (which de-rotates before the Dll) is not nulling._  |
|  double | [**dp\_async\_dsss\_receiver\_get\_car\_nco\_freq**](#function-dp_async_dsss_receiver_get_car_nco_freq) (const [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state) <br>_LOOP 1 (pre-despread Costas) loop-filter output = NCO frequency command, cycles/sample of the front-end (chip\_rate\*spc) rate._  |
|  double | [**dp\_async\_dsss\_receiver\_get\_chip\_phase**](#function-dp_async_dsss_receiver_get_chip_phase) (const [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state) <br>_The live Dll's code phase, chips, in the Dll's instantaneous convention_  _the mirror of_[_**acq\_result\_t::code\_phase**_](structacq__result__t.md#variable-code_phase) _'s correlation-lag convention (_[_**dp\_acq\_build\_handoff()**_](acq__core_8h.md#function-dp_acq_build_handoff) _)._ |
|  double | [**dp\_async\_dsss\_receiver\_get\_cn0\_dbhz\_est**](#function-dp_async_dsss_receiver_get_cn0_dbhz_est) (const [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state) <br>_The seed's C/N0 estimate, dB-Hz, unchanged until reset()._  |
|  int | [**dp\_async\_dsss\_receiver\_get\_code\_locked**](#function-dp_async_dsss_receiver_get_code_locked) (const [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state) <br>_Binary code-lock flag from the live tracking Dll's own verify-counted (pfa-tuned) lock detector — the fundamental DSSS "am I
despreading" lock, de-chattered by up/down hysteresis._  |
|  double | [**dp\_async\_dsss\_receiver\_get\_code\_rate**](#function-dp_async_dsss_receiver_get_code_rate) (const [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state) <br>_The Dll's code rate, chips per nominal chip (~1.0 at lock), without the carrier aid; a coasting Dll reports its held rate._  |
|  size\_t | [**dp\_async\_dsss\_receiver\_get\_coherent\_bins**](#function-dp_async_dsss_receiver_get_coherent_bins) (const [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state) <br>_The searcher's slow-time depth D: epochs per coherent block._  |
|  double | [**dp\_async\_dsss\_receiver\_get\_doppler\_hz**](#function-dp_async_dsss_receiver_get_doppler_hz) (const [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state) <br>_The Doppler estimate the chain was built from, Hz: the seed's while refining, the CarrierAcquisition-refined value once tracking. The live carrier is status().doppler\_hz._  |
|  double | [**dp\_async\_dsss\_receiver\_get\_lock**](#function-dp_async_dsss_receiver_get_lock) (const [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state) <br>_The carrier lock statistic of the MpskReceiver this receiver tracks with: the EMA of its M-th-power lock signal, near 1 when locked and near 0 on noise._  |
|  double | [**dp\_async\_dsss\_receiver\_get\_lock\_metric**](#function-dp_async_dsss_receiver_get_lock_metric) (const [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state) <br>_Symbol-lock metric = SNR-weighted EMA of (I^2-Q^2)/(I^2+Q^2) = cos(2\*phi) over the emitted symbols (locked -&gt; ~+1). Drives_ `locked` _._ |
|  double | [**dp\_async\_dsss\_receiver\_get\_lock\_threshold**](#function-dp_async_dsss_receiver_get_lock_threshold) (const [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state) <br>_The lock-metric threshold_ `locked` _is declared strictly above (0.5, ASYNC\_DSSS\_RX\_LOCK\_UP); exposed alongside lock\_metric for engineering debug._ |
|  int | [**dp\_async\_dsss\_receiver\_get\_locked**](#function-dp_async_dsss_receiver_get_locked) (const [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state) <br>_The receiver's symbol-lock flag: a hysteretic detector over get\_lock\_metric(), declared after 30 consecutive symbols strictly above get\_lock\_threshold() (0.5) and dropped after 15 consecutive symbols strictly below 0.3 (ASYNC\_DSSS\_RX\_LOCK\_N\_UP, \_UP, \_N\_DOWN and \_DOWN, fixed at create). The flag the release rule and a pool's holder act on._  |
|  int | [**dp\_async\_dsss\_receiver\_get\_lost**](#function-dp_async_dsss_receiver_get_lost) (const [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state) <br>_1 once the release rule has fired: both lock flags were down, continuously, for longer than_ `lost_confirm_s` _while tracking_ _an emitter that left, or a seed that never locked. The loops have stopped and samples are consumed and discarded; the holder releases the assignment and calls reset(). 0 in every other state, and always 0 with_`lost_confirm_s = 0` _._ |
|  double | [**dp\_async\_dsss\_receiver\_get\_mpsk\_last\_error**](#function-dp_async_dsss_receiver_get_mpsk_last_error) (const [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state) <br>_MpskReceiver carrier phase discriminator (rad): the residual carrier phase LOOP 2 (post-despread) is not nulling._  |
|  int | [**dp\_async\_dsss\_receiver\_get\_n**](#function-dp_async_dsss_receiver_get_n) (const [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state) <br> |
|  double | [**dp\_async\_dsss\_receiver\_get\_nco\_freq**](#function-dp_async_dsss_receiver_get_nco_freq) (const [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state) <br>_The MpskReceiver's carrier NCO command, its loop filter's whole output, cycles/sample at its input rate (_ `sps * symbol_rate` _). Its mean tracks a Doppler ramp with no lag (unlike get\_norm\_freq()); its variance is the carrier loop's stress._ |
|  double | [**dp\_async\_dsss\_receiver\_get\_norm\_freq**](#function-dp_async_dsss_receiver_get_norm_freq) (const [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state) <br>_The post-despread carrier the MpskReceiver tracks: its centre plus its loop's integrator, cycles/sample at its input rate (_ `sps * symbol_rate` _). Lags a Doppler ramp by the Type-II ramp error._ |
|  int | [**dp\_async\_dsss\_receiver\_get\_refining**](#function-dp_async_dsss_receiver_get_refining) (const [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state) <br>_1 while refining: the CarrierAcquisition dwell; 0 in every other state._  |
|  size\_t | [**dp\_async\_dsss\_receiver\_get\_segments**](#function-dp_async_dsss_receiver_get_segments) (const [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state) <br> |
|  size\_t | [**dp\_async\_dsss\_receiver\_get\_sps**](#function-dp_async_dsss_receiver_get_sps) (const [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state) <br> |
|  void | [**dp\_async\_dsss\_receiver\_get\_state**](#function-dp_async_dsss_receiver_get_state) (const [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state, void \* blob) <br> |
|  int | [**dp\_async\_dsss\_receiver\_get\_threads**](#function-dp_async_dsss_receiver_get_threads) (const [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state) <br>_Workers the searcher runs, the calling thread included; 1 = serial. Set with_ [_**dp\_async\_dsss\_receiver\_set\_threads()**_](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_set_threads) _._ |
|  int | [**dp\_async\_dsss\_receiver\_get\_tracking**](#function-dp_async_dsss_receiver_get_tracking) (const [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state) <br>_1 once the live tracking chain is built and demodulating; 0 in every other state._  |
|  void | [**dp\_async\_dsss\_receiver\_reset**](#function-dp_async_dsss_receiver_reset) ([**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state) <br>_Return to the searching state. Resets the embedded Acquisition and rebuilds the refine-stage chain and the live tracking chain back to their placeholder seed (phase 0, no Doppler). A receiver that has locked cannot be "reset back to tracking the same signal," only back to searching_  _every object's reset() semantics in this codebase._ |
|  int | [**dp\_async\_dsss\_receiver\_seed**](#function-dp_async_dsss_receiver_seed) ([**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state, double chip\_phase, double doppler\_hz\_est, double cn0\_dbhz\_est) <br>_Take a detection from outside and start refining from it._  |
|  int | [**dp\_async\_dsss\_receiver\_seed\_ok**](#function-dp_async_dsss_receiver_seed_ok) (const [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state, double chip\_phase, double doppler\_hz\_est) <br>_The seed domain: whether_ `(chip_phase, doppler_hz_est)` _is one_[_**dp\_async\_dsss\_receiver\_seed()**_](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_seed) _accepts, and one a live receiver can hold._ |
|  int | [**dp\_async\_dsss\_receiver\_set\_refine\_min\_blocks**](#function-dp_async_dsss_receiver_set_refine_min_blocks) ([**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state, size\_t n\_blocks) <br>_Floor the refine's dwell at_ `n_blocks` _, whatever the detection sizing asks (docs/design/async-dsss-receiver-measurements.md §12.16, #1265)._ |
|  int | [**dp\_async\_dsss\_receiver\_set\_state**](#function-dp_async_dsss_receiver_set_state) ([**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state, const void \* blob) <br> |
|  int | [**dp\_async\_dsss\_receiver\_set\_telemetry**](#function-dp_async_dsss_receiver_set_telemetry) ([**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state, [**dp\_tlm\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_t) \* tlm, const char \* prefix, uint32\_t decim) <br>_Attach (or detach) a telemetry context across the receiver's loops, and register the receiver's own lifecycle and symbol-lock decision._  |
|  int | [**dp\_async\_dsss\_receiver\_set\_threads**](#function-dp_async_dsss_receiver_set_threads) ([**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state, int n) <br>_Set the workers the searcher fans its Doppler tiles across._  |
|  size\_t | [**dp\_async\_dsss\_receiver\_state\_bytes**](#function-dp_async_dsss_receiver_state_bytes) (const [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state) <br> |
|  [**async\_dsss\_receiver\_status\_t**](structasync__dsss__receiver__status__t.md) | [**dp\_async\_dsss\_receiver\_status**](#function-dp_async_dsss_receiver_status) (const [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state) <br>_Read the status record, by value: the state; where the emitter is now (the live carrier), its chip phase, code rate and C/N0; both lock flags with the symbol-lock metric and threshold; both carrier residuals; and the two clocks in input samples (since the state was entered; both flags down without a break)._  |
|  size\_t | [**dp\_async\_dsss\_receiver\_steps**](#function-dp_async_dsss_receiver_steps) ([**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state, const float \_Complex \* x, size\_t x\_len, float \_Complex \* out, size\_t max\_out) <br>_Stream raw cf32 samples; emit demodulated symbols once tracking._  |
|  size\_t | [**dp\_async\_dsss\_receiver\_steps\_max\_out**](#function-dp_async_dsss_receiver_steps_max_out) ([**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state) <br>_The output capacity steps() needs beyond the input's._  |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**ASYNC\_DSSS\_RECEIVER\_STATE\_MAGIC**](async__dsss__receiver__core_8h.md#define-async_dsss_receiver_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('A', 'D', 'R', 'X')`<br> |
| define  | [**ASYNC\_DSSS\_RECEIVER\_STATE\_VERSION**](async__dsss__receiver__core_8h.md#define-async_dsss_receiver_state_version)  `12u /\* v12: samples\_in, the dwell alignment, threads; v11: no cell mode; v10: the dll children grew (output level, #2495); v9: refine\_min\_blocks; v8: refused\_hits; v7: no hand-off flavor; v6: the cell pull-in \*/`<br> |
| define  | [**ASYNC\_DSSS\_RX\_BN\_CARRIER**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_bn_carrier)  `0.04`<br> |
| define  | [**ASYNC\_DSSS\_RX\_CARRIER\_PULLIN\_HZ**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_carrier_pullin_hz) (chip\_rate, code\_len) `([**ASYNC\_DSSS\_RX\_BN\_CARRIER**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_bn_carrier) \* (chip\_rate) / (2.0 \* (double)(code\_len)))`<br> |
| define  | [**ASYNC\_DSSS\_RX\_DEMOD\_BN\_CARRIER**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_demod_bn_carrier)  `0.01`<br> |
| define  | [**ASYNC\_DSSS\_RX\_DEMOD\_BN\_TIMING**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_demod_bn_timing)  `0.005`<br> |
| define  | [**ASYNC\_DSSS\_RX\_DEMOD\_LOCK\_THRESH**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_demod_lock_thresh)  `0.3`<br> |
| define  | [**ASYNC\_DSSS\_RX\_DEMOD\_RRC\_BETA**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_demod_rrc_beta)  `0.35`<br> |
| define  | [**ASYNC\_DSSS\_RX\_DEMOD\_RRC\_SPAN**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_demod_rrc_span)  `8`<br> |
| define  | [**ASYNC\_DSSS\_RX\_DEMOD\_ZETA**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_demod_zeta)  `0.707`<br> |
| define  | [**ASYNC\_DSSS\_RX\_DLL\_BN**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_dll_bn)  `0.002`<br> |
| define  | [**ASYNC\_DSSS\_RX\_DLL\_SPACING**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_dll_spacing)  `0.5`<br> |
| define  | [**ASYNC\_DSSS\_RX\_DLL\_ZETA**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_dll_zeta)  `0.707`<br> |
| define  | [**ASYNC\_DSSS\_RX\_LOCK\_DOWN**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_lock_down)  `0.3`<br> |
| define  | [**ASYNC\_DSSS\_RX\_LOCK\_DWELL**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_lock_dwell)  `30u`<br> |
| define  | [**ASYNC\_DSSS\_RX\_LOCK\_N\_DOWN**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_lock_n_down)  `15u`<br> |
| define  | [**ASYNC\_DSSS\_RX\_LOCK\_N\_UP**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_lock_n_up)  `30u`<br> |
| define  | [**ASYNC\_DSSS\_RX\_LOCK\_UP**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_lock_up)  `0.5`<br> |
| define  | [**ASYNC\_DSSS\_RX\_REFINE\_DLL\_SPACING**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_refine_dll_spacing)  `0.5`<br> |
| define  | [**ASYNC\_DSSS\_RX\_REFINE\_MIN\_BLOCKS**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_refine_min_blocks)  `7u`<br> |

## Detailed Description


Unlike `DsssReceiver`, which goes straight from an acquisition hit to tracking on the hit's coarse Doppler estimate, this object refines the estimate between the two, closing the low-Es/N0 pull-in gap a coarse-only estimate leaves at large static Doppler offsets. Its design of record is docs/design/async-dsss-parallel.md: N of these receivers, always on, each searching and tracking by itself under a controller that only watches. The despreader, the states and the release under it are docs/design/async-dsss-receiver.md. Its states are the `ASYNC_DSSS_RX_*` values below, also `status().state`:



* **searching** (the start): samples feed the embedded `Acquisition`. A hit is made into a seed by `dp_acq_build_handoff()` and taken through `dp_async_dsss_receiver_seed()`, and the unconsumed tail of the same call goes straight on. The refine chain is a FROZEN carrier wipe (`costas_wipeoff()` at the coarse estimate, `costas_update()` never called) feeding a collection `Dll` whose `dp_dll_lookback_segments(refine_max_error_db)` windows oversample each epoch  the asynchronous data's residual carrier rides a ~symbol\_rate-wide spectrum a single per-epoch dump would alias (see `refine_max_error_db` on `dp_async_dsss_receiver_create()`)  then a `RateConverter` to `CarrierAcquisition`'s rate, then `CarrierAcquisition` itself.
* **refining**: samples feed the refine chain until `CarrierAcquisition` is ready or gives up; then the live tracking chain is built FRESH, from the hand-off's chip phase advanced by the clock dilation over the refine (not wherever the refine-stage `Dll` drifted) and the refined  on a give-up, the seed's  Doppler.
* **tracking**: the pre-despread carrier loop (`costas_wipeoff`/ `costas_update`) -&gt; `Dll` -&gt; `RateConverter` -&gt; `MpskReceiver`. The carrier loop updates once per code period from a NON-DATA-AIDED (squaring) discriminator over that period's coherent-I&D partials (`adr_track_period()`): a code period spans ~0.9 data symbols at SPEC's ratio, so a transition lands inside nearly every period, and squaring is what makes the error transition-robust. At `ASYNC_DSSS_RX_BN_CARRIER` the loop pulls the refined seed in and removes the full coupled Doppler (offset and 500 Hz/s ramp) before despreading, so `MpskReceiver` is left a small residual. A pure PLL: no FLL.
* **lost**: the emitter is gone. Without `reacquire`, entered from tracking when BOTH lock flags have been down, without a break, for longer than `lost_confirm_s` (docs/design/async-dsss-receiver.md §10, §11.2); the loops stop and samples are discarded until `reset()`. `lost_confirm_s = 0` (the default) never enters it. With `reacquire` the release is symbol lock alone and returns the receiver to searching instead, so it never reads lost (docs/design/async-dsss-parallel.md §3). While the confirm interval runs the loops HOLD what they settled on with both flags up rather than run on noise ([**dp\_dll\_set\_coast()**](dll__core_8h.md#function-dp_dll_set_coast); docs/design/async-dsss-receiver.md §10; docs/design/async-dsss-receiver-measurements.md §12.19, doppler#1271), so a departed emitter's receiver stands where the emitter left it, and neither sweeps onto a neighbour's code nor restarts its release clock on one it followed. One flag down is a degrade: reported, and the loops run, which is what rides out two live emitters crossing each other's code phase.




`dp_async_dsss_receiver_seed()` hands a searching receiver a detection from outside: the record its own hit would have produced (a hit is a seed the object makes for itself). It refuses on a receiver that already holds one, so a hold is never silently overwritten.


The refine and track stages share ONE carrier-wipe scratch/carry buffer set (`car_wiped_buf`/`car_carry_buf`/`car_carry_len`, sized `tsamps = code_len*spc`); they never run concurrently. The carry holds 0..tsamps-1 live samples (a full period is processed at once, and set\_state refuses tsamps); the blob packs its fixed tsamps region last, the live samples then zeros.



```C++
dp_async_dsss_receiver_state_t *rx = dp_async_dsss_receiver_create(
    code, code_len, 3.0e6, 2100.0,   // chip_rate, symbol_rate
    2, 2,                            // spc, m (BPSK)
    55.0, 1e-3, 0.9, 100.0,          // cn0_dbhz, pfa, pd,
                                     // doppler_uncertainty
    4, 8, 0,                         // segments, sps, differential
    0.5, 4, 14.0, 64, 8, false, 100000,  // refine_* tuning
    0.0,                             // carrier_freq_hz (0 = aiding off)
    0.0,                             // lost_confirm_s (0 = never lost)
    false);                          // reacquire (off: lost waits)
float _Complex syms[4096];
size_t n = dp_async_dsss_receiver_steps(rx, x, x_len, syms, 4096);
dp_async_dsss_receiver_destroy(rx);
```
 


    
## Public Types Documentation




### enum async\_dsss\_rx\_state\_t 

```C++
enum async_dsss_rx_state_t {
    ASYNC_DSSS_RX_SEARCHING = 0,
    ASYNC_DSSS_RX_REFINING = 1,
    ASYNC_DSSS_RX_TRACKING = 2,
    ASYNC_DSSS_RX_RESERVED = 3,
    ASYNC_DSSS_RX_LOST = 4
};
```




<hr>
## Public Functions Documentation




### function dp\_async\_dsss\_receiver\_configure\_chain\_raw 

_Pin the live-tracking despread/resample/demod grid directly. The escape hatch for the composition-specific knob:_ `segments` _(the live Dll's tracking parameter) and_`sps` _/_`n` _(MpskReceiver's samples and terminal outputs per symbol) are independently overridable, still bridged by a freshly-sized_`RateConverter` _and never coupled to each other. While searching/refining it re-pins the grid used to build the next tracking chain; once tracking it rebuilds_`dll` _/_`rc` _/_`rx` _in place, allocating every replacement before adopting it, and re-seeds the rebuilt chain from the whole carrier_ _loop 1 plus loop 2's integrator, the sum status() reports_ _so a retune keeps the signal it was tracking._
```C++
int dp_async_dsss_receiver_configure_chain_raw (
    dp_async_dsss_receiver_state_t * state,
    size_t segments,
    size_t sps,
    int n
) 
```



The new grid is checked as create() checked the first, before anything is rebuilt, and refused with the receiver left on its prior grid: the Dll's domain at `segments`, `sps` &gt;= 2, a symbol period of at most 2^20 Dll partials, which the Dll's aid would otherwise refuse mid-build (doppler#2103), and an `n` MpskReceiver would refuse (#2421). Still an abort: an allocation failure in the rebuild (doppler#2112).




**Parameters:**


* `state` Must be non-NULL. 
* `segments` Live-tracking Dll segments per code period. 
* `sps` MpskReceiver samples per symbol (the resample target), &gt;= 2. 
* `n` MpskReceiver's terminal outputs per symbol (`m_out`): inside [**dp\_ratesync\_m\_ok()**](ratesync__core_8h.md#function-dp_ratesync_m_ok) (even, 2..8), and dividing `sps`. 



**Returns:**

0 on success, -1 on a grid refused as above. 
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



### function dp\_async\_dsss\_receiver\_configure\_lock\_raw 

_Re-tune the live-tracking Dll's code-lock detector directly. Forwards to_ `dp_dll_configure_lock_raw()` _on the live tracking Dll (the one behind_`get_code_locked()` _), NOT the refine-stage collection Dll. It retunes the Dll the receiver holds now; every chain build (a seed, the hand-over, reset() or configure\_chain\_raw()) replaces that Dll with a freshly sized one, so call it once tracking. The detector is the hysteretic lockdet over the DLL's per-N-look CFAR statistic — the levels and verify counts trade declare latency against false-alarm rate._
```C++
void dp_async_dsss_receiver_configure_lock_raw (
    dp_async_dsss_receiver_state_t * state,
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
>>> rx.tracking                       # replaced at the next chain build
0
```
 




        

<hr>



### function dp\_async\_dsss\_receiver\_configure\_search\_raw 

_Pin the embedded Acquisition's search grid directly. Forwards to_ `dp_acq_configure_search_raw()` __ _the escape hatch under this object's_`symbol_rate` _-driven auto-sizing, for a power user who wants a specific_`(doppler_bins, n_noncoh)` _. The pin replaces the whole auto-sized grid, the window-tiled search included. Only meaningful while searching; the search does not run again until the next_`reset()` _._
```C++
int dp_async_dsss_receiver_configure_search_raw (
    dp_async_dsss_receiver_state_t * state,
    size_t doppler_bins,
    size_t n_noncoh
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `doppler_bins` The coherent depth to pin, in `[1, reps]`: 1, as the receiver's continuous engine has `reps = 1`. 
* `n_noncoh` Non-coherent looks accumulated per grid cell (1..256); more looks buys sensitivity at the cost of dwell, replacing the auto-sized count. 



**Returns:**

0 on success, -1 on invalid grid (see dp\_acq\_configure\_search\_raw). 
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



### function dp\_async\_dsss\_receiver\_create 

_Create an AsyncDsssReceiver in the searching state._ 
```C++
dp_async_dsss_receiver_state_t * dp_async_dsss_receiver_create (
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
    double lost_confirm_s,
    bool reacquire,
    double code_only_s,
    double doppler_rate
) 
```



Only `code`/`chip_rate`/`symbol_rate` describe the signal itself. The `refine_*` defaults are sized to close SPEC's 4-5 dB, 500 Hz/s combined scenario as they are; a power user can override each one.




**Parameters:**


* `code` Spreading code, one 0/1 chip per element (0 -&gt; +1, 1 -&gt; -1 BPSK; only the low bit is used, so pass 0/1, not +/-1). 
* `code_len` Chips in `code`. 
* `chip_rate` Chip rate, Hz. 
* `symbol_rate` Data-symbol rate, Hz: finite, &gt; 0, at most `chip_rate`, and at most 2^20 Dll partials per symbol at `segments`. 
* `spc` Samples/chip; default 2. 
* `m` PSK order, 2/4/8; default 2 (BPSK). 
* `cn0_dbhz` Design C/N0, dB-Hz; default 55.0  feeds BOTH the embedded Acquisition's own sizing AND (derated by `refine_design_margin_db`) CarrierAcquisition's `design_snr`. 
* `pfa` Acquisition false-alarm target; default 1e-3. Also CarrierAcquisition's own `pfa`. 
* `pd` Acquisition detection-probability target; default 0.9. Also CarrierAcquisition's own `pd`. 
* `doppler_uncertainty` One-sided Doppler search half-range, Hz; default 100.0. 
* `segments` Live-tracking Dll's own segments; default 4. 
* `sps` MpskReceiver's samples/symbol; default 8. 
* `differential` MpskReceiver's differential demap; default 0 (coherent). It acts in MpskReceiver's bits(), which this object does not offer, so steps()' symbols are the same either way (#2471). 
* `refine_max_error_db` Max tolerable async-lookback correlation-power loss driving the refine-stage collection Dll's coherent-I&D window count via [**dp\_dll\_lookback\_segments()**](dll__core_8h.md#function-dp_dll_lookback_segments). Oversampling the epoch is required for the asynchronous data: the residual carrier rides a ~symbol\_rate-wide data-modulated spectrum, so segments&gt;1 (default yields 11 at tsamps=2046) samples it above Nyquist; segments=1 undersamples and aliases it. Default 0.5. 
* `refine_samples_per_symbol` CarrierAcquisition's own operating rate = this \* symbol\_rate; default 4. 
* `refine_design_margin_db` Empirical derating of cn0\_dbhz before CarrierAcquisition's design\_snr; default 14.0. 
* `refine_n_fft` CarrierAcquisition's own block size; default 64. 
* `refine_zero_pad` CarrierAcquisition's own zero\_pad; default 8. 
* `refine_sequential` CarrierAcquisition's sequential mode; default false  its early per-block test fires on far too little averaging at SPEC's Es/N0 floor (as few as 4 blocks, 150-200+ Hz off), so false waits the full design\_snr-derived dwell. 
* `refine_max_n_blocks` CarrierAcquisition's give-up cap in sequential mode, and the cap on the dwell in either mode; default 100000. 
* `carrier_freq_hz` Nominal RF carrier frequency, Hz, enabling carrier-&gt;code aiding; 0.0 (default) = off. When &gt; 0, the coupled code-rate Doppler (carrier\_offset/carrier\_freq) is fed to the tracking Dll via [**dp\_dll\_set\_rate\_aid()**](dll__core_8h.md#function-dp_dll_set_rate_aid) so the code loop rides a dilated clock the discriminator alone can't pull in at low SNR. Set to the receiver's own downlink RF frequency for a physically-coupled Doppler capture. Coupled, it is above half the sample rate ([**dp\_acq\_carrier\_freq\_ok()**](acq__core_8h.md#function-dp_acq_carrier_freq_ok)). 
* `lost_confirm_s` Release rule: both lock flags down, continuously, for longer than this many seconds puts the receiver in the lost state (see get\_lost()). Size it past the longest fade the link must ride. The clock also runs from the first tracking sample, when neither flag is up yet, so a hand-off that never locks within the interval is released the same way as an emitter that leaves. Default 0.0 = never  the exit is then reset(). 
* `reacquire` Release on symbol lock alone, and search again by itself. With it set, the release clock counts samples of tracking with the SYMBOL lock flag down (code lock is not consulted), and when it passes lost\_confirm\_s the receiver resets itself to searching instead of entering lost: it never waits for reset(). A pool of receivers that are always online uses it. The clock runs from the first tracking sample, as above, so a track that never locks (a noise false alarm) is released too; size lost\_confirm\_s past the pull-in time to symbol lock, or a slow lock is released before it comes. Requires a lost\_confirm\_s of at least one input sample, since a release that never fires would never search again; a blob in the lost state is refused by set\_state(). Default false: the release rule above. 
* `code_only_s` Seconds of code-only signal the waveform holds at any chip phase (a symbol-free window; design async-dsss-parallel.md §4), &gt;= 0 and finite. The receiver hands its searcher `code_only_epochs = floor(code_only_s * chip_rate / code_len) - 1` (at least 1), and the searcher's slow-time depth follows: `coherent_bins = D`, at most `(code_only_epochs + 1)/2`, rounded down to a 7-smooth length ([**dp\_acq\_create\_continuous()**](acq__core_8h.md#function-dp_acq_create_continuous)). Default 0: no window, D = 1. 
* `doppler_rate` Doppler rate in Hz/s, &gt;= 0 and finite, that the slow-time depth is bounded against: at most `f_epoch / sqrt(2 * doppler_rate)` epochs ([**dp\_acq\_create\_continuous()**](acq__core_8h.md#function-dp_acq_create_continuous)). Default 0: the window is the only bound. 



**Returns:**

A new receiver, searching; NULL on an invalid argument, including reacquire with a lost\_confirm\_s under one sample. 
```C++
>>> import numpy as np
>>> from doppler.dsss import AsyncDsssReceiver
>>> from doppler.wfm import Gold
>>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
>>> rx = AsyncDsssReceiver(code, chip_rate=3.069e6, symbol_rate=2700.0,
...                        spc=2, doppler_uncertainty=500.0)
>>> (rx.tracking, rx.refining, rx.lost)       # searching
(0, 0, 0)
```
 





        

<hr>



### function dp\_async\_dsss\_receiver\_destroy 

_Destroy a receiver and release every child._ 
```C++
void dp_async_dsss_receiver_destroy (
    dp_async_dsss_receiver_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function dp\_async\_dsss\_receiver\_get\_car\_last\_error 

_Pre-despread Costas phase discriminator (rad): the residual carrier phase LOOP 1 (which de-rotates before the Dll) is not nulling._ 
```C++
double dp_async_dsss_receiver_get_car_last_error (
    const dp_async_dsss_receiver_state_t * state
) 
```




<hr>



### function dp\_async\_dsss\_receiver\_get\_car\_nco\_freq 

_LOOP 1 (pre-despread Costas) loop-filter output = NCO frequency command, cycles/sample of the front-end (chip\_rate\*spc) rate._ 
```C++
double dp_async_dsss_receiver_get_car_nco_freq (
    const dp_async_dsss_receiver_state_t * state
) 
```




<hr>



### function dp\_async\_dsss\_receiver\_get\_chip\_phase 

_The live Dll's code phase, chips, in the Dll's instantaneous convention_  _the mirror of_[_**acq\_result\_t::code\_phase**_](structacq__result__t.md#variable-code_phase) _'s correlation-lag convention (_[_**dp\_acq\_build\_handoff()**_](acq__core_8h.md#function-dp_acq_build_handoff) _)._
```C++
double dp_async_dsss_receiver_get_chip_phase (
    const dp_async_dsss_receiver_state_t * state
) 
```




<hr>



### function dp\_async\_dsss\_receiver\_get\_cn0\_dbhz\_est 

_The seed's C/N0 estimate, dB-Hz, unchanged until reset()._ 
```C++
double dp_async_dsss_receiver_get_cn0_dbhz_est (
    const dp_async_dsss_receiver_state_t * state
) 
```




<hr>



### function dp\_async\_dsss\_receiver\_get\_code\_locked 

_Binary code-lock flag from the live tracking Dll's own verify-counted (pfa-tuned) lock detector — the fundamental DSSS "am I
despreading" lock, de-chattered by up/down hysteresis._ 
```C++
int dp_async_dsss_receiver_get_code_locked (
    const dp_async_dsss_receiver_state_t * state
) 
```




<hr>



### function dp\_async\_dsss\_receiver\_get\_code\_rate 

_The Dll's code rate, chips per nominal chip (~1.0 at lock), without the carrier aid; a coasting Dll reports its held rate._ 
```C++
double dp_async_dsss_receiver_get_code_rate (
    const dp_async_dsss_receiver_state_t * state
) 
```




<hr>



### function dp\_async\_dsss\_receiver\_get\_coherent\_bins 

_The searcher's slow-time depth D: epochs per coherent block._ 
```C++
size_t dp_async_dsss_receiver_get_coherent_bins (
    const dp_async_dsss_receiver_state_t * state
) 
```



Derived at create from `code_only_s` and `doppler_rate` ([**dp\_async\_dsss\_receiver\_create()**](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_create)); 1 without a code-only window. Every search dwell after a reset() starts on a multiple of `D * code_len * spc` input samples counted from create: the receiver discards input up to that boundary first, at most one block. Receivers fed the same stream from create therefore search on the same block grid.




**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

D, &gt;= 1. 
```C++
>>> import numpy as np
>>> from doppler.dsss import AsyncDsssReceiver
>>> from doppler.wfm import Gold
>>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
>>> rx = AsyncDsssReceiver(code, chip_rate=3.069e6, symbol_rate=2700.0,
...                        spc=2, doppler_uncertainty=500.0)
>>> rx.coherent_bins                  # no code-only window
1
>>> rx = AsyncDsssReceiver(code, chip_rate=3.069e6, symbol_rate=2700.0,
...                        spc=2, doppler_uncertainty=500.0,
...                        code_only_s=0.0627)
>>> rx.coherent_bins                  # 187 epochs: 94, run 7-smooth
90
```
 





        

<hr>



### function dp\_async\_dsss\_receiver\_get\_doppler\_hz 

_The Doppler estimate the chain was built from, Hz: the seed's while refining, the CarrierAcquisition-refined value once tracking. The live carrier is status().doppler\_hz._ 
```C++
double dp_async_dsss_receiver_get_doppler_hz (
    const dp_async_dsss_receiver_state_t * state
) 
```




<hr>



### function dp\_async\_dsss\_receiver\_get\_lock 

_The carrier lock statistic of the MpskReceiver this receiver tracks with: the EMA of its M-th-power lock signal, near 1 when locked and near 0 on noise._ 
```C++
double dp_async_dsss_receiver_get_lock (
    const dp_async_dsss_receiver_state_t * state
) 
```




<hr>



### function dp\_async\_dsss\_receiver\_get\_lock\_metric 

_Symbol-lock metric = SNR-weighted EMA of (I^2-Q^2)/(I^2+Q^2) = cos(2\*phi) over the emitted symbols (locked -&gt; ~+1). Drives_ `locked` _._
```C++
double dp_async_dsss_receiver_get_lock_metric (
    const dp_async_dsss_receiver_state_t * state
) 
```




<hr>



### function dp\_async\_dsss\_receiver\_get\_lock\_threshold 

_The lock-metric threshold_ `locked` _is declared strictly above (0.5, ASYNC\_DSSS\_RX\_LOCK\_UP); exposed alongside lock\_metric for engineering debug._
```C++
double dp_async_dsss_receiver_get_lock_threshold (
    const dp_async_dsss_receiver_state_t * state
) 
```




<hr>



### function dp\_async\_dsss\_receiver\_get\_locked 

_The receiver's symbol-lock flag: a hysteretic detector over get\_lock\_metric(), declared after 30 consecutive symbols strictly above get\_lock\_threshold() (0.5) and dropped after 15 consecutive symbols strictly below 0.3 (ASYNC\_DSSS\_RX\_LOCK\_N\_UP, \_UP, \_N\_DOWN and \_DOWN, fixed at create). The flag the release rule and a pool's holder act on._ 
```C++
int dp_async_dsss_receiver_get_locked (
    const dp_async_dsss_receiver_state_t * state
) 
```




<hr>



### function dp\_async\_dsss\_receiver\_get\_lost 

_1 once the release rule has fired: both lock flags were down, continuously, for longer than_ `lost_confirm_s` _while tracking_ _an emitter that left, or a seed that never locked. The loops have stopped and samples are consumed and discarded; the holder releases the assignment and calls reset(). 0 in every other state, and always 0 with_`lost_confirm_s = 0` _._
```C++
int dp_async_dsss_receiver_get_lost (
    const dp_async_dsss_receiver_state_t * state
) 
```




<hr>



### function dp\_async\_dsss\_receiver\_get\_mpsk\_last\_error 

_MpskReceiver carrier phase discriminator (rad): the residual carrier phase LOOP 2 (post-despread) is not nulling._ 
```C++
double dp_async_dsss_receiver_get_mpsk_last_error (
    const dp_async_dsss_receiver_state_t * state
) 
```




<hr>



### function dp\_async\_dsss\_receiver\_get\_n 

```C++
int dp_async_dsss_receiver_get_n (
    const dp_async_dsss_receiver_state_t * state
) 
```




<hr>



### function dp\_async\_dsss\_receiver\_get\_nco\_freq 

_The MpskReceiver's carrier NCO command, its loop filter's whole output, cycles/sample at its input rate (_ `sps * symbol_rate` _). Its mean tracks a Doppler ramp with no lag (unlike get\_norm\_freq()); its variance is the carrier loop's stress._
```C++
double dp_async_dsss_receiver_get_nco_freq (
    const dp_async_dsss_receiver_state_t * state
) 
```




<hr>



### function dp\_async\_dsss\_receiver\_get\_norm\_freq 

_The post-despread carrier the MpskReceiver tracks: its centre plus its loop's integrator, cycles/sample at its input rate (_ `sps * symbol_rate` _). Lags a Doppler ramp by the Type-II ramp error._
```C++
double dp_async_dsss_receiver_get_norm_freq (
    const dp_async_dsss_receiver_state_t * state
) 
```




<hr>



### function dp\_async\_dsss\_receiver\_get\_refining 

_1 while refining: the CarrierAcquisition dwell; 0 in every other state._ 
```C++
int dp_async_dsss_receiver_get_refining (
    const dp_async_dsss_receiver_state_t * state
) 
```




<hr>



### function dp\_async\_dsss\_receiver\_get\_segments 

```C++
size_t dp_async_dsss_receiver_get_segments (
    const dp_async_dsss_receiver_state_t * state
) 
```




<hr>



### function dp\_async\_dsss\_receiver\_get\_sps 

```C++
size_t dp_async_dsss_receiver_get_sps (
    const dp_async_dsss_receiver_state_t * state
) 
```




<hr>



### function dp\_async\_dsss\_receiver\_get\_state 

```C++
void dp_async_dsss_receiver_get_state (
    const dp_async_dsss_receiver_state_t * state,
    void * blob
) 
```




<hr>



### function dp\_async\_dsss\_receiver\_get\_threads 

_Workers the searcher runs, the calling thread included; 1 = serial. Set with_ [_**dp\_async\_dsss\_receiver\_set\_threads()**_](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_set_threads) _._
```C++
int dp_async_dsss_receiver_get_threads (
    const dp_async_dsss_receiver_state_t * state
) 
```




<hr>



### function dp\_async\_dsss\_receiver\_get\_tracking 

_1 once the live tracking chain is built and demodulating; 0 in every other state._ 
```C++
int dp_async_dsss_receiver_get_tracking (
    const dp_async_dsss_receiver_state_t * state
) 
```




<hr>



### function dp\_async\_dsss\_receiver\_reset 

_Return to the searching state. Resets the embedded Acquisition and rebuilds the refine-stage chain and the live tracking chain back to their placeholder seed (phase 0, no Doppler). A receiver that has locked cannot be "reset back to tracking the same signal," only back to searching_  _every object's reset() semantics in this codebase._
```C++
void dp_async_dsss_receiver_reset (
    dp_async_dsss_receiver_state_t * state
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



### function dp\_async\_dsss\_receiver\_seed 

_Take a detection from outside and start refining from it._ 
```C++
int dp_async_dsss_receiver_seed (
    dp_async_dsss_receiver_state_t * state,
    double chip_phase,
    double doppler_hz_est,
    double cn0_dbhz_est
) 
```



The hand-off of docs/design/async-dsss-receiver.md §2.2: the three numbers a searcher's hit carries that this receiver uses  `acq_handoff_t`'s `chip_phase`, `doppler_hz_est` and `cn0_dbhz_est`  exactly as its own hit would have produced them (`steps()` calls this on its own hit). `chip_phase` is the code's instantaneous phase in chips, Dll's convention, at the FIRST sample of the next `steps()` call; [**dp\_acq\_build\_handoff()**](acq__core_8h.md#function-dp_acq_build_handoff) makes it from a hit's lag. The refine chain is rebuilt from the seed and the state becomes refining; the unconsumed tail is the caller's to feed.


Refused (`DP_ERR_INVALID`, nothing changes) on a receiver that is not waiting for one  refining, tracking or lost  because "assigned once" is a property of the object, not of the caller's bookkeeping; `reset()` releases it. Accepted while searching: an outside hit simply beats the receiver's own. Also refused outside the seed domain, [**dp\_async\_dsss\_receiver\_seed\_ok()**](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_seed_ok).




**Parameters:**


* `state` Must be non-NULL. 
* `chip_phase` Code phase at the next sample, chips, in `[0, code_len)`. 
* `doppler_hz_est` Coarse Doppler estimate, Hz (the refine stage sharpens it). 
* `cn0_dbhz_est` The hit's C/N0 estimate, dB-Hz; reported back by get\_cn0\_dbhz\_est(), unchanged until reset(). 



**Returns:**

`DP_OK`, or `DP_ERR_INVALID` when refused. 
```C++
>>> import numpy as np
>>> from doppler.dsss import AsyncDsssReceiver
>>> from doppler.wfm import Gold
>>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
>>> rx = AsyncDsssReceiver(code, chip_rate=3.069e6,
...                        symbol_rate=2700.0, spc=2)
>>> rx.seed(chip_phase=512.25, doppler_hz_est=-1500.0,
...         cn0_dbhz_est=48.0)
>>> (rx.refining, rx.doppler_hz, rx.cn0_dbhz_est)
(1, -1500.0, 48.0)

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



### function dp\_async\_dsss\_receiver\_seed\_ok 

_The seed domain: whether_ `(chip_phase, doppler_hz_est)` _is one_[_**dp\_async\_dsss\_receiver\_seed()**_](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_seed) _accepts, and one a live receiver can hold._
```C++
int dp_async_dsss_receiver_seed_ok (
    const dp_async_dsss_receiver_state_t * state,
    double chip_phase,
    double doppler_hz_est
) 
```



`chip_phase` must be finite and inside `[0, code_len)`, and `doppler_hz_est` must be strictly inside `(-fs/2, fs/2)`, where `fs` is `chip_rate * spc`. A Doppler at or past half the sample rate is not reportable ([**dp\_acq\_carrier\_freq\_ok()**](acq__core_8h.md#function-dp_acq_carrier_freq_ok) relies on this bound), and a seed there sent the hand-off's phase to NaN (doppler#2103).


seed() refuses by this predicate, set\_state() restores the seed fields only inside it, the searching receiver's hit path does not seed from a hit it fails, and the hand-over holds the REFINED Doppler to it too: a seed in range whose residual carries it to or past fs/2 is refused at the hand-over, counted in refused\_hits, and the receiver searches again rather than track a Doppler it cannot report. It reads only the receiver's fixed config (code length, chip rate, spc), so any receiver built from the same config gives the same answer.




**Parameters:**


* `state` Must be non-NULL. 
* `chip_phase` Code phase, chips. 
* `doppler_hz_est` Doppler estimate, Hz. 



**Returns:**

1 inside the seed domain, 0 outside it (a NaN is outside). 





        

<hr>



### function dp\_async\_dsss\_receiver\_set\_refine\_min\_blocks 

_Floor the refine's dwell at_ `n_blocks` _, whatever the detection sizing asks (docs/design/async-dsss-receiver-measurements.md §12.16, #1265)._
```C++
int dp_async_dsss_receiver_set_refine_min_blocks (
    dp_async_dsss_receiver_state_t * state,
    size_t n_blocks
) 
```



CarrierAcquisition's dwell is sized for DETECTION at the derated C/N0 (`cn0_dbhz - refine_design_margin_db`), so it shortens as the C/N0 rises  two blocks at 45 dB-Hz with the shipped margin  while the noise of the estimate it hands the tracking chain does not shorten with it: 210 Hz at two blocks against a chain that pulls in from a few hundred, so one hand-over in sixty landed outside and tracked the code with the carrier never locked. Seven blocks (42 ms, the default and the floor dwell of docs/design/async-dsss-receiver-measurements.md §12.10) hold the estimate to 77 Hz. Applied to the next refine chain built  a receiver already refining keeps its dwell. A mutator's value, so it travels in the state blob (#2022). `n_blocks` of 0 removes the floor; the value is clamped by `refine_max_n_blocks` where that cap is lower.




**Parameters:**


* `state` Must be non-NULL. 
* `n_blocks` The floor, blocks. 



**Returns:**

`DP_OK`. 
```C++
>>> from doppler.dsss import AsyncDsssReceiver
>>> rx = AsyncDsssReceiver(code=[1, 0, 1, 1, 0, 0, 1], chip_rate=1e6,
...                        symbol_rate=1e6 / 28.0, spc=4, cn0_dbhz=60.0)
>>> rx.refine_min_blocks                     # the default floor
7
>>> rx.set_refine_min_blocks(12)
>>> rx.refine_min_blocks
12
```
 





        

<hr>



### function dp\_async\_dsss\_receiver\_set\_state 

```C++
int dp_async_dsss_receiver_set_state (
    dp_async_dsss_receiver_state_t * state,
    const void * blob
) 
```




<hr>



### function dp\_async\_dsss\_receiver\_set\_telemetry 

_Attach (or detach) a telemetry context across the receiver's loops, and register the receiver's own lifecycle and symbol-lock decision._ 
```C++
int dp_async_dsss_receiver_set_telemetry (
    dp_async_dsss_receiver_state_t * state,
    dp_tlm_t * tlm,
    const char * prefix,
    uint32_t decim
) 
```



The receiver reports under the prefix, once per steps() call that carries samples, after the call, so on the time axis at the stamp the caller set for that block (dp\_tlm\_set\_now()):
* "&lt;prefix&gt;.state"  where the call left the receiver, the ASYNC\_DSSS\_RX\_\* value: 0 searching, 1 refining, 2 tracking, 4 lost (status().state);
* "&lt;prefix&gt;.handover\_hz"  the Doppler the chain was built from, Hz ([**dp\_async\_dsss\_receiver\_get\_doppler\_hz()**](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_get_doppler_hz)): 0 before a seed, the seed's while refining, the refined value from the hand-over on, and 0 again after a refused hand-over or a reset(), which return the receiver to searching. A record is a float, so it resolves about 0.5 Hz near 5 MHz. Not the live carrier: status().doppler\_hz reads that, and ".car.freq" and ".car.nco" are the carrier loop's own view of it, in cycles a sample at the input rate, so times chip\_rate \* spc for Hz;
* "&lt;prefix&gt;.refused"  the hits refused as seeds so far (status().refused\_hits).




Feed blocks of about a code period, as the examples do, and these read as a time series of the lifecycle: when the search hit, how long the refine took, what it handed over, when the release came.


Each loop reports under the prefix, at its own update rate:
* "&lt;prefix&gt;.car"  the chip-domain carrier loop ([**dp\_costas\_set\_telemetry()**](costas__core_8h.md#function-dp_costas_set_telemetry): ".lock", ".e", ".freq", ".locked", ".nco"), once per carrier update: once a code period, except while it coasts, when loop 1 is held;
* "&lt;prefix&gt;.code"  the code loop ([**dp\_dll\_set\_telemetry()**](dll__core_8h.md#function-dp_dll_set_telemetry): ".e", ".rate", ".lock", ".locked", ".nco"), once per discriminator update;
* "&lt;prefix&gt;.demod"  the symbol-domain demodulator ([**dp\_mpsk\_receiver\_set\_telemetry()**](mpsk__receiver__core_8h.md#function-dp_mpsk_receiver_set_telemetry): its lock, its carrier and timing loops, the symbols, its AGC), once per recovered symbol (the AGC's pair once per gain update);
* "&lt;prefix&gt;.acq"  the acquisition engine ([**dp\_acq\_set\_telemetry()**](acq__core_8h.md#function-dp_acq_set_telemetry)), once per decided dwell;
* "&lt;prefix&gt;.rc"  the rate converter between the code loop and the demodulator ([**dp\_RateConverter\_set\_telemetry()**](RateConverter__core_8h.md#function-dp_rateconverter_set_telemetry)). It reports only an AGC, and this receiver builds it without one, so it registers no probes;
* "&lt;prefix&gt;.sym.lock" and "&lt;prefix&gt;.sym.locked"  the receiver's own symbol-lock metric ([**dp\_async\_dsss\_receiver\_get\_lock\_metric()**](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_get_lock_metric)) and its decision ([**dp\_async\_dsss\_receiver\_get\_locked()**](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_get_locked)), the one its users act on, once per emitted symbol.




The carrier and code loops report their filters two ways: ".car.freq" and ".code.rate" (and the demodulator's ".demod.car.freq") are the integrators, each loop's frequency memory, which a coast holds; ".car.nco", ".code.nco" and ".demod.car.nco" are the complete commands the NCOs run at, the proportional term (and the code loop's carrier aid) included. ".demod.sync.rate" is the timing loop's output, and ".e" is each discriminator's input to its filter.


The attachment is a REQUEST kept on the receiver: every seed rebuilds the tracking chain, and the rebuilt loops are attached again under the same names (so the same probe ids). Records are therefore continuous across re-seeds, and a reset keeps the attachment too.


Setup path, never hot; the context is borrowed and must outlive the attachment, and the receiver must step on one thread (SPSC rules in [**dp\_tlm/dp\_tlm\_core.h**](dp__tlm__core_8h.md)). Passing NULL detaches every loop.




**Parameters:**


* `state` Must be non-NULL. 
* `tlm` Telemetry context to attach, or NULL to detach. 
* `prefix` Probe-name prefix; NULL means "rx". 
* `decim` Emit every decim-th event on each probe; &gt;= 1. 



**Returns:**

DP\_OK, or DP\_ERR\_INVALID on decim 0, a prefix too long for some probe's name  refused before anything registers on `tlm`, because the loops are first attached to a scratch context that finds the refusal  or a probe table with no room for every probe (found the same way, from the names the scratch holds). Every refusal leaves `tlm's` table untouched and the receiver as it was: detached, or still on its previous context under its previous prefix. 
```C++
>>> import numpy as np
>>> from doppler.dsss import AsyncDsssReceiver
>>> from doppler.telemetry import Telemetry
>>> from doppler.wfm import Gold
>>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
>>> rx = AsyncDsssReceiver(code, chip_rate=3.069e6, symbol_rate=2700.0,
...                        spc=2, doppler_uncertainty=500.0)
>>> tlm = Telemetry(1 << 12)
>>> rx.set_telemetry(tlm, "rx")
>>> sorted(p for p in tlm.probe_names if p.startswith("rx.c"))
['rx.car.e', 'rx.car.freq', 'rx.car.lock', 'rx.car.locked', 'rx.car.nco', 'rx.code.e', 'rx.code.lock', 'rx.code.locked', 'rx.code.nco', 'rx.code.rate']
>>> sorted(p for p in tlm.probe_names if p.startswith(("rx.s", "rx.h",
...                                                    "rx.r")))
['rx.handover_hz', 'rx.refused', 'rx.state', 'rx.sym.lock', 'rx.sym.locked']
>>> rx.set_telemetry(None, "rx")      # detach
```
 





        

<hr>



### function dp\_async\_dsss\_receiver\_set\_threads 

_Set the workers the searcher fans its Doppler tiles across._ 
```C++
int dp_async_dsss_receiver_set_threads (
    dp_async_dsss_receiver_state_t * state,
    int n
) 
```



Forwards to [**dp\_acq\_set\_threads()**](acq__core_8h.md#function-dp_acq_set_threads) on the receiver's searcher. The search is bit-identical at every count, so this changes the cost of a steps() call and nothing about its output. A pool that steps its receivers in parallel sets each one to 1. A mutator's value, so it travels in the state blob (#2022). Setup path; not while another thread is inside steps().




**Parameters:**


* `state` Must be non-NULL. 
* `n` Thread count; 0 = online cores, 1 = serial. 



**Returns:**

`DP_OK`. The count actually running is [**dp\_async\_dsss\_receiver\_get\_threads()**](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_get_threads); a searcher with one Doppler tile always runs 1. 
```C++
>>> from doppler.dsss import AsyncDsssReceiver
>>> rx = AsyncDsssReceiver(code=[1, 0, 1, 1, 0, 0, 1], chip_rate=1e6,
...                        symbol_rate=1e6 / 28.0, spc=4, cn0_dbhz=60.0)
>>> rx.set_threads(1)
>>> rx.threads
1
```
 





        

<hr>



### function dp\_async\_dsss\_receiver\_state\_bytes 

```C++
size_t dp_async_dsss_receiver_state_bytes (
    const dp_async_dsss_receiver_state_t * state
) 
```




<hr>



### function dp\_async\_dsss\_receiver\_status 

_Read the status record, by value: the state; where the emitter is now (the live carrier), its chip phase, code rate and C/N0; both lock flags with the symbol-lock metric and threshold; both carrier residuals; and the two clocks in input samples (since the state was entered; both flags down without a break)._ 
```C++
async_dsss_receiver_status_t dp_async_dsss_receiver_status (
    const dp_async_dsss_receiver_state_t * state
) 
```



Cheap and allocation-free: every field is a read of live state. The one-at-a-time getters report the same fields, `doppler_hz` apart (see the record); this is the face a controller reads. No timestamp: the clocks are in input samples, and the holder, which owns the sample clock, stamps the record.




**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

The record, by value. 
```C++
>>> import numpy as np
>>> from doppler.dsss import AsyncDsssReceiver
>>> from doppler.wfm import Gold
>>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
>>> rx = AsyncDsssReceiver(code, chip_rate=3.069e6,
...                        symbol_rate=2700.0, spc=2)
>>> st = rx.status()
>>> (st.state, st.doppler_hz, st.code_locked, st.locked)   # searching
(0, 0.0, 0, 0)
>>> rx.seed(chip_phase=100.0, doppler_hz_est=-250.0, cn0_dbhz_est=50.0)
>>> st = rx.status()
>>> (st.state, round(st.doppler_hz, 6), st.cn0_dbhz_est)  # refining
(1, -250.0, 50.0)
>>> _ = rx.steps(np.zeros(2046, np.complex64))
>>> rx.status().state_samples                             # since seed
2046
```
 





        

<hr>



### function dp\_async\_dsss\_receiver\_steps 

_Stream raw cf32 samples; emit demodulated symbols once tracking._ 
```C++
size_t dp_async_dsss_receiver_steps (
    dp_async_dsss_receiver_state_t * state,
    const float _Complex * x,
    size_t x_len,
    float _Complex * out,
    size_t max_out
) 
```



Drives the state machine. It emits nothing while searching or refining (an empty return is normal, not an error): a hit seeds the frozen-carrier refine chain, `CarrierAcquisition` sharpens the coarse Doppler estimate, and only once it is ready (or gives up) is the live tracking chain built and demodulation begins. A hit inside a call hands the rest of the call straight to the refine chain, so no sample is dropped there; at the refine's end, what the refine did not consume of the last call goes on to the live chain. Accepts any block size; state carries across calls, so a capture can be fed in frames of any length with no seam. Lost (after the release rule fires) consumes the samples and emit nothing, so the feeding loop is the same in every state; while tracking, the release clock runs on the two lock flags after every call, or on the symbol lock flag alone with `reacquire` (see `lost_confirm_s`). Tracking emits symbols whether or not they are data: a noise false alarm can reach tracking and emit until the release rule lets it go  or, at the default lost\_confirm\_s of 0, until reset()  and symbol lock (`locked`) is what says the symbols are a signal's, so read it rather than the count (#2439). Under SPEC's coupled offset + 500 Hz/s Doppler ramp the pre-despread Costas removes the full carrier dynamics before the code loop, so the recovered constellation lands cleanly on the BPSK real axis.




**Parameters:**


* `state` Must be non-NULL. 
* `x` Input cf32 samples. 
* `x_len` Number of input samples. 
* `out` Output symbols; caller provides `max_out` capacity. 
* `max_out` Output capacity. steps() never writes past it, and the symbols past it are dropped, not held for the next call. `x_len` is not always enough: a call that completes a carried code period emits the whole period's symbols (#2469). 



**Returns:**

Number of symbols written: 0 while searching, refining or lost, or before a full symbol's worth of input. 
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



### function dp\_async\_dsss\_receiver\_steps\_max\_out 

_The output capacity steps() needs beyond the input's._ 
```C++
size_t dp_async_dsss_receiver_steps_max_out (
    dp_async_dsss_receiver_state_t * state
) 
```



Returns 0, which is NOT a bound. A call that completes a code period carried over from earlier calls emits all of that period's symbols (code\_len times symbol\_rate over chip\_rate of them), even if it brings one sample. At more than one symbol per period a call can therefore emit more symbols than its `x_len`, and those past `max_out` are dropped (see [**dp\_async\_dsss\_receiver\_steps()**](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_steps)). A true bound is #2469.




**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

0. 





        

<hr>
## Macro Definition Documentation





### define ASYNC\_DSSS\_RECEIVER\_STATE\_MAGIC 

```C++
#define ASYNC_DSSS_RECEIVER_STATE_MAGIC `DP_FOURCC ('A', 'D', 'R', 'X')`
```




<hr>



### define ASYNC\_DSSS\_RECEIVER\_STATE\_VERSION 

```C++
#define ASYNC_DSSS_RECEIVER_STATE_VERSION `12u /* v12: samples_in, the dwell alignment, threads; v11: no cell mode; v10: the dll children grew (output level, #2495); v9: refine_min_blocks; v8: refused_hits; v7: no hand-off flavor; v6: the cell pull-in */`
```




<hr>



### define ASYNC\_DSSS\_RX\_BN\_CARRIER 

```C++
#define ASYNC_DSSS_RX_BN_CARRIER `0.04`
```




<hr>



### define ASYNC\_DSSS\_RX\_CARRIER\_PULLIN\_HZ 

```C++
#define ASYNC_DSSS_RX_CARRIER_PULLIN_HZ (
    chip_rate,
    code_len
) `( ASYNC_DSSS_RX_BN_CARRIER * (chip_rate) / (2.0 * (double)(code_len)))`
```



The pre-despread carrier loop's acquisition bound, Hz: `bn / m` cycles per sample over its one-code-period update (`m = 2`, the squaring discriminator), `native/validation/costas_pullin.c`'s measured 100% line  reliable to twice it, dead by four. 97.8 Hz at 5 Mcps over Gold-1023. 


        

<hr>



### define ASYNC\_DSSS\_RX\_DEMOD\_BN\_CARRIER 

```C++
#define ASYNC_DSSS_RX_DEMOD_BN_CARRIER `0.01`
```




<hr>



### define ASYNC\_DSSS\_RX\_DEMOD\_BN\_TIMING 

```C++
#define ASYNC_DSSS_RX_DEMOD_BN_TIMING `0.005`
```




<hr>



### define ASYNC\_DSSS\_RX\_DEMOD\_LOCK\_THRESH 

```C++
#define ASYNC_DSSS_RX_DEMOD_LOCK_THRESH `0.3`
```




<hr>



### define ASYNC\_DSSS\_RX\_DEMOD\_RRC\_BETA 

```C++
#define ASYNC_DSSS_RX_DEMOD_RRC_BETA `0.35`
```




<hr>



### define ASYNC\_DSSS\_RX\_DEMOD\_RRC\_SPAN 

```C++
#define ASYNC_DSSS_RX_DEMOD_RRC_SPAN `8`
```




<hr>



### define ASYNC\_DSSS\_RX\_DEMOD\_ZETA 

```C++
#define ASYNC_DSSS_RX_DEMOD_ZETA `0.707`
```




<hr>



### define ASYNC\_DSSS\_RX\_DLL\_BN 

```C++
#define ASYNC_DSSS_RX_DLL_BN `0.002`
```




<hr>



### define ASYNC\_DSSS\_RX\_DLL\_SPACING 

```C++
#define ASYNC_DSSS_RX_DLL_SPACING `0.5`
```




<hr>



### define ASYNC\_DSSS\_RX\_DLL\_ZETA 

```C++
#define ASYNC_DSSS_RX_DLL_ZETA `0.707`
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



### define ASYNC\_DSSS\_RX\_REFINE\_DLL\_SPACING 

```C++
#define ASYNC_DSSS_RX_REFINE_DLL_SPACING `0.5`
```




<hr>



### define ASYNC\_DSSS\_RX\_REFINE\_MIN\_BLOCKS 

```C++
#define ASYNC_DSSS_RX_REFINE_MIN_BLOCKS `7u`
```



Default floor on the refine's dwell, blocks: the floor dwell of docs/design/async-dsss-receiver-measurements.md §12.10 (77 Hz of estimate noise at 45 dB-Hz); see [**dp\_async\_dsss\_receiver\_set\_refine\_min\_blocks()**](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_set_refine_min_blocks). 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/async_dsss_receiver/async_dsss_receiver_core.h`

