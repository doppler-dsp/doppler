

# File async\_dsss\_receiver\_core.h



[**FileList**](files.md) **>** [**async\_dsss\_receiver**](dir_565c73e0c8995e904663dd9fb6485ceb.md) **>** [**async\_dsss\_receiver\_core.h**](async__dsss__receiver__core_8h.md)

[Go to the source code of this file](async__dsss__receiver__core_8h_source.md)

_Composed continuous DSSS receiver: Acquisition -&gt; hand-off -&gt; CarrierAcquisition refine -&gt; carrier\_nda/Dll/RateConverter/ MpskReceiver track, one object._ [More...](#detailed-description)

* `#include "doppler/RateConverter/RateConverter_core.h"`
* `#include "doppler/correlator/correlator_core.h"`
* `#include "doppler/carrier_acq/carrier_acq_core.h"`
* `#include "doppler/cic/cic_core.h"`
* `#include "doppler/carrier_nda/carrier_nda_core.h"`
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
* `#include "doppler/detection/detection_core.h"`
* `#include "doppler/spectral/spectral_core.h"`
* `#include "doppler/fft/fft_core.h"`
* `#include "doppler/acc_trace/acc_trace_core.h"`
* `#include "doppler/ber/ber_core.h"`
* `#include "doppler/doppler_channel/doppler_channel_core.h"`
* `#include "doppler/dp_tlm/dp_tlm_core.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**async\_dsss\_receiver\_extra\_t**](structasync__dsss__receiver__extra__t.md) <br>_The receiver's own fields in its state blob, after the envelope and before the children._  |
| struct | [**async\_dsss\_receiver\_status\_t**](structasync__dsss__receiver__status__t.md) <br>_One consistent picture of what the receiver is doing, by value._  |
| struct | [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) <br>_Composed receiver state._  |


## Public Types

| Type | Name |
| ---: | :--- |
| enum  | [**async\_dsss\_rx\_state\_t**](#enum-async_dsss_rx_state_t)  <br> |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  double | [**dp\_async\_dsss\_receiver\_carrier\_bn**](#function-dp_async_dsss_receiver_carrier_bn) (double cn0\_dbhz, double chip\_rate, size\_t code\_len) <br>_The pre-despread carrier loop's bandwidth at a design C/N0: the owner's loop-SNR rule (async-dsss-receiver.md §1.3), no wider than ASYNC\_DSSS\_RX\_BN\_CARRIER._  |
|  int | [**dp\_async\_dsss\_receiver\_configure\_chain\_raw**](#function-dp_async_dsss_receiver_configure_chain_raw) ([**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state, size\_t segments, size\_t sps, int n) <br>_Pin the live-tracking despread/resample/demod grid directly. The escape hatch for the composition-specific knob:_ `segments` _(the live Dll's tracking parameter) and_`sps` _/_`n` _(MpskReceiver's samples and terminal outputs per symbol) are independently overridable, still bridged by a freshly-sized_`RateConverter` _and never coupled to each other. While searching/refining it re-pins the grid used to build the next tracking chain; once tracking it rebuilds_`dll` _/_`rc` _/_`rx` _in place, allocating every replacement before adopting it, and re-seeds the rebuilt chain from the whole carrier_ _loop 1 plus loop 2's integrator, the sum status() reports_ _so a retune keeps the signal it was tracking._ |
|  void | [**dp\_async\_dsss\_receiver\_configure\_lock\_raw**](#function-dp_async_dsss_receiver_configure_lock_raw) ([**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state, double up\_thresh, double down\_thresh, size\_t n\_looks, double alpha, uint32\_t n\_up, uint32\_t n\_down) <br>_Re-tune the live-tracking Dll's code-lock detector directly. Forwards to_ `dp_dll_configure_lock_raw()` _on the live tracking Dll (the one behind_`get_code_locked()` _), NOT the refine-stage collection Dll. It retunes the Dll the receiver holds now; every chain build (a seed, the hand-over, reset() or configure\_chain\_raw()) replaces that Dll with a freshly sized one, so call it once tracking. The detector is the hysteretic lockdet over the DLL's per-N-look CFAR statistic — the levels and verify counts trade declare latency against false-alarm rate._ |
|  int | [**dp\_async\_dsss\_receiver\_configure\_search\_raw**](#function-dp_async_dsss_receiver_configure_search_raw) ([**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state, size\_t doppler\_bins, size\_t n\_noncoh) <br>_Pin the embedded search's grid directly. Forwards to_ `dp_correlator_configure_search_raw()` __ _the escape hatch under this object's_`symbol_rate` _-driven auto-sizing, for a power user who wants a specific_`(doppler_bins, n_noncoh)` _. The pin replaces the whole auto-sized grid, the window-tiled search included. Only meaningful while searching; the search does not run again until the next_`reset()` _._ |
|  [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* | [**dp\_async\_dsss\_receiver\_create**](#function-dp_async_dsss_receiver_create) (const uint8\_t \* code, size\_t code\_len, double chip\_rate, double symbol\_rate, double sample\_rate, int m, double cn0\_dbhz, double pfa, double pd, double doppler\_uncertainty, size\_t segments, size\_t sps, int differential, double refine\_max\_error\_db, size\_t refine\_samples\_per\_symbol, double refine\_design\_margin\_db, size\_t refine\_n\_fft, size\_t refine\_zero\_pad, bool refine\_sequential, size\_t refine\_max\_n\_blocks, double carrier\_freq\_hz, double lost\_confirm\_s, bool reacquire, double code\_only\_s, double doppler\_rate, bool refine) <br>_Create an AsyncDsssReceiver in the searching state._  |
|  void | [**dp\_async\_dsss\_receiver\_destroy**](#function-dp_async_dsss_receiver_destroy) ([**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state) <br>_Destroy a receiver and release every child._  |
|  double | [**dp\_async\_dsss\_receiver\_get\_car\_last\_error**](#function-dp_async_dsss_receiver_get_car_last_error) (const [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state) <br>_The pre-despread carrier loop's last phase error (rad): the residual carrier phase LOOP 1 (which de-rotates before the Dll) is not nulling, as its loop filter takes it_  _carrier\_nda's discriminator divided by its slope (_[_**carrier\_nda\_loop\_error()**_](carrier__nda__core_8h.md#function-carrier_nda_loop_error) _), so radians near lock. The ".car.e" probe is the raw discriminator, twice this._ |
|  [**dp\_lockdet\_rule\_t**](structdp__lockdet__rule__t.md) | [**dp\_async\_dsss\_receiver\_get\_car\_lock\_rule**](#function-dp_async_dsss_receiver_get_car_lock_rule) (const [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state) <br>_The carrier loop's lock rule; see_ [_**dp\_async\_dsss\_receiver\_get\_sym\_lock\_rule()**_](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_get_sym_lock_rule) _._ |
|  double | [**dp\_async\_dsss\_receiver\_get\_car\_nco\_freq**](#function-dp_async_dsss_receiver_get_car_nco_freq) (const [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state) <br>_LOOP 1 (the pre-despread carrier loop) output = NCO frequency command, cycles/sample of the front-end (chip\_rate\*spc) rate._  |
|  double | [**dp\_async\_dsss\_receiver\_get\_chip\_phase**](#function-dp_async_dsss_receiver_get_chip_phase) (const [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state) <br>_The code phase, chips, at the sample after the last one fed, in the Dll's instantaneous convention_  _the mirror of dp\_correlator\_hit\_t::code\_phase's correlation-lag convention (_[_**dp\_correlator\_build\_handoff()**_](correlator__core_8h.md#function-dp_correlator_build_handoff) _). Tracking, it is the live Dll's phase carried over the partial period the chain holds (it runs whole periods), so it names the same sample a search's picks are carried to at any sample rate (#3063). While refining there is no live Dll: it is the seed's phase advanced by the clock dilation since, the phase the hand-over starts the live chain from._ |
|  double | [**dp\_async\_dsss\_receiver\_get\_cn0\_dbhz\_est**](#function-dp_async_dsss_receiver_get_cn0_dbhz_est) (const [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state) <br>_The seed's C/N0 estimate, dB-Hz, unchanged until reset()._  |
|  [**dp\_lockdet\_rule\_t**](structdp__lockdet__rule__t.md) | [**dp\_async\_dsss\_receiver\_get\_code\_lock\_rule**](#function-dp_async_dsss_receiver_get_code_lock_rule) (const [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state) <br>_The code loop's lock rule; see_ [_**dp\_async\_dsss\_receiver\_get\_sym\_lock\_rule()**_](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_get_sym_lock_rule) _._ |
|  int | [**dp\_async\_dsss\_receiver\_get\_code\_locked**](#function-dp_async_dsss_receiver_get_code_locked) (const [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state) <br>_Binary code-lock flag from the live tracking Dll's own verify-counted (pfa-tuned) lock detector — the fundamental DSSS "am I
despreading" lock, de-chattered by up/down hysteresis._  |
|  double | [**dp\_async\_dsss\_receiver\_get\_code\_rate**](#function-dp_async_dsss_receiver_get_code_rate) (const [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state) <br>_The Dll's code rate, chips per nominal chip (~1.0 at lock), without the carrier aid; a coasting Dll reports its held rate._  |
|  size\_t | [**dp\_async\_dsss\_receiver\_get\_coherent\_bins**](#function-dp_async_dsss_receiver_get_coherent_bins) (const [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state) <br>_The searcher's slow-time depth D: epochs per coherent block._  |
|  [**dp\_lockdet\_rule\_t**](structdp__lockdet__rule__t.md) | [**dp\_async\_dsss\_receiver\_get\_demod\_lock\_rule**](#function-dp_async_dsss_receiver_get_demod_lock_rule) (const [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state) <br>_The demodulator's carrier lock rule; see_ [_**dp\_async\_dsss\_receiver\_get\_sym\_lock\_rule()**_](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_get_sym_lock_rule) _._ |
|  [**dp\_lockdet\_rule\_t**](structdp__lockdet__rule__t.md) | [**dp\_async\_dsss\_receiver\_get\_demod\_sync\_lock\_rule**](#function-dp_async_dsss_receiver_get_demod_sync_lock_rule) (const [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state) <br>_The demodulator's timing lock rule; see_ [_**dp\_async\_dsss\_receiver\_get\_sym\_lock\_rule()**_](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_get_sym_lock_rule) _._ |
|  double | [**dp\_async\_dsss\_receiver\_get\_doppler\_hz**](#function-dp_async_dsss_receiver_get_doppler_hz) (const [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state) <br>_The HAND-OVER's Doppler, Hz: what the live chain was built from, frozen for the track's whole tenure_  _the seed's while refining, the refine's estimate from the hand-over on. It does not follow the carrier: under a 400 Hz/s ramp it is 400 Hz stale a second later. Where the emitter is now is status().doppler\_hz, a field of the same name with the other meaning._ |
|  int | [**dp\_async\_dsss\_receiver\_get\_handover\_source**](#function-dp_async_dsss_receiver_get_handover_source) (const [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state) <br>_Which estimate the live chain's Doppler came from: ASYNC\_DSSS\_RX\_HO\_NONE until a hand-over, then \_SEED or \_DATA (the refine's choice, §4.7), NONE again at the next seed._  |
|  double | [**dp\_async\_dsss\_receiver\_get\_lock**](#function-dp_async_dsss_receiver_get_lock) (const [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state) <br>_The carrier lock statistic of the MpskReceiver this receiver tracks with: the EMA of its M-th-power lock signal, near 1 when locked and near 0 on noise._  |
|  double | [**dp\_async\_dsss\_receiver\_get\_lock\_metric**](#function-dp_async_dsss_receiver_get_lock_metric) (const [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state) <br>_Symbol-lock metric = SNR-weighted EMA of (I^2-Q^2)/(I^2+Q^2) = cos(2\*phi) over the emitted symbols (locked -&gt; ~+1). Drives_ `locked` _._ |
|  double | [**dp\_async\_dsss\_receiver\_get\_lock\_threshold**](#function-dp_async_dsss_receiver_get_lock_threshold) (const [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state) <br>_The lock-metric threshold_ `locked` _is declared strictly above (0.5, ASYNC\_DSSS\_RX\_LOCK\_UP); exposed alongside lock\_metric for engineering debug._ |
|  int | [**dp\_async\_dsss\_receiver\_get\_locked**](#function-dp_async_dsss_receiver_get_locked) (const [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state) <br>_The receiver's symbol-lock flag: a hysteretic detector over get\_lock\_metric(), declared after 30 consecutive symbols strictly above get\_lock\_threshold() (0.5) and dropped after 15 consecutive symbols strictly below 0.3 (ASYNC\_DSSS\_RX\_LOCK\_N\_UP, \_UP, \_N\_DOWN and \_DOWN, fixed at create). The flag the release rule and a pool's holder act on._  |
|  int | [**dp\_async\_dsss\_receiver\_get\_lost**](#function-dp_async_dsss_receiver_get_lost) (const [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state) <br>_1 once the release rule has fired: both lock flags were down, continuously, for longer than_ `lost_confirm_s` _while tracking_ _an emitter that left, or a seed that never locked. The loops have stopped and samples are consumed and discarded; the holder releases the assignment and calls reset(). 0 in every other state, and always 0 with_`lost_confirm_s = 0` _._ |
|  double | [**dp\_async\_dsss\_receiver\_get\_mpsk\_last\_error**](#function-dp_async_dsss_receiver_get_mpsk_last_error) (const [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state) <br>_MpskReceiver carrier phase discriminator (rad): the residual carrier phase LOOP 2 (post-despread) is not nulling._  |
|  int | [**dp\_async\_dsss\_receiver\_get\_n**](#function-dp_async_dsss_receiver_get_n) (const [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state) <br>_MpskReceiver's terminal outputs per symbol (even, 2..8)._  |
|  size\_t | [**dp\_async\_dsss\_receiver\_get\_n\_exclusions**](#function-dp_async_dsss_receiver_get_n_exclusions) (const [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state) <br>_Code phases set\_exclusions() holds._  |
|  double | [**dp\_async\_dsss\_receiver\_get\_nco\_freq**](#function-dp_async_dsss_receiver_get_nco_freq) (const [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state) <br>_The MpskReceiver's carrier NCO command, its loop filter's whole output, cycles/sample at its input rate (_ `sps * symbol_rate` _). Its mean tracks a Doppler ramp with no lag (unlike get\_norm\_freq()); its variance is the carrier loop's stress._ |
|  double | [**dp\_async\_dsss\_receiver\_get\_norm\_freq**](#function-dp_async_dsss_receiver_get_norm_freq) (const [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state) <br>_The post-despread carrier the MpskReceiver tracks: its centre plus its loop's integrator, cycles/sample at its input rate (_ `sps * symbol_rate` _). Lags a Doppler ramp by the Type-II ramp error._ |
|  size\_t | [**dp\_async\_dsss\_receiver\_get\_rank**](#function-dp_async_dsss_receiver_get_rank) (const [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state) <br>_Which surviving peak of a dwell the search takes; 0 = the strongest. Set with_ [_**dp\_async\_dsss\_receiver\_set\_rank()**_](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_set_rank) _._ |
|  int | [**dp\_async\_dsss\_receiver\_get\_refining**](#function-dp_async_dsss_receiver_get_refining) (const [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state) <br>_1 while refining: the CarrierAcquisition dwell; 0 in every other state._  |
|  size\_t | [**dp\_async\_dsss\_receiver\_get\_segments**](#function-dp_async_dsss_receiver_get_segments) (const [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state) <br>_The live-tracking Dll's partial correlations per code period._  |
|  size\_t | [**dp\_async\_dsss\_receiver\_get\_sps**](#function-dp_async_dsss_receiver_get_sps) (const [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state) <br>_MpskReceiver's samples per symbol._  |
|  void | [**dp\_async\_dsss\_receiver\_get\_state**](#function-dp_async_dsss_receiver_get_state) (const [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state, void \* blob) <br>_Serialize the receiver: its own fields, then each child, then the carrier carry (live samples, zero-padded to tsamps)._  |
|  [**dp\_lockdet\_rule\_t**](structdp__lockdet__rule__t.md) | [**dp\_async\_dsss\_receiver\_get\_sym\_lock\_rule**](#function-dp_async_dsss_receiver_get_sym_lock_rule) (const [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state) <br>_The rules behind the receiver's five lock decisions, each its levels and verify counts (_ [_**dp\_lockdet\_rule\_t**_](structdp__lockdet__rule__t.md) _), for a caller that draws a lock statistic beside the line it is decided against._ |
|  int | [**dp\_async\_dsss\_receiver\_get\_threads**](#function-dp_async_dsss_receiver_get_threads) (const [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state) <br>_Workers the searcher runs, the calling thread included; 1 = serial. Set with_ [_**dp\_async\_dsss\_receiver\_set\_threads()**_](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_set_threads) _._ |
|  int | [**dp\_async\_dsss\_receiver\_get\_tracking**](#function-dp_async_dsss_receiver_get_tracking) (const [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state) <br>_1 once the live tracking chain is built and demodulating; 0 in every other state._  |
|  void | [**dp\_async\_dsss\_receiver\_reset**](#function-dp_async_dsss_receiver_reset) ([**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state) <br>_Return to the searching state. Resets the embedded search and rebuilds the refine-stage chain and the live tracking chain back to their placeholder seed (phase 0, no Doppler). A receiver that has locked cannot be "reset back to tracking the same signal," only back to searching_  _every object's reset() semantics in this codebase._ |
|  int | [**dp\_async\_dsss\_receiver\_seed**](#function-dp_async_dsss_receiver_seed) ([**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state, double chip\_phase, double doppler\_hz\_est, double cn0\_dbhz\_est) <br>_Take a detection from outside and start refining from it._  |
|  int | [**dp\_async\_dsss\_receiver\_seed\_ok**](#function-dp_async_dsss_receiver_seed_ok) (const [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state, double chip\_phase, double doppler\_hz\_est) <br>_The seed domain: whether_ `(chip_phase, doppler_hz_est)` _is one_[_**dp\_async\_dsss\_receiver\_seed()**_](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_seed) _accepts, and one a live receiver can hold._ |
|  int | [**dp\_async\_dsss\_receiver\_set\_exclusions**](#function-dp_async_dsss_receiver_set_exclusions) ([**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state, const double \* phases, size\_t phases\_len) <br>_Code phases the search must not take: signals another receiver already tracks._  |
|  int | [**dp\_async\_dsss\_receiver\_set\_rank**](#function-dp_async_dsss_receiver_set_rank) ([**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state, size\_t rank) <br>_Take the_ `rank-th` _surviving peak of a dwell, not the strongest._ |
|  int | [**dp\_async\_dsss\_receiver\_set\_refine\_min\_blocks**](#function-dp_async_dsss_receiver_set_refine_min_blocks) ([**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state, size\_t n\_blocks) <br>_Floor the refine's dwell at_ `n_blocks` _, whatever the detection sizing asks (docs/design/async-dsss-receiver-measurements.md §12.16, #1265)._ |
|  int | [**dp\_async\_dsss\_receiver\_set\_state**](#function-dp_async_dsss_receiver_set_state) ([**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state, const void \* blob) <br>_Restore a blob written by_ [_**dp\_async\_dsss\_receiver\_get\_state()**_](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_get_state) _._ |
|  int | [**dp\_async\_dsss\_receiver\_set\_telemetry**](#function-dp_async_dsss_receiver_set_telemetry) ([**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state, [**dp\_tlm\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_t) \* tlm, const char \* prefix, uint32\_t decim) <br>_Attach (or detach) a telemetry context across the receiver's loops, and register the receiver's own lifecycle and symbol-lock decision._  |
|  int | [**dp\_async\_dsss\_receiver\_set\_threads**](#function-dp_async_dsss_receiver_set_threads) ([**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state, int n) <br>_Set the workers the searcher fans its Doppler tiles across._  |
|  size\_t | [**dp\_async\_dsss\_receiver\_state\_bytes**](#function-dp_async_dsss_receiver_state_bytes) (const [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state) <br>_Size in bytes of the blob_ [_**dp\_async\_dsss\_receiver\_get\_state()**_](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_get_state) _writes._ |
|  [**async\_dsss\_receiver\_status\_t**](structasync__dsss__receiver__status__t.md) | [**dp\_async\_dsss\_receiver\_status**](#function-dp_async_dsss_receiver_status) (const [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state) <br>_Read the status record, by value: the state; where the emitter is now (the live carrier), its chip phase, code rate and C/N0; both lock flags with the symbol-lock metric and threshold; both carrier residuals; and the two clocks in input samples (since the state was entered; both flags down without a break)._  |
|  size\_t | [**dp\_async\_dsss\_receiver\_steps**](#function-dp_async_dsss_receiver_steps) ([**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state, const float \_Complex \* x, size\_t x\_len, float \_Complex \* out, size\_t max\_out) <br>_Stream raw cf32 samples; emit demodulated symbols once tracking._  |
|  size\_t | [**dp\_async\_dsss\_receiver\_steps\_max\_out**](#function-dp_async_dsss_receiver_steps_max_out) ([**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state) <br>_The output capacity steps() needs beyond the input's._  |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**ASYNC\_DSSS\_RECEIVER\_STATE\_MAGIC**](async__dsss__receiver__core_8h.md#define-async_dsss_receiver_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('A', 'D', 'R', 'X')`<br>_Envelope magic ("ADRX") identifying an AsyncDsssReceiver blob._  |
| define  | [**ASYNC\_DSSS\_RECEIVER\_STATE\_VERSION**](async__dsss__receiver__core_8h.md#define-async_dsss_receiver_state_version)  `22u /\* v22: the pre-despread carrier loop is carrier\_nda ('CNDA' children for car and car\_held); v21: the squared-stream refine estimator reverted, its fields gone (79fa50cfe); v20: no threads, the search's own blob carries set\_threads()'s request (#3055); v19: the squared-stream refine estimator (§4.7); v18: the exclusions' and picks' samples; v17: the refine hand-over's ring (#2974); v16: the search child is the correlator's search flavor ('CORL', #2752 3.4b); v15: pooled, search-only picks; v14: the search resampler and its origin; v13: rank, exclusions; v12: samples\_in, the dwell alignment, threads; v11: no cell mode; v10: the dll children grew (output level, #2495); v9: refine\_min\_blocks; v8: refused\_hits; v7: no hand-off flavor; v6: the cell pull-in \*/`<br>_Layout version of the AsyncDsssReceiver blob (history below)._  |
| define  | [**ASYNC\_DSSS\_RX\_BN\_CARRIER**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_bn_carrier)  `0.04`<br> |
| define  | [**ASYNC\_DSSS\_RX\_CARRIER\_LOOP\_SNR\_DB**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_carrier_loop_snr_db)  `20.0`<br> |
| define  | [**ASYNC\_DSSS\_RX\_CARRIER\_PULLIN\_HZ**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_carrier_pullin_hz) (bn, chip\_rate, code\_len) `((bn) \* (chip\_rate) / (2.0 \* (double)(code\_len)))`<br> |
| define  | [**ASYNC\_DSSS\_RX\_CAR\_DEN\_ALPHA**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_car_den_alpha)  `(1.0 / 256.0)`<br> |
| define  | [**ASYNC\_DSSS\_RX\_CAR\_LOCK\_PD**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_car_lock_pd)  `0.99`<br> |
| define  | [**ASYNC\_DSSS\_RX\_CAR\_LOCK\_PFA**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_car_lock_pfa)  `1e-3`<br> |
| define  | [**ASYNC\_DSSS\_RX\_DEMOD\_BN\_CARRIER**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_demod_bn_carrier)  `0.01`<br> |
| define  | [**ASYNC\_DSSS\_RX\_DEMOD\_BN\_TIMING**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_demod_bn_timing)  `0.005`<br> |
| define  | [**ASYNC\_DSSS\_RX\_DEMOD\_LOCK\_THRESH**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_demod_lock_thresh)  `0.3`<br> |
| define  | [**ASYNC\_DSSS\_RX\_DEMOD\_RRC\_BETA**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_demod_rrc_beta)  `0.35`<br> |
| define  | [**ASYNC\_DSSS\_RX\_DEMOD\_RRC\_SPAN**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_demod_rrc_span)  `8`<br> |
| define  | [**ASYNC\_DSSS\_RX\_DEMOD\_ZETA**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_demod_zeta)  `0.707`<br> |
| define  | [**ASYNC\_DSSS\_RX\_DLL\_BN**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_dll_bn)  `0.002`<br> |
| define  | [**ASYNC\_DSSS\_RX\_DLL\_SPACING**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_dll_spacing)  `0.5`<br> |
| define  | [**ASYNC\_DSSS\_RX\_DLL\_ZETA**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_dll_zeta)  `0.707`<br> |
| define  | [**ASYNC\_DSSS\_RX\_HO\_DATA**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_ho_data)  `2`<br>_The hand-over took the data-template estimator's._  |
| define  | [**ASYNC\_DSSS\_RX\_HO\_NONE**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_ho_none)  `0`<br>_Where the last hand-over's Doppler came from (".ho\_src" and_ [_**dp\_async\_dsss\_receiver\_get\_handover\_source()**_](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_get_handover_source) _): none since the last seed (searching or refining), the seed's own (the refine gave up, or its estimate failed the hand-over's checks), or the data-template estimator's (§4.7)._ |
| define  | [**ASYNC\_DSSS\_RX\_HO\_SEED**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_ho_seed)  `1`<br>_The hand-over took the seed's own Doppler._  |
| define  | [**ASYNC\_DSSS\_RX\_LOCK\_DOWN**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_lock_down)  `0.3`<br> |
| define  | [**ASYNC\_DSSS\_RX\_LOCK\_DWELL**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_lock_dwell)  `30u`<br> |
| define  | [**ASYNC\_DSSS\_RX\_LOCK\_N\_DOWN**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_lock_n_down)  `15u`<br> |
| define  | [**ASYNC\_DSSS\_RX\_LOCK\_N\_UP**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_lock_n_up)  `30u`<br> |
| define  | [**ASYNC\_DSSS\_RX\_LOCK\_UP**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_lock_up)  `0.5`<br> |
| define  | [**ASYNC\_DSSS\_RX\_MAX\_EXCLUSIONS**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_max_exclusions)  `64u`<br> |
| define  | [**ASYNC\_DSSS\_RX\_MAX\_PEAKS**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_max_peaks)  `16u`<br> |
| define  | [**ASYNC\_DSSS\_RX\_NO\_REFINE\_MIN\_BINS**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_no_refine_min_bins) (bn) `((size\_t)(1.0 / (bn) + 0.5))`<br> |
| define  | [**ASYNC\_DSSS\_RX\_REFINE\_DLL\_SPACING**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_refine_dll_spacing)  `0.5`<br> |
| define  | [**ASYNC\_DSSS\_RX\_REFINE\_MIN\_BLOCKS**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_refine_min_blocks)  `7u`<br> |

## Detailed Description


Unlike `DsssReceiver`, which goes straight from an acquisition hit to tracking on the hit's coarse Doppler estimate, this object refines the estimate between the two, closing the low-Es/N0 pull-in gap a coarse-only estimate leaves at large static Doppler offsets. Its design of record is docs/design/async-dsss-receiver.md: the receiver (§1-§4) and its place in an AsyncDsssPool (§5-§9). The despreader, the states and the release under it are docs/design/async-dsss-receiver.md. Its states are the `ASYNC_DSSS_RX_*` values below, also `status().state`:



* **searching** (the start): samples feed the embedded continuous search (the `Correlator` flavor). A hit is made into a seed by `dp_correlator_build_handoff()` and taken through `dp_async_dsss_receiver_seed()`, and the unconsumed tail of the same call goes straight on. The refine chain is a FROZEN carrier wipe (`costas_wipeoff()` at the coarse estimate, `costas_update()` never called) feeding a collection `Dll` whose `dp_dll_lookback_segments(refine_max_error_db)` windows oversample each epoch  the asynchronous data's residual carrier rides a ~symbol\_rate-wide spectrum a single per-epoch dump would alias (see `refine_max_error_db` on `dp_async_dsss_receiver_create()`)  then a `RateConverter` to `CarrierAcquisition`'s rate, then `CarrierAcquisition` itself.
* **refining**: samples feed the refine chain until `CarrierAcquisition` is ready or gives up; then the live tracking chain is built FRESH, from the hand-off's chip phase advanced by the clock dilation over the refine (not wherever the refine-stage `Dll` drifted) and the refined  on a give-up, the seed's  Doppler. It starts at the refine's estimate of where it stopped, `ceil(k * period)` samples from the hand-off (k whole code periods, the fraction carried in its phase), on the same input sample at any chunking (doppler#2974). The refine decides up to its chain's lag past that estimate, so the receiver keeps its last refine samples in a ring sized at create for that lag (`refine_hist_cap`, from [**dp\_RateConverter\_get\_count\_lag()**](RateConverter__core_8h.md#function-dp_rateconverter_get_count_lag)), and the start is read from it when it is behind the call. A start the input has not reached yet is waited for, the receiver still refining. A refine that decides further past than the ring holds (a refine Dll drifting on noise; no signal geometry measured) starts at the first such point the ring still holds, still the same at any chunking. The ring is held for the receiver's lifetime  up to ~38 KiB (Gold-1023 at 2 samples a chip)  and carried in every blob, a pool member's included.
* **tracking**: the pre-despread carrier loop (`carrier_nda_wipeoff`/ `carrier_nda_update`) -&gt; `Dll` -&gt; `RateConverter` -&gt; `MpskReceiver`. The carrier loop is carrier\_nda at M = 2. It updates once a code period on the Dll's despread window sum (`adr_track_period()`): the look-back's window does not span a data transition, and its M = 2 error `Im(w^2)` removes the data with no sign decision. The error is divided by the window's signal power (the unit denominator: the averaged `|w|^2` less the Dll's window noise), and the lock is the ratio of sums of `I^2 - Q^2` over `I^2 + Q^2` on blocks sized by det\_ratio\_block\_len() (docs/design/async-dsss-receiver.md, "The
    loop's lock statistic"). It removes the coupled Doppler (offset and 500 Hz/s ramp) before despreading, so `MpskReceiver` is left a small residual. A pure PLL: no FLL.
* **lost**: the emitter is gone. Without `reacquire`, entered from tracking when BOTH lock flags have been down, without a break, for longer than `lost_confirm_s` (docs/design/async-dsss-receiver.md §10, §11.2); the loops stop and samples are discarded until `reset()`. `lost_confirm_s = 0` (the default) never enters it. With `reacquire` the release is symbol lock alone and returns the receiver to searching instead, so it never reads lost (docs/design/async-dsss-receiver.md §10). While the confirm interval runs the loops HOLD what they settled on with both flags up rather than run on noise ([**dp\_dll\_set\_coast()**](dll__core_8h.md#function-dp_dll_set_coast); docs/design/async-dsss-receiver.md §10; docs/design/async-dsss-receiver-measurements.md §12.19, doppler#1271), so a departed emitter's receiver stands where the emitter left it, and neither sweeps onto a neighbour's code nor restarts its release clock on one it followed. One flag down is a degrade: reported, and the loops run, which is what rides out two live emitters crossing each other's code phase.




`dp_async_dsss_receiver_seed()` hands a searching receiver a detection from outside: the record its own hit would have produced (a hit is a seed the object makes for itself). It refuses on a receiver that already holds one, so a hold is never silently overwritten.


The refine and track stages share ONE carrier-wipe scratch/carry buffer set (`car_wiped_buf`/`car_carry_buf`/`car_carry_len`, sized `tsamps = code_len*spc`); they never run concurrently. The carry holds 0..tsamps-1 live samples (a full period is processed at once, and set\_state refuses tsamps); the blob packs its fixed tsamps region last, the live samples then zeros.



```C++
// 3 Mcps, 2,100 sym/s, BPSK at 2 samples a chip; a one-epoch search.
const double chip_rate           = 3.0e6;
const double symbol_rate         = 2100.0;
const double sample_rate         = 6.0e6;  // 2 samples a chip
const int    m                   = 2;      // BPSK
const double cn0_dbhz            = 55.0;
const double pfa                 = 1e-3;
const double pd                  = 0.9;
const double doppler_uncertainty = 100.0;  // Hz, one-sided
const size_t segments = 4, sps = 8;
const int    differential        = 0;
const double refine_max_error_db = 0.5;    // the refine_* tuning
const size_t refine_sps = 4, refine_n_fft = 64, refine_zero_pad = 8;
const double refine_margin_db    = 14.0;
const bool   refine_sequential   = false;
const size_t refine_max_blocks   = 100000;
const double carrier_freq_hz     = 0.0;    // 0 = aiding off
const double lost_confirm_s      = 0.0;    // 0 = never lost
const bool   reacquire           = false;  // lost waits for reset()
const double code_only_s         = 0.0;    // no window: D = 1
const double doppler_rate        = 0.0;
const bool   refine              = true;   // D = 1 needs the refine

dp_async_dsss_receiver_state_t *rx = dp_async_dsss_receiver_create (
    code, code_len, chip_rate, symbol_rate, sample_rate, m, cn0_dbhz, pfa,
    pd, doppler_uncertainty, segments, sps, differential,
    refine_max_error_db, refine_sps, refine_margin_db, refine_n_fft,
    refine_zero_pad, refine_sequential, refine_max_blocks, carrier_freq_hz,
    lost_confirm_s, reacquire, code_only_s, doppler_rate, refine);
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
    ASYNC_DSSS_RX_IDLE = 3,
    ASYNC_DSSS_RX_LOST = 4
};
```



The state machine's values (`state` below, also the first byte of a serialized blob's extra record). Searching -&gt; refining -&gt; tracking, and seed() puts a searching receiver at refining; lost is reached from tracking by the release rule and left only by reset(). With reacquire the release skips lost and goes straight back to searching. 


        

<hr>
## Public Functions Documentation




### function dp\_async\_dsss\_receiver\_carrier\_bn 

_The pre-despread carrier loop's bandwidth at a design C/N0: the owner's loop-SNR rule (async-dsss-receiver.md §1.3), no wider than ASYNC\_DSSS\_RX\_BN\_CARRIER._ 
```C++
double dp_async_dsss_receiver_carrier_bn (
    double cn0_dbhz,
    double chip_rate,
    size_t code_len
) 
```



The loop updates once a code period, `T = code_len / chip_rate`, on a coherent sum whose SNR is `rho = C/N0 T`. Its loop SNR, counting the M = 2 detector's squaring loss `S_L = 1 / (1 + 1/(2 rho))`, is `rho S_L / (2 bn)`, so ASYNC\_DSSS\_RX\_CARRIER\_LOOP\_SNR\_DB of it asks `bn <= rho S_L / 200`. The bandwidth is that, or ASYNC\_DSSS\_RX\_BN\_CARRIER where the rule allows more: the rule is a ceiling, and the release's 0.04 is the widest the loop has been designed at. At the 415.1-B-1 floor (C/N0 38.77 dB-Hz) it is 0.00993 at 2.2 GHz and 0.00744 at 2.8 GHz. create() sizes the receiver's carrier loop with it, at `cn0_dbhz`.




**Parameters:**


* `cn0_dbhz` Design C/N0, dB-Hz. 
* `chip_rate` Chips per second, &gt; 0. 
* `code_len` Chips a code period, &gt; 0. 



**Returns:**

The bandwidth, per update. 
```C++
#include "doppler/async_dsss_receiver/async_dsss_receiver_core.h"
#include <stdio.h>
int main (void)
{
  double chip = 2.2e9 * 31.0 / 21216.0;  // 415.1-B-1 at 2.2 GHz
  printf ("%.4f\n", dp_async_dsss_receiver_carrier_bn (38.77, chip, 1023));
  printf ("%.2f\n", dp_async_dsss_receiver_carrier_bn (70.0, chip, 1023));
  return 0;  // prints 0.0099 then 0.04
}
```
 





        

<hr>



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
* `n` MpskReceiver's terminal outputs per symbol (`m_out`): inside [**dp\_ratesync\_m\_ok()**](ratesync__core_8h.md#function-dp_ratesync_m_ok) (even, 2..8) and at most `sps`  [**mpsk\_rx\_m\_out\_ok()**](mpsk__rx__loops_8h.md#function-mpsk_rx_m_out_ok), the rule create()'s own derived `n` satisfies, so get\_n() always re-applies (doppler#2650). It need not divide `sps`. 



**Returns:**

0 on success, -1 on a grid refused as above. 
```C++
>>> import numpy as np
>>> from doppler.dsss import AsyncDsssReceiver
>>> from doppler.wfm import Gold
>>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
>>> rx = AsyncDsssReceiver(code, chip_rate=3.069e6, symbol_rate=2700.0,
...                        sample_rate=6.138e6, doppler_uncertainty=500.0)
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



The rule this replaces is sized when the chain is built, from rates the application chooses, by the detection module and not here: the non-coherent count by [**dp\_det\_n\_noncoh()**](detection__core_8h.md#function-dp_det_n_noncoh) (at the detection and false-alarm probabilities, and the design C/N0) and the drop count by [**dp\_det\_verify\_count()**](detection__core_8h.md#function-dp_det_verify_count) (at its miss probability and false-drop budget), handed to [**dp\_dll\_configure\_lock()**](dll__core_8h.md#function-dp_dll_configure_lock). The values are set in adr\_build\_track\_chain() (async\_dsss\_receiver\_core.c); this header does not restate them.




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
...                        sample_rate=6.138e6, doppler_uncertainty=500.0)
>>> rx.configure_lock_raw(up_thresh=0.4, down_thresh=0.2, n_looks=20,
...                       alpha=0.1, n_up=5, n_down=3)
>>> rx.tracking                       # replaced at the next chain build
0
```
 




        

<hr>



### function dp\_async\_dsss\_receiver\_configure\_search\_raw 

_Pin the embedded search's grid directly. Forwards to_ `dp_correlator_configure_search_raw()` __ _the escape hatch under this object's_`symbol_rate` _-driven auto-sizing, for a power user who wants a specific_`(doppler_bins, n_noncoh)` _. The pin replaces the whole auto-sized grid, the window-tiled search included. Only meaningful while searching; the search does not run again until the next_`reset()` _._
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

0 on success; DP\_ERR\_INVALID on an invalid grid, DP\_ERR\_MEMORY on a failed allocation (see dp\_correlator\_configure\_search\_raw). 
```C++
>>> import numpy as np
>>> from doppler.dsss import AsyncDsssReceiver
>>> from doppler.wfm import Gold
>>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
>>> rx = AsyncDsssReceiver(code, chip_rate=3.069e6, symbol_rate=2700.0,
...                        sample_rate=6.138e6, doppler_uncertainty=500.0)
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
    double sample_rate,
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
    double doppler_rate,
    bool refine
) 
```



Only `code`/`chip_rate`/`symbol_rate` describe the signal itself. The `refine_*` defaults are sized to close the specification's 4-5 dB, 500 Hz/s combined scenario as they are; a power user can override each one.




**Parameters:**


* `code` Spreading code, one 0/1 chip per element (0 -&gt; +1, 1 -&gt; -1 BPSK; only the low bit is used, so pass 0/1, not +/-1). 
* `code_len` Chips in `code`. 
* `chip_rate` Chip rate, Hz. 
* `symbol_rate` Data-symbol rate, Hz: finite, &gt; 0, at most `chip_rate`, and at most 2^20 Dll partials per symbol at `segments`. 
* `sample_rate` The rate of the samples pushed, Hz: any ratio to `chip_rate` of at least one, or 0 for two samples a chip (the default). Refine and tracking run at this rate. The search runs at it too when one code period is a whole number of samples ([**dp\_correlator\_search\_period()**](correlator__core_8h.md#function-dp_correlator_search_period)); a fractional period is resampled, for the search alone, to a power of two. 
* `m` PSK order, 2/4/8; default 2 (BPSK). 
* `cn0_dbhz` Design C/N0, dB-Hz; default 55.0  feeds BOTH the embedded search's own sizing AND (derated by `refine_design_margin_db`) CarrierAcquisition's `design_snr`. 
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
* `refine_sequential` CarrierAcquisition's sequential mode; default false  its early per-block test fires on far too little averaging at the specification's Es/N0 floor (as few as 4 blocks, 150-200+ Hz off), so false waits the full design\_snr-derived dwell. 
* `refine_max_n_blocks` CarrierAcquisition's give-up cap in sequential mode, and the cap on the dwell in either mode; default 100000. 
* `carrier_freq_hz` Nominal RF carrier frequency, Hz, enabling carrier-&gt;code aiding; 0.0 (default) = off. When &gt; 0, the coupled code-rate Doppler (carrier\_offset/carrier\_freq) is fed to the tracking Dll via [**dp\_dll\_set\_rate\_aid()**](dll__core_8h.md#function-dp_dll_set_rate_aid) so the code loop rides a dilated clock the discriminator alone can't pull in at low SNR. Set to the receiver's own downlink RF frequency for a physically-coupled Doppler capture. Coupled, it is above half the sample rate ([**dp\_correlator\_carrier\_freq\_ok()**](correlator__core_8h.md#function-dp_correlator_carrier_freq_ok)). 
* `lost_confirm_s` Release rule: both lock flags down, continuously, for longer than this many seconds puts the receiver in the lost state (see get\_lost()). Size it past the longest fade the link must ride. The clock also runs from the first tracking sample, when neither flag is up yet, so a hand-off that never locks within the interval is released the same way as an emitter that leaves. Default 0.0 = never  the exit is then reset(). 
* `reacquire` Release on symbol lock alone, and search again by itself. With it set, the release clock counts samples of tracking with the SYMBOL lock flag down (code lock is not consulted), and when it passes lost\_confirm\_s the receiver resets itself to searching instead of entering lost: it never waits for reset(). A pool of receivers that are always online uses it. The clock runs from the first tracking sample, as above, so a track that never locks (a noise false alarm) is released too; size lost\_confirm\_s past the pull-in time to symbol lock, or a slow lock is released before it comes. Requires a lost\_confirm\_s of at least one input sample, since a release that never fires would never search again; a blob in the lost state is refused by set\_state(). Default false: the release rule above. 
* `code_only_s` Seconds of code-only signal the waveform holds at any chip phase (a symbol-free window; design async-dsss-receiver.md §2.3), &gt;= 0 and finite. The receiver hands its searcher `code_only_epochs = floor(code_only_s * chip_rate / code_len) - 1` (at least 1), and the searcher's slow-time depth follows: `coherent_bins = D`, at most `(code_only_epochs + 1)/2`, rounded down to a 7-smooth length ([**dp\_correlator\_create\_continuous()**](correlator__core_8h.md#function-dp_correlator_create_continuous)). Default 0: no window, D = 1. 
* `doppler_rate` Doppler rate in Hz/s, &gt;= 0 and finite, that the slow-time depth is bounded against: at most `f_epoch / sqrt(2 * doppler_rate)` epochs ([**dp\_correlator\_create\_continuous()**](correlator__core_8h.md#function-dp_correlator_create_continuous)). Default 0: the window is the only bound. 
* `refine` Whether a seed refines its Doppler before tracking. True (the default): a seed enters refining, CarrierAcquisition estimates the residual over its dwell, and tracking starts from the refined Doppler (the `refine_*` parameters). False: a seed builds the tracking chain at once, at the seed's own sample, from the seed's code phase and Doppler; the receiver goes from searching (or idle) to tracking and never reads refining, and the `refine_*` parameters are unused. The Doppler handed over is the search's: the block's mean, within half a Doppler row of it, so within half a row plus `doppler_rate` times half the dwell (`coherent_bins` code periods) of the Doppler at the hand-over sample. False holds only while half the search's Doppler row, `chip_rate / (code_len * coherent_bins)`, is inside the carrier loop's acquisition bound, ASYNC\_DSSS\_RX\_CARRIER\_PULLIN\_HZ: that is `coherent_bins >=  ASYNC_DSSS_RX_NO_REFINE_MIN_BINS(bn)`, with `bn` the receiver's carrier bandwidth ([**dp\_async\_dsss\_receiver\_carrier\_bn()**](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_carrier_bn)): 25 at 0.04, at any chip rate. A receiver configured outside it (a one-epoch search, D = 1, among them) is refused, and a blob in the refining state is refused by set\_state(). The name is a placeholder (async-dsss-receiver.md §4.4). 



**Returns:**

A new receiver, searching; NULL on an invalid argument, including reacquire with a lost\_confirm\_s under one sample, refine false with coherent\_bins under 25, and a symbol shorter than a code period (`symbol_rate * code_len > chip_rate`): the pre-despread carrier loop forms its error, once a code period, from one coherent sum over the code-period window the Dll's look-back picks ([**dp\_dll\_get\_window\_sum()**](dll__core_8h.md#function-dp_dll_get_window_sum)), which is free of data transitions only while a symbol spans a code period (async-dsss-receiver.md §4.4). 
```C++
>>> import numpy as np
>>> from doppler.dsss import AsyncDsssReceiver
>>> from doppler.wfm import Gold
>>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
>>> rx = AsyncDsssReceiver(code, chip_rate=3.069e6, symbol_rate=2700.0,
...                        sample_rate=6.138e6, doppler_uncertainty=500.0)
>>> (rx.tracking, rx.refining, rx.lost)       # searching
(0, 0, 0)

With `refine` false a seed goes straight to tracking; that needs a
search at least 25 code periods deep, here a 0.1 s code-only window:

>>> fast = AsyncDsssReceiver(code, chip_rate=3.069e6, symbol_rate=2700.0,
...                          sample_rate=6.138e6, doppler_uncertainty=500.0,
...                          code_only_s=0.1, refine=False)
>>> fast.coherent_bins
150
>>> fast.seed(chip_phase=512.25, doppler_hz_est=-150.0, cn0_dbhz_est=48.0)
>>> (fast.tracking, fast.refining, fast.doppler_hz)
(1, 0, -150.0)
>>> try:                                       # one epoch deep: refused
...     AsyncDsssReceiver(code, chip_rate=3.069e6, symbol_rate=2700.0,
...                       sample_rate=6.138e6, refine=False)
... except ValueError:
...     print("refused")
refused
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

_The pre-despread carrier loop's last phase error (rad): the residual carrier phase LOOP 1 (which de-rotates before the Dll) is not nulling, as its loop filter takes it_  _carrier\_nda's discriminator divided by its slope (_[_**carrier\_nda\_loop\_error()**_](carrier__nda__core_8h.md#function-carrier_nda_loop_error) _), so radians near lock. The ".car.e" probe is the raw discriminator, twice this._
```C++
double dp_async_dsss_receiver_get_car_last_error (
    const dp_async_dsss_receiver_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Phase error, radians. 





        

<hr>



### function dp\_async\_dsss\_receiver\_get\_car\_lock\_rule 

_The carrier loop's lock rule; see_ [_**dp\_async\_dsss\_receiver\_get\_sym\_lock\_rule()**_](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_get_sym_lock_rule) _._
```C++
dp_lockdet_rule_t dp_async_dsss_receiver_get_car_lock_rule (
    const dp_async_dsss_receiver_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

The rule. 





        

<hr>



### function dp\_async\_dsss\_receiver\_get\_car\_nco\_freq 

_LOOP 1 (the pre-despread carrier loop) output = NCO frequency command, cycles/sample of the front-end (chip\_rate\*spc) rate._ 
```C++
double dp_async_dsss_receiver_get_car_nco_freq (
    const dp_async_dsss_receiver_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

NCO command, cycles/sample at chip\_rate \* spc. 





        

<hr>



### function dp\_async\_dsss\_receiver\_get\_chip\_phase 

_The code phase, chips, at the sample after the last one fed, in the Dll's instantaneous convention_  _the mirror of dp\_correlator\_hit\_t::code\_phase's correlation-lag convention (_[_**dp\_correlator\_build\_handoff()**_](correlator__core_8h.md#function-dp_correlator_build_handoff) _). Tracking, it is the live Dll's phase carried over the partial period the chain holds (it runs whole periods), so it names the same sample a search's picks are carried to at any sample rate (#3063). While refining there is no live Dll: it is the seed's phase advanced by the clock dilation since, the phase the hand-over starts the live chain from._
```C++
double dp_async_dsss_receiver_get_chip_phase (
    const dp_async_dsss_receiver_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Code phase, chips. 





        

<hr>



### function dp\_async\_dsss\_receiver\_get\_cn0\_dbhz\_est 

_The seed's C/N0 estimate, dB-Hz, unchanged until reset()._ 
```C++
double dp_async_dsss_receiver_get_cn0_dbhz_est (
    const dp_async_dsss_receiver_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

C/N0, dB-Hz; 0 after create and reset(). 





        

<hr>



### function dp\_async\_dsss\_receiver\_get\_code\_lock\_rule 

_The code loop's lock rule; see_ [_**dp\_async\_dsss\_receiver\_get\_sym\_lock\_rule()**_](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_get_sym_lock_rule) _._
```C++
dp_lockdet_rule_t dp_async_dsss_receiver_get_code_lock_rule (
    const dp_async_dsss_receiver_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

The rule. 





        

<hr>



### function dp\_async\_dsss\_receiver\_get\_code\_locked 

_Binary code-lock flag from the live tracking Dll's own verify-counted (pfa-tuned) lock detector — the fundamental DSSS "am I
despreading" lock, de-chattered by up/down hysteresis._ 
```C++
int dp_async_dsss_receiver_get_code_locked (
    const dp_async_dsss_receiver_state_t * state
) 
```



This is the flag 415.1-B-1 §7.4.1's "short-code PN lock" names: its acquisition requirement, PN lock with probability at least 0.9 within 20 s of the signal's arrival, anywhere in +/-1,500 Hz of best lock, is read on it (validate\_async\_dsss\_receiver\_pullin, whose cells assert the requirement is not refuted at 95%). 

**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

1 while code lock is declared, else 0. 





        

<hr>



### function dp\_async\_dsss\_receiver\_get\_code\_rate 

_The Dll's code rate, chips per nominal chip (~1.0 at lock), without the carrier aid; a coasting Dll reports its held rate._ 
```C++
double dp_async_dsss_receiver_get_code_rate (
    const dp_async_dsss_receiver_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Code rate, chips per nominal chip. 





        

<hr>



### function dp\_async\_dsss\_receiver\_get\_coherent\_bins 

_The searcher's slow-time depth D: epochs per coherent block._ 
```C++
size_t dp_async_dsss_receiver_get_coherent_bins (
    const dp_async_dsss_receiver_state_t * state
) 
```



Derived at create from `code_only_s` and `doppler_rate` ([**dp\_async\_dsss\_receiver\_create()**](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_create)); 1 without a code-only window. Every restarted search  reset(), a release with `reacquire`, a refused hand-over, configure\_search\_raw()  starts its next dwell on a multiple of `D * code_len * spc` input samples counted from create: the receiver discards input up to that boundary first, at most one block. Receivers fed the same stream from create therefore search on the same block grid.




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
...                        sample_rate=6.138e6, doppler_uncertainty=500.0)
>>> rx.coherent_bins                  # no code-only window
1
>>> rx = AsyncDsssReceiver(code, chip_rate=3.069e6, symbol_rate=2700.0,
...                        sample_rate=6.138e6, doppler_uncertainty=500.0,
...                        code_only_s=0.0627)
>>> rx.coherent_bins                  # 187 epochs: 94, run 7-smooth
90
```
 





        

<hr>



### function dp\_async\_dsss\_receiver\_get\_demod\_lock\_rule 

_The demodulator's carrier lock rule; see_ [_**dp\_async\_dsss\_receiver\_get\_sym\_lock\_rule()**_](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_get_sym_lock_rule) _._
```C++
dp_lockdet_rule_t dp_async_dsss_receiver_get_demod_lock_rule (
    const dp_async_dsss_receiver_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

The rule. 





        

<hr>



### function dp\_async\_dsss\_receiver\_get\_demod\_sync\_lock\_rule 

_The demodulator's timing lock rule; see_ [_**dp\_async\_dsss\_receiver\_get\_sym\_lock\_rule()**_](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_get_sym_lock_rule) _._
```C++
dp_lockdet_rule_t dp_async_dsss_receiver_get_demod_sync_lock_rule (
    const dp_async_dsss_receiver_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

The rule. 





        

<hr>



### function dp\_async\_dsss\_receiver\_get\_doppler\_hz 

_The HAND-OVER's Doppler, Hz: what the live chain was built from, frozen for the track's whole tenure_  _the seed's while refining, the refine's estimate from the hand-over on. It does not follow the carrier: under a 400 Hz/s ramp it is 400 Hz stale a second later. Where the emitter is now is status().doppler\_hz, a field of the same name with the other meaning._
```C++
double dp_async_dsss_receiver_get_doppler_hz (
    const dp_async_dsss_receiver_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Doppler, Hz; 0 after create, reset() or a refused hand-over. 





        

<hr>



### function dp\_async\_dsss\_receiver\_get\_handover\_source 

_Which estimate the live chain's Doppler came from: ASYNC\_DSSS\_RX\_HO\_NONE until a hand-over, then \_SEED or \_DATA (the refine's choice, §4.7), NONE again at the next seed._ 
```C++
int dp_async_dsss_receiver_get_handover_source (
    const dp_async_dsss_receiver_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

One of the ASYNC\_DSSS\_RX\_HO\_\* values. 





        

<hr>



### function dp\_async\_dsss\_receiver\_get\_lock 

_The carrier lock statistic of the MpskReceiver this receiver tracks with: the EMA of its M-th-power lock signal, near 1 when locked and near 0 on noise._ 
```C++
double dp_async_dsss_receiver_get_lock (
    const dp_async_dsss_receiver_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

The MpskReceiver's lock statistic. 





        

<hr>



### function dp\_async\_dsss\_receiver\_get\_lock\_metric 

_Symbol-lock metric = SNR-weighted EMA of (I^2-Q^2)/(I^2+Q^2) = cos(2\*phi) over the emitted symbols (locked -&gt; ~+1). Drives_ `locked` _._
```C++
double dp_async_dsss_receiver_get_lock_metric (
    const dp_async_dsss_receiver_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

The metric, in -1..+1; reset when the track chain is built. 





        

<hr>



### function dp\_async\_dsss\_receiver\_get\_lock\_threshold 

_The lock-metric threshold_ `locked` _is declared strictly above (0.5, ASYNC\_DSSS\_RX\_LOCK\_UP); exposed alongside lock\_metric for engineering debug._
```C++
double dp_async_dsss_receiver_get_lock_threshold (
    const dp_async_dsss_receiver_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

The up threshold, 0.5. 





        

<hr>



### function dp\_async\_dsss\_receiver\_get\_locked 

_The receiver's symbol-lock flag: a hysteretic detector over get\_lock\_metric(), declared after 30 consecutive symbols strictly above get\_lock\_threshold() (0.5) and dropped after 15 consecutive symbols strictly below 0.3 (ASYNC\_DSSS\_RX\_LOCK\_N\_UP, \_UP, \_N\_DOWN and \_DOWN, fixed at create). The flag the release rule and a pool's holder act on._ 
```C++
int dp_async_dsss_receiver_get_locked (
    const dp_async_dsss_receiver_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

1 while symbol lock is declared, else 0. 





        

<hr>



### function dp\_async\_dsss\_receiver\_get\_lost 

_1 once the release rule has fired: both lock flags were down, continuously, for longer than_ `lost_confirm_s` _while tracking_ _an emitter that left, or a seed that never locked. The loops have stopped and samples are consumed and discarded; the holder releases the assignment and calls reset(). 0 in every other state, and always 0 with_`lost_confirm_s = 0` _._
```C++
int dp_async_dsss_receiver_get_lost (
    const dp_async_dsss_receiver_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

1 in the lost state, else 0. 





        

<hr>



### function dp\_async\_dsss\_receiver\_get\_mpsk\_last\_error 

_MpskReceiver carrier phase discriminator (rad): the residual carrier phase LOOP 2 (post-despread) is not nulling._ 
```C++
double dp_async_dsss_receiver_get_mpsk_last_error (
    const dp_async_dsss_receiver_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Phase error, radians. 





        

<hr>



### function dp\_async\_dsss\_receiver\_get\_n 

_MpskReceiver's terminal outputs per symbol (even, 2..8)._ 
```C++
int dp_async_dsss_receiver_get_n (
    const dp_async_dsss_receiver_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Outputs per symbol; set at create or by configure\_chain\_raw(). 





        

<hr>



### function dp\_async\_dsss\_receiver\_get\_n\_exclusions 

_Code phases set\_exclusions() holds._ 
```C++
size_t dp_async_dsss_receiver_get_n_exclusions (
    const dp_async_dsss_receiver_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Count, 0 to ASYNC\_DSSS\_RX\_MAX\_EXCLUSIONS. 





        

<hr>



### function dp\_async\_dsss\_receiver\_get\_nco\_freq 

_The MpskReceiver's carrier NCO command, its loop filter's whole output, cycles/sample at its input rate (_ `sps * symbol_rate` _). Its mean tracks a Doppler ramp with no lag (unlike get\_norm\_freq()); its variance is the carrier loop's stress._
```C++
double dp_async_dsss_receiver_get_nco_freq (
    const dp_async_dsss_receiver_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

NCO command, cycles/sample at sps \* symbol\_rate. 





        

<hr>



### function dp\_async\_dsss\_receiver\_get\_norm\_freq 

_The post-despread carrier the MpskReceiver tracks: its centre plus its loop's integrator, cycles/sample at its input rate (_ `sps * symbol_rate` _). Lags a Doppler ramp by the Type-II ramp error._
```C++
double dp_async_dsss_receiver_get_norm_freq (
    const dp_async_dsss_receiver_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Carrier frequency, cycles/sample at sps \* symbol\_rate. 





        

<hr>



### function dp\_async\_dsss\_receiver\_get\_rank 

_Which surviving peak of a dwell the search takes; 0 = the strongest. Set with_ [_**dp\_async\_dsss\_receiver\_set\_rank()**_](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_set_rank) _._
```C++
size_t dp_async_dsss_receiver_get_rank (
    const dp_async_dsss_receiver_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

The rank, 0 to ASYNC\_DSSS\_RX\_MAX\_PEAKS - 1. 





        

<hr>



### function dp\_async\_dsss\_receiver\_get\_refining 

_1 while refining: the CarrierAcquisition dwell; 0 in every other state._ 
```C++
int dp_async_dsss_receiver_get_refining (
    const dp_async_dsss_receiver_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

1 in the refining state, else 0. 





        

<hr>



### function dp\_async\_dsss\_receiver\_get\_segments 

_The live-tracking Dll's partial correlations per code period._ 
```C++
size_t dp_async_dsss_receiver_get_segments (
    const dp_async_dsss_receiver_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Segments; set at create or by configure\_chain\_raw(). 





        

<hr>



### function dp\_async\_dsss\_receiver\_get\_sps 

_MpskReceiver's samples per symbol._ 
```C++
size_t dp_async_dsss_receiver_get_sps (
    const dp_async_dsss_receiver_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Samples per symbol; set at create or by configure\_chain\_raw(). 





        

<hr>



### function dp\_async\_dsss\_receiver\_get\_state 

_Serialize the receiver: its own fields, then each child, then the carrier carry (live samples, zero-padded to tsamps)._ 
```C++
void dp_async_dsss_receiver_get_state (
    const dp_async_dsss_receiver_state_t * state,
    void * blob
) 
```



Scratch buffers, the telemetry attachment and the refine\_periods diagnostic are not carried.




**Parameters:**


* `state` Must be non-NULL. 
* `blob` Caller-owned buffer of at least [**dp\_async\_dsss\_receiver\_state\_bytes()**](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_state_bytes) bytes. 




        

<hr>



### function dp\_async\_dsss\_receiver\_get\_sym\_lock\_rule 

_The rules behind the receiver's five lock decisions, each its levels and verify counts (_ [_**dp\_lockdet\_rule\_t**_](structdp__lockdet__rule__t.md) _), for a caller that draws a lock statistic beside the line it is decided against._
```C++
dp_lockdet_rule_t dp_async_dsss_receiver_get_sym_lock_rule (
    const dp_async_dsss_receiver_state_t * state
) 
```




* sym: the symbol-lock decision (`locked`) on the lock metric, always ASYNC\_DSSS\_RX\_LOCK\_UP / \_DOWN / \_N\_UP / \_N\_DOWN; telemetry ".sym.lock";
* car: the pre-despread carrier loop's decision on its lock metric ([**dp\_carrier\_nda\_get\_lock\_rule()**](carrier__nda__core_8h.md#function-dp_carrier_nda_get_lock_rule)); ".car.lock";
* code: the Dll's decision on its statistic R ([**dp\_dll\_get\_lock\_rule()**](dll__core_8h.md#function-dp_dll_get_lock_rule)), as configure\_lock\_raw() last retuned it on the Dll held now; ".code.lock";
* demod, demod\_sync: the demodulator's carrier and timing decisions ([**dp\_mpsk\_receiver\_get\_lock\_rule()**](mpsk__receiver__core_8h.md#function-dp_mpsk_receiver_get_lock_rule), \_get\_sync\_lock\_rule()); ".demod.lock" and ".demod.sync.lock".




The tracking chain is rebuilt at every seed, so read them after the hand-over; each rebuild sets the same rules. 

**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

The rule. 





        

<hr>



### function dp\_async\_dsss\_receiver\_get\_threads 

_Workers the searcher runs, the calling thread included; 1 = serial. Set with_ [_**dp\_async\_dsss\_receiver\_set\_threads()**_](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_set_threads) _._
```C++
int dp_async_dsss_receiver_get_threads (
    const dp_async_dsss_receiver_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

The embedded search's thread count, &gt;= 1. 





        

<hr>



### function dp\_async\_dsss\_receiver\_get\_tracking 

_1 once the live tracking chain is built and demodulating; 0 in every other state._ 
```C++
int dp_async_dsss_receiver_get_tracking (
    const dp_async_dsss_receiver_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

1 in the tracking state, else 0. 





        

<hr>



### function dp\_async\_dsss\_receiver\_reset 

_Return to the searching state. Resets the embedded search and rebuilds the refine-stage chain and the live tracking chain back to their placeholder seed (phase 0, no Doppler). A receiver that has locked cannot be "reset back to tracking the same signal," only back to searching_  _every object's reset() semantics in this codebase._
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
...                        sample_rate=6.138e6, doppler_uncertainty=500.0)
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



The hand-off of docs/design/async-dsss-receiver.md §2.2: the three numbers a searcher's hit carries that this receiver uses  `dp_correlator_handoff_t`'s `chip_phase`, `doppler_hz_est` and `cn0_dbhz_est`  exactly as its own hit would have produced them (`steps()` calls this on its own hit). `chip_phase` is the code's instantaneous phase in chips, Dll's convention, at the FIRST sample of the next `steps()` call; [**dp\_correlator\_build\_handoff()**](correlator__core_8h.md#function-dp_correlator_build_handoff) makes it from a hit's lag. The refine chain is rebuilt from the seed and the state becomes refining; the unconsumed tail is the caller's to feed.


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
...                        symbol_rate=2700.0, sample_rate=6.138e6)
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



`chip_phase` must be finite and inside `[0, code_len)`, and `doppler_hz_est` must be strictly inside `(-fs/2, fs/2)`, where `fs` is `chip_rate * spc`. A Doppler at or past half the sample rate is not reportable ([**dp\_correlator\_carrier\_freq\_ok()**](correlator__core_8h.md#function-dp_correlator_carrier_freq_ok) relies on this bound), and a seed there sent the hand-off's phase to NaN (doppler#2103).


seed() refuses by this predicate, set\_state() restores the seed fields only inside it, the searching receiver's hit path does not seed from a hit it fails, and the hand-over holds the REFINED Doppler to it too: a seed in range whose residual carries it to or past fs/2 is refused at the hand-over, counted in refused\_hits, and the receiver searches again rather than track a Doppler it cannot report. It reads only the receiver's fixed config (code length, chip rate, spc), so any receiver built from the same config gives the same answer.




**Parameters:**


* `state` Must be non-NULL. 
* `chip_phase` Code phase, chips. 
* `doppler_hz_est` Doppler estimate, Hz. 



**Returns:**

1 inside the seed domain, 0 outside it (a NaN is outside). 





        

<hr>



### function dp\_async\_dsss\_receiver\_set\_exclusions 

_Code phases the search must not take: signals another receiver already tracks._ 
```C++
int dp_async_dsss_receiver_set_exclusions (
    dp_async_dsss_receiver_state_t * state,
    const double * phases,
    size_t phases_len
) 
```



Replaces the list. A peak within one chip, circularly, of any of them is dropped from a dwell's list at any Doppler, before ranking (set\_rank()). Phases are chips in status().chip\_phase's convention, at the sample after the last one fed when they are set, and are folded into [0, code\_len); a dwell decided calls later carries them from that sample to its own. An empty list (the default) excludes nothing. A tracking receiver is not affected: the list applies to its next search. A mutator's value, so it travels in the state blob (#2022).




**Parameters:**


* `state` Must be non-NULL. 
* `phases` Code phases, chips; finite. May be NULL when `phases_len` is 0. 
* `phases_len` At most [**ASYNC\_DSSS\_RX\_MAX\_EXCLUSIONS**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_max_exclusions). 



**Returns:**

`DP_OK`; `DP_ERR_INVALID` (list unchanged) on a non-finite phase or too many. 
```C++
>>> import numpy as np
>>> from doppler.dsss import AsyncDsssReceiver
>>> rx = AsyncDsssReceiver(code=[1, 0, 1, 1, 0, 0, 1], chip_rate=1e6,
...                        symbol_rate=1e6 / 28.0, sample_rate=4e6, cn0_dbhz=60.0)
>>> rx.set_exclusions(np.array([2.5, 9.0]))   # 9 folds to 2
>>> rx.n_exclusions
2
```
 





        

<hr>



### function dp\_async\_dsss\_receiver\_set\_rank 

_Take the_ `rank-th` _surviving peak of a dwell, not the strongest._
```C++
int dp_async_dsss_receiver_set_rank (
    dp_async_dsss_receiver_state_t * state,
    size_t rank
) 
```



The searcher reports up to [**ASYNC\_DSSS\_RX\_MAX\_PEAKS**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_max_peaks) peaks per dwell, strongest first. The receiver drops every peak within one chip (circularly) of a stronger one, so one signal's neighbouring cells hold one place; then every peak a stronger signal's own sidelobes could explain; then every peak within one chip of an excluded code phase (set\_exclusions()). The survivors are ranked from 0. A dwell with no peak at this rank hands nothing off and the search goes on.


The sidelobe rule, per cell: a strong signal's list also holds its code's Doppler-offset sidelobes, up to [**dp\_correlator\_dsl\_db()**](correlator__core_8h.md#function-dp_correlator_dsl_db) below it and hundreds of chips from its phase (design acq-surface.md §8). A peak counts as a second signal only if its test\_stat exceeds what the stronger signals' sidelobes can read AT ITS CELL by more than the detector's own gate ([**dp\_correlator\_gate\_stat()**](correlator__core_8h.md#function-dp_correlator_gate_stat)). Each stronger signal reads its statistic on its own lag ([**dp\_correlator\_hit\_t::test\_stat\_peak**](structdp__correlator__hit__t.md#variable-test_stat_peak)) times the code's cross-ambiguity from it to this cell ([**dp\_correlator\_sidelobe\_ratio()**](correlator__core_8h.md#function-dp_correlator_sidelobe_ratio)), which reaches [**dp\_correlator\_dsl\_db()**](correlator__core_8h.md#function-dp_correlator_dsl_db) only at the worst cells. The bound is the strongest of these; on a deep block (get\_coherent\_bins() &gt; 1) it is the sum over the stronger signals on the peak's own Doppler row, modulo the epoch rate, when that is larger, since their fixed sidelobes can stack on one cell, and each is floored at [**dp\_correlator\_dsl\_db()**](correlator__core_8h.md#function-dp_correlator_dsl_db) until a strong signal's data splatter is modelled (doppler#2730). So the range does not narrow with the number of signals on the air (doppler#2554), and a sidelobe passes only when the noise on it alone would have cleared the gate, the rate the search already runs at. A second signal is taken well past [**dp\_correlator\_dsl\_db()**](correlator__core_8h.md#function-dp_correlator_dsl_db) below a stronger one at most cells: on a one-epoch search, measured, in every stream to 10 dB below and in a fifth of them at 20 dB (design async-dsss-receiver.md §7.2, the near-far limit). The model is one clean code period: when a data transition falls inside the stronger signal's epoch  common, 55% of epochs at the specification's 1.8 epochs a symbol  its sidelobes read far above it at some cells (20.4 dB at one, measured by native/validation/acq\_emitter\_floor.c), and only the gate stands between them and a false second signal. Over the operating range the gate held (the near-far sweep, data on both, took no sidelobe); past it, or on a one-epoch search of a much stronger signal, it is not bounded (doppler#2734). Searchers that share a stream and its block grid (get\_coherent\_bins()) and hold ranks 0..k-1 therefore take k different signals in one dwell. Takes effect at the next dwell. A mutator's value, so it travels in the state blob (#2022).




**Parameters:**


* `state` Must be non-NULL. 
* `rank` 0 (the strongest, the default) to [**ASYNC\_DSSS\_RX\_MAX\_PEAKS**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_max_peaks) - 1. 



**Returns:**

`DP_OK`; `DP_ERR_INVALID` (rank unchanged) past the list. 
```C++
>>> from doppler.dsss import AsyncDsssReceiver
>>> rx = AsyncDsssReceiver(code=[1, 0, 1, 1, 0, 0, 1], chip_rate=1e6,
...                        symbol_rate=1e6 / 28.0, sample_rate=4e6, cn0_dbhz=60.0)
>>> rx.set_rank(1)
>>> rx.rank
1
```
 





        

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
...                        symbol_rate=1e6 / 28.0, sample_rate=4e6, cn0_dbhz=60.0)
>>> rx.refine_min_blocks                     # the default floor
7
>>> rx.set_refine_min_blocks(12)
>>> rx.refine_min_blocks
12
```
 





        

<hr>



### function dp\_async\_dsss\_receiver\_set\_state 

_Restore a blob written by_ [_**dp\_async\_dsss\_receiver\_get\_state()**_](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_get_state) _._
```C++
int dp_async_dsss_receiver_set_state (
    dp_async_dsss_receiver_state_t * state,
    const void * blob
) 
```



The envelope, the layout keys (segments, sps, n, refine\_segments), the state value, the seed, rank, exclusions and thread count are all checked before any child is decoded; a refusal there changes nothing. The children are then restored in order, so a child that rejects its sub-blob leaves the children before it already restored. Restores set\_threads()'s value by calling [**dp\_correlator\_set\_threads()**](correlator__core_8h.md#function-dp_correlator_set_threads).




**Parameters:**


* `state` Must be non-NULL; built with the same configuration. 
* `blob` A blob produced by [**dp\_async\_dsss\_receiver\_get\_state()**](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_get_state). 



**Returns:**

`DP_OK`, or `DP_ERR_INVALID` if the blob is refused. 





        

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
* "&lt;prefix&gt;.state"  where the call left the receiver, the ASYNC\_DSSS\_RX\_\* value: 0 searching, 1 refining, 2 tracking, 3 idle (a pooled receiver holding nothing), 4 lost (status().state);
* "&lt;prefix&gt;.handover\_hz"  the Doppler the chain was built from, Hz ([**dp\_async\_dsss\_receiver\_get\_doppler\_hz()**](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_get_doppler_hz)): 0 before a seed, the seed's while refining, the refined value from the hand-over on, and 0 again after a refused hand-over or a reset(), which return the receiver to searching. A record is a float, so it resolves about 0.5 Hz near 5 MHz. Not the live carrier: status().doppler\_hz reads that, and ".car.freq" and ".car.nco" are the carrier loop's own view of it, in cycles a sample at the input rate, so times chip\_rate \* spc for Hz;
* "&lt;prefix&gt;.refused"  the hits refused as seeds so far (status().refused\_hits);
* "&lt;prefix&gt;.ho\_src"  which estimate the live chain was built from, an ASYNC\_DSSS\_RX\_HO\_\* value (0 none since the seed, 1 the seed's, 2 the data template's);
* "&lt;prefix&gt;.cn0"  the seed's C/N0 estimate, dB-Hz (status().cn0\_dbhz\_est): constant over a tenure, since the receiver keeps no running estimate while tracking;
* "&lt;prefix&gt;.phase.car"  the pre-despread carrier NCO's phase at the call's FIRST sample, cycles in [0, 1) ([**dp\_carrier\_nda\_get\_nco\_phase\_ahead()**](carrier__nda__core_8h.md#function-dp_carrier_nda_get_nco_phase_ahead) past the samples the chain still carries), exact: the loop steers only at a period's end. The phase of the tracking chain; while searching or refining it is the placeholder chain's, and ".state" says which;
* "&lt;prefix&gt;.phase.code"  the code NCO's prompt phase at the call's first sample, chips ([**dp\_dll\_get\_code\_phase\_ahead()**](dll__core_8h.md#function-dp_dll_get_code_phase_ahead)), exact to within a code-loop update inside the carried samples. It is the code NCO's word, the START of a sample's dwell; the Dll takes its prompt at the dwell's CENTRE, half an increment later ([**dll\_tap\_base()**](dll__core_8h.md#function-dll_tap_base), [**nco\_dwell\_center()**](nco__core_8h.md#function-nco_dwell_center): the zero-order-hold rule). So a prompt on the continuous code phase reads here half a sample BEFORE it (0.5 chip\_rate / fs chips), with no correlation lost  measured in the pull-in captures, -0.186 chip at 2.69 samples a chip (0.500 sample) and -0.148 at 3.42 (0.505).




The two phases are stamped at the call's first sample, which is the stamp the caller set (dp\_tlm\_set\_now()) for this block, so a caller who knows the transmitted carrier and code phase at that sample subtracts them for the loops' true errors  the stress the discriminator outputs ".car.e" and ".code.e" only estimate.


Feed blocks of about a code period, as the examples do, and these read as a time series of the lifecycle: when the search hit, how long the refine took, what it handed over, when the release came.


Each loop reports under the prefix, at its own update rate:
* "&lt;prefix&gt;.car"  the chip-domain carrier loop ([**dp\_carrier\_nda\_set\_telemetry()**](carrier__nda__core_8h.md#function-dp_carrier_nda_set_telemetry): ".lock", ".e", ".freq", ".locked", ".nco"; ".e" is the raw discriminator, slope 2, so twice the phase error in radians near lock, and ".lock" the last block's ratio of sums), once per carrier update: once a code period, except while it coasts, when loop 1 is held;
* "&lt;prefix&gt;.code"  the code loop ([**dp\_dll\_set\_telemetry()**](dll__core_8h.md#function-dp_dll_set_telemetry): ".e", ".rate", ".lock", ".locked", ".nco", and the lock statistic's two terms ".prompt\_amp" and ".offset\_pwr"), once per code period (with the symbol aid on, the last steer held);
* "&lt;prefix&gt;.code.steer"  the code loop's filter at each steer of the symbol aid ([**dp\_dll\_set\_steer\_telemetry()**](dll__core_8h.md#function-dp_dll_set_steer_telemetry): ".e", ".prop", ".integ", ".ctrl", ".aid"), the loop's own cadence;
* "&lt;prefix&gt;.demod"  the symbol-domain demodulator ([**dp\_mpsk\_receiver\_set\_telemetry()**](mpsk__receiver__core_8h.md#function-dp_mpsk_receiver_set_telemetry): its lock, its carrier and timing loops, the symbols, its AGC), once per recovered symbol (the AGC's pair once per gain update), and the timing loop's two filter terms beside them, ".demod.sync.prop" and ".demod.sync.integ" ([**dp\_ratesync\_loop\_set\_filter\_telemetry()**](ratesync__core_8h.md#function-dp_ratesync_loop_set_filter_telemetry));
* "&lt;prefix&gt;.acq"  the acquisition engine ([**dp\_correlator\_set\_telemetry()**](correlator__core_8h.md#function-dp_correlator_set_telemetry)), once per decided dwell;
* "&lt;prefix&gt;.rc"  the rate converter between the code loop and the demodulator ([**dp\_RateConverter\_set\_telemetry()**](RateConverter__core_8h.md#function-dp_rateconverter_set_telemetry)). It reports only an AGC, and this receiver builds it without one, so it registers no probes;
* "&lt;prefix&gt;.sym.lock" and "&lt;prefix&gt;.sym.locked"  the receiver's own symbol-lock metric ([**dp\_async\_dsss\_receiver\_get\_lock\_metric()**](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_get_lock_metric)) and its decision ([**dp\_async\_dsss\_receiver\_get\_locked()**](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_get_locked)), the one its users act on, once per emitted symbol; "&lt;prefix&gt;.sym.num" and "&lt;prefix&gt;.sym.den", the metric's two EMAs (their ratio is ".sym.lock"), on the same symbols;
* "&lt;prefix&gt;.ref"  the refine's estimator, the data template's ([**dp\_carrier\_acq\_set\_telemetry()**](carrier__acq__core_8h.md#function-dp_carrier_acq_set_telemetry): ".stat", ".thr", ".hz", ".n", ".ready"), once per tested block while refining.
* "&lt;prefix&gt;.state", "&lt;prefix&gt;.handover\_hz", "&lt;prefix&gt;.refused", "&lt;prefix&gt;.ho\_src", "&lt;prefix&gt;.cn0", "&lt;prefix&gt;.phase.car" and "&lt;prefix&gt;.phase.code"  the lifecycle (an async\_dsss\_rx\_state\_t), the Doppler the chain was built from (Hz), the refused hits, the hand-over's source (ASYNC\_DSSS\_RX\_HO\_\*), the seed's C/N0 (dB-Hz) and the loops' NCO phases (cycles, chips) at the call's first sample, once per steps() call;
* "&lt;prefix&gt;.had\_lock", "&lt;prefix&gt;.coast" and "&lt;prefix&gt;.hold"  the loop control the receiver decides once a code period from the code and symbol flags, each 0 or 1: whether both have been up together since the seed (from then on the loops coast rather than run free), whether this period coasts (both down: the carrier restored to its hold point and frozen, the code loop dead-reckoning  one decision drives both loops), and whether this period re-marked the hold point (both up); once per code period while tracking;
* "&lt;prefix&gt;.release"  the release clock: how long both flags have been down (the symbol flag alone with reacquire), as a fraction of the confirm interval, so the receiver is released once it passes 1; 0 while a flag is up and when there is no release rule. Once per steps() call.




The carrier and code loops report their filters two ways: ".car.freq" and ".code.rate" (and the demodulator's ".demod.car.freq") are the integrators, each loop's frequency memory, which a coast holds; ".car.nco", ".code.nco" and ".demod.car.nco" are the complete commands the NCOs run at, the proportional term (and the code loop's carrier aid) included. ".demod.sync.rate" is the timing loop's output, and ".e" is each discriminator's input to its filter.


The attachment is a REQUEST kept on the receiver: every seed rebuilds the tracking chain, and the rebuilt loops are attached again under the same names (so the same probe ids). Records are therefore continuous across re-seeds, and a reset keeps the attachment too.


Setup path, never hot; the context is borrowed and must outlive the attachment, and the receiver must step on one thread (SPSC rules in [**dp\_tlm/dp\_tlm\_core.h**](dp__tlm__core_8h.md)). Passing NULL detaches every loop.




**Parameters:**


* `state` Must be non-NULL. 
* `tlm` Telemetry context to attach, or NULL to detach. 
* `prefix` Probe-name prefix; NULL means "rx". 
* `decim` Emit every decim-th event on each probe; &gt;= 1. 



**Returns:**

DP\_OK, or DP\_ERR\_INVALID on decim 0, a prefix too long for some probe's name  refused before anything registers on `tlm`, because the loops are first attached to a scratch context that finds the refusal  or a probe table with no room for every probe (found the same way, from the names the scratch holds); DP\_ERR\_MEMORY when the attach cannot allocate, its scratch context or a loop's own (the searcher builds one). Every refusal leaves `tlm's` table untouched and the receiver as it was: detached, or still on its previous context under its previous prefix. 
```C++
>>> import numpy as np
>>> from doppler.dsss import AsyncDsssReceiver
>>> from doppler.telemetry import Telemetry
>>> from doppler.wfm import Gold
>>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
>>> rx = AsyncDsssReceiver(code, chip_rate=3.069e6, symbol_rate=2700.0,
...                        sample_rate=6.138e6, doppler_uncertainty=500.0)
>>> tlm = Telemetry(1 << 12)
>>> rx.set_telemetry(tlm, "rx")
>>> sorted(p for p in tlm.probe_names if p.startswith(("rx.car",
...                                                    "rx.code")))
['rx.car.e', 'rx.car.freq', 'rx.car.lock', 'rx.car.locked', 'rx.car.nco', 'rx.code.e', 'rx.code.lock', 'rx.code.locked', 'rx.code.nco', 'rx.code.offset_pwr', 'rx.code.prompt_amp', 'rx.code.rate', 'rx.code.steer.aid', 'rx.code.steer.ctrl', 'rx.code.steer.e', 'rx.code.steer.integ', 'rx.code.steer.prop']
>>> sorted(p for p in tlm.probe_names if p.startswith(("rx.s", "rx.h",
...                                                    "rx.refu")))
['rx.had_lock', 'rx.handover_hz', 'rx.ho_src', 'rx.hold', 'rx.refused', 'rx.state', 'rx.sym.den', 'rx.sym.lock', 'rx.sym.locked', 'rx.sym.num']
>>> sorted(p for p in tlm.probe_names if p.startswith(("rx.ph", "rx.cn")))
['rx.cn0', 'rx.phase.car', 'rx.phase.code']
>>> sorted(p for p in tlm.probe_names if p.startswith("rx.ref"))
['rx.ref.hz', 'rx.ref.n', 'rx.ref.ready', 'rx.ref.stat', 'rx.ref.thr', 'rx.refused']
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



Forwards to [**dp\_correlator\_set\_threads()**](correlator__core_8h.md#function-dp_correlator_set_threads) on the receiver's searcher. The search is bit-identical at every count, so this changes the cost of a steps() call and nothing about its output. A pool that steps its receivers in parallel sets each one to 1. A mutator's value, so it travels in the state blob (#2022), in one place: the searcher's own blob, as the request, which [**dp\_correlator\_set\_state()**](correlator__core_8h.md#function-dp_correlator_set_state) applies through [**dp\_correlator\_set\_threads()**](correlator__core_8h.md#function-dp_correlator_set_threads). A pool's 1 restores as 1 everywhere; a writer's 0 restores as 0, which the reader runs on its own online cores. Setup path; not while another thread is inside steps().




**Parameters:**


* `state` Must be non-NULL. 
* `n` Thread count; 0 = online cores, 1 = serial. 



**Returns:**

`DP_OK`. The count actually running is [**dp\_async\_dsss\_receiver\_get\_threads()**](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_get_threads); a searcher with one Doppler tile always runs 1. 
```C++
>>> from doppler.dsss import AsyncDsssReceiver
>>> rx = AsyncDsssReceiver(code=[1, 0, 1, 1, 0, 0, 1], chip_rate=1e6,
...                        symbol_rate=1e6 / 28.0, sample_rate=4e6, cn0_dbhz=60.0)
>>> rx.set_threads(1)
>>> rx.threads
1
```
 





        

<hr>



### function dp\_async\_dsss\_receiver\_state\_bytes 

_Size in bytes of the blob_ [_**dp\_async\_dsss\_receiver\_get\_state()**_](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_get_state) _writes._
```C++
size_t dp_async_dsss_receiver_state_bytes (
    const dp_async_dsss_receiver_state_t * state
) 
```



Envelope + [**async\_dsss\_receiver\_extra\_t**](structasync__dsss__receiver__extra__t.md) + every child's blob + a fixed tsamps-sample carry region + the hand-over ring (refine\_hist\_cap samples); fixed for a given configuration.




**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Blob size, bytes. 





        

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
...                        symbol_rate=2700.0, sample_rate=6.138e6)
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



Drives the state machine. It emits nothing while searching or refining (an empty return is normal, not an error): a hit seeds the frozen-carrier refine chain, `CarrierAcquisition` sharpens the coarse Doppler estimate, and only once it is ready (or gives up) is the live tracking chain built and demodulation begins. A hit inside a call hands the rest of the call straight to the refine chain, so no sample is dropped there; at the refine's end, what the refine did not consume of the last call goes on to the live chain. Accepts any block size; state carries across calls, so a capture can be fed in frames of any length with no seam. Lost (after the release rule fires) consumes the samples and emit nothing, so the feeding loop is the same in every state; while tracking, the release clock runs on the two lock flags after every call, or on the symbol lock flag alone with `reacquire` (see `lost_confirm_s`). Tracking emits symbols whether or not they are data: a noise false alarm can reach tracking and emit until the release rule lets it go  or, at the default lost\_confirm\_s of 0, until reset()  and symbol lock (`locked`) is what says the symbols are a signal's, so read it rather than the count (#2439). Under the specification's coupled offset + 500 Hz/s Doppler ramp the pre-despread carrier loop removes the full carrier dynamics before the code loop, so the recovered constellation lands cleanly on the BPSK real axis.




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
...     code, chip_rate=chip, symbol_rate=sym, sample_rate=fs,
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

_Envelope magic ("ADRX") identifying an AsyncDsssReceiver blob._ 
```C++
#define ASYNC_DSSS_RECEIVER_STATE_MAGIC `DP_FOURCC ('A', 'D', 'R', 'X')`
```




<hr>



### define ASYNC\_DSSS\_RECEIVER\_STATE\_VERSION 

_Layout version of the AsyncDsssReceiver blob (history below)._ 
```C++
#define ASYNC_DSSS_RECEIVER_STATE_VERSION `22u /* v22: the pre-despread carrier loop is carrier_nda ('CNDA' children for car and car_held); v21: the squared-stream refine estimator reverted, its fields gone (79fa50cfe); v20: no threads, the search's own blob carries set_threads()'s request (#3055); v19: the squared-stream refine estimator (§4.7); v18: the exclusions' and picks' samples; v17: the refine hand-over's ring (#2974); v16: the search child is the correlator's search flavor ('CORL', #2752 3.4b); v15: pooled, search-only picks; v14: the search resampler and its origin; v13: rank, exclusions; v12: samples_in, the dwell alignment, threads; v11: no cell mode; v10: the dll children grew (output level, #2495); v9: refine_min_blocks; v8: refused_hits; v7: no hand-off flavor; v6: the cell pull-in */`
```




<hr>



### define ASYNC\_DSSS\_RX\_BN\_CARRIER 

```C++
#define ASYNC_DSSS_RX_BN_CARRIER `0.04`
```



The widest bandwidth the pre-despread carrier loop takes, per update (a code period): the receiver's own is [**dp\_async\_dsss\_receiver\_carrier\_bn()**](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_carrier_bn) at its design C/N0, which is this value wherever the loop-SNR rule allows it and narrower where it does not. The refine stage's frozen carrier (a Costas whose update is never called) reads only its seed frequency and update length.


0.04 is the widest the loop has been designed at: tracking the specification's 500 Hz/s ramp with a per-period Type-II loop needs the bandwidth to keep the velocity lag (~ramp/omega\_n^2) small, and at the design C/N0's rule the loop is narrower. bn is the closed loop's noise bandwidth (carrier\_nda divides out its detector's slope, #3224). No FLL. 


        

<hr>



### define ASYNC\_DSSS\_RX\_CARRIER\_LOOP\_SNR\_DB 

```C++
#define ASYNC_DSSS_RX_CARRIER_LOOP_SNR_DB `20.0`
```



The pre-despread carrier loop's least loop SNR at the design C/N0, dB: the owner's rule (async-dsss-receiver.md §1.3), `rho S_L / (2 bn)` with `rho = C/N0 T` the update's SNR and `S_L = 1 / (1 + 1/(2 rho))` the M = 2 detector's squaring loss (the owner's ruling, 2026-11-22), so `bn <= rho S_L / 200`. 


        

<hr>



### define ASYNC\_DSSS\_RX\_CARRIER\_PULLIN\_HZ 

```C++
#define ASYNC_DSSS_RX_CARRIER_PULLIN_HZ (
    bn,
    chip_rate,
    code_len
) `((bn) * (chip_rate) / (2.0 * (double)(code_len)))`
```



Half the pre-despread carrier loop's noise bandwidth, Hz, at `bn:` `B_L / 2 = bn / (2 T)`, T a code period. 97.8 Hz at 5 Mcps over Gold-1023 and bn 0.04. It is the owner's half-bandwidth rule for a frequency step; the sweep on carrier\_nda (sweep-1b7312b09) measured its no-slip reach at the 4 dB floor at 0.25 `B_L` (unit form), half of this, so a step at this bound may slip during pull-in, where slips are allowed (the owner's ruling, 2026-11-22). 


        

<hr>



### define ASYNC\_DSSS\_RX\_CAR\_DEN\_ALPHA 

```C++
#define ASYNC_DSSS_RX_CAR_DEN_ALPHA `(1.0 / 256.0)`
```



The pre-despread carrier loop's unit denominator: the EMA weight an update of the window sum's power (docs/design/async-dsss-receiver.md, "The averaging length": 81 / 64 ms at the floor, 10 to 12 times the loop's memory). 


        

<hr>



### define ASYNC\_DSSS\_RX\_CAR\_LOCK\_PD 

```C++
#define ASYNC_DSSS_RX_CAR_LOCK_PD `0.99`
```



The carrier block lock's detection probability a block at the design C/N0, the code lock's. 


        

<hr>



### define ASYNC\_DSSS\_RX\_CAR\_LOCK\_PFA 

```C++
#define ASYNC_DSSS_RX_CAR_LOCK_PFA `1e-3`
```



The carrier block lock's false-alarm rate a block, the code lock's. 


        

<hr>



### define ASYNC\_DSSS\_RX\_DEMOD\_BN\_CARRIER 

```C++
#define ASYNC_DSSS_RX_DEMOD_BN_CARRIER `0.01`
```



MpskReceiver's carrier-loop bn, normalised to the symbol rate. 


        

<hr>



### define ASYNC\_DSSS\_RX\_DEMOD\_BN\_TIMING 

```C++
#define ASYNC_DSSS_RX_DEMOD_BN_TIMING `0.005`
```



MpskReceiver's timing-loop bn, normalised to the symbol rate. 


        

<hr>



### define ASYNC\_DSSS\_RX\_DEMOD\_LOCK\_THRESH 

```C++
#define ASYNC_DSSS_RX_DEMOD_LOCK_THRESH `0.3`
```



MpskReceiver's carrier lock threshold. 


        

<hr>



### define ASYNC\_DSSS\_RX\_DEMOD\_RRC\_BETA 

```C++
#define ASYNC_DSSS_RX_DEMOD_RRC_BETA `0.35`
```



MpskReceiver's RRC roll-off argument (unused by its I&D pulse). 


        

<hr>



### define ASYNC\_DSSS\_RX\_DEMOD\_RRC\_SPAN 

```C++
#define ASYNC_DSSS_RX_DEMOD_RRC_SPAN `8`
```



MpskReceiver's RRC span argument, symbols (unused by its I&D pulse). 


        

<hr>



### define ASYNC\_DSSS\_RX\_DEMOD\_ZETA 

```C++
#define ASYNC_DSSS_RX_DEMOD_ZETA `0.707`
```



MpskReceiver's carrier-loop damping factor. 


        

<hr>



### define ASYNC\_DSSS\_RX\_DLL\_BN 

```C++
#define ASYNC_DSSS_RX_DLL_BN `0.002`
```



The Dll's bn, per epoch whatever the steer rate (the symbol-period aid steers once per symbol and keeps bn per epoch): DsssReceiver's value, narrower than [**dp\_dll\_create()**](dll__core_8h.md#function-dp_dll_create)'s default 0.01 for noise immunity at the low-Es/N0 floor. 


        

<hr>



### define ASYNC\_DSSS\_RX\_DLL\_SPACING 

```C++
#define ASYNC_DSSS_RX_DLL_SPACING `0.5`
```



The tracking Dll's early-late spacing, chips. 


        

<hr>



### define ASYNC\_DSSS\_RX\_DLL\_ZETA 

```C++
#define ASYNC_DSSS_RX_DLL_ZETA `0.707`
```



Damping factor of both Dlls (the tracker and the refine collector). 


        

<hr>



### define ASYNC\_DSSS\_RX\_HO\_DATA 

_The hand-over took the data-template estimator's._ 
```C++
#define ASYNC_DSSS_RX_HO_DATA `2`
```




<hr>



### define ASYNC\_DSSS\_RX\_HO\_NONE 

_Where the last hand-over's Doppler came from (".ho\_src" and_ [_**dp\_async\_dsss\_receiver\_get\_handover\_source()**_](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_get_handover_source) _): none since the last seed (searching or refining), the seed's own (the refine gave up, or its estimate failed the hand-over's checks), or the data-template estimator's (§4.7)._
```C++
#define ASYNC_DSSS_RX_HO_NONE `0`
```




<hr>



### define ASYNC\_DSSS\_RX\_HO\_SEED 

_The hand-over took the seed's own Doppler._ 
```C++
#define ASYNC_DSSS_RX_HO_SEED `1`
```




<hr>



### define ASYNC\_DSSS\_RX\_LOCK\_DOWN 

```C++
#define ASYNC_DSSS_RX_LOCK_DOWN `0.3`
```



Lock metric `locked` must be strictly below to count down. 


        

<hr>



### define ASYNC\_DSSS\_RX\_LOCK\_DWELL 

```C++
#define ASYNC_DSSS_RX_LOCK_DWELL `30u`
```



Symbol-lock metric EMA dwell, symbols (EMA weight 1/dwell). 


        

<hr>



### define ASYNC\_DSSS\_RX\_LOCK\_N\_DOWN 

```C++
#define ASYNC_DSSS_RX_LOCK_N_DOWN `15u`
```



Consecutive symbols below LOCK\_DOWN that drop `locked`. 


        

<hr>



### define ASYNC\_DSSS\_RX\_LOCK\_N\_UP 

```C++
#define ASYNC_DSSS_RX_LOCK_N_UP `30u`
```



Consecutive symbols above LOCK\_UP that declare `locked`. 


        

<hr>



### define ASYNC\_DSSS\_RX\_LOCK\_UP 

```C++
#define ASYNC_DSSS_RX_LOCK_UP `0.5`
```



Lock metric `locked` must be strictly above to count up. 


        

<hr>



### define ASYNC\_DSSS\_RX\_MAX\_EXCLUSIONS 

```C++
#define ASYNC_DSSS_RX_MAX_EXCLUSIONS `64u`
```



Most code phases [**dp\_async\_dsss\_receiver\_set\_exclusions()**](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_set_exclusions) holds: one per signal another receiver tracks. 


        

<hr>



### define ASYNC\_DSSS\_RX\_MAX\_PEAKS 

```C++
#define ASYNC_DSSS_RX_MAX_PEAKS `16u`
```



Peaks the searcher reports per dwell ([**dp\_correlator\_set\_max\_peaks()**](correlator__core_8h.md#function-dp_correlator_set_max_peaks)): the list the receiver filters by exclusion, de-duplicates and ranks ([**dp\_async\_dsss\_receiver\_set\_rank()**](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_set_rank)). A rank is below this. 


        

<hr>



### define ASYNC\_DSSS\_RX\_NO\_REFINE\_MIN\_BINS 

```C++
#define ASYNC_DSSS_RX_NO_REFINE_MIN_BINS (
    bn
) `((size_t)(1.0 / (bn) + 0.5))`
```



The least search depth, `coherent_bins`, a receiver built with `refine` false takes at carrier bandwidth `bn:` half the search's Doppler row, `chip_rate / (2 * code_len * coherent_bins)`, is then inside ASYNC\_DSSS\_RX\_CARRIER\_PULLIN\_HZ, which is `coherent_bins >= 1 / bn` at any chip rate (async-dsss-receiver.md §4.4). 


        

<hr>



### define ASYNC\_DSSS\_RX\_REFINE\_DLL\_SPACING 

```C++
#define ASYNC_DSSS_RX_REFINE_DLL_SPACING `0.5`
```



The refine-stage collection Dll's early-late spacing, chips. 


        

<hr>



### define ASYNC\_DSSS\_RX\_REFINE\_MIN\_BLOCKS 

```C++
#define ASYNC_DSSS_RX_REFINE_MIN_BLOCKS `7u`
```



Default floor on the refine's dwell, blocks: the floor dwell of docs/design/async-dsss-receiver-measurements.md §12.10 (77 Hz of estimate noise at 45 dB-Hz); see [**dp\_async\_dsss\_receiver\_set\_refine\_min\_blocks()**](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_set_refine_min_blocks). 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/async_dsss_receiver/async_dsss_receiver_core.h`

