

# File correlator\_core.h



[**FileList**](files.md) **>** [**correlator**](dir_296d11a08af6c23b555f7cd87cd10cbd.md) **>** [**correlator\_core.h**](correlator__core_8h.md)

[Go to the source code of this file](correlator__core_8h_source.md)

_The one FFT cross-correlator, with coherent integrate-and-dump._ [More...](#detailed-description)

* `#include "doppler/clib_common.h"`
* `#include "doppler/dp_parallel.h"`
* `#include "doppler/dp_state.h"`
* `#include "doppler/f32_buffer/f32_buffer_core.h"`
* `#include "doppler/fft/fft_core.h"`
* `#include "doppler/fft2d/fft2d_core.h"`
* `#include "doppler/cvt/cvt_core.h"`
* `#include "doppler/detection/detection_core.h"`
* `#include "doppler/dp_tlm/dp_tlm_core.h"`
* `#include "doppler/jm_perf.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**det\_peak\_t**](structdet__peak__t.md) <br>_One listed peak of a surface: a cell and its value, in the surface's own units (a magnitude on a coherent surface, a power on a non-coherent one). What det\_peak\_list() (_ [_**correlator\_private.h**_](correlator__private_8h.md) _) returns, and what the acquisition engine keeps per dwell._ |
| struct | [**dp\_correlator\_fold\_t**](structdp__correlator__fold__t.md) <br>_A C reducer: called once per map, in h order, by_ [_**dp\_correlator\_execute\_fold()**_](correlator__core_8h.md#function-dp_correlator_execute_fold) _._ |
| struct | [**dp\_correlator\_handoff\_t**](structdp__correlator__handoff__t.md) <br>_Wire-ready hand-off record built from one_ [_**dp\_correlator\_hit\_t**_](structdp__correlator__hit__t.md) _hit._ |
| struct | [**dp\_correlator\_hit\_t**](structdp__correlator__hit__t.md) <br>_One detection, from any detector or search flavor's_ [_**dp\_correlator\_push()**_](correlator__core_8h.md#function-dp_correlator_push) _._ |
| struct | [**dp\_correlator\_part\_t**](structdp__correlator__part__t.md) <br>_One tile's share of a decided surface (docs/design/acq-surface.md §13): the surface is cut into_ `window_bins` _chunks of whole rows, and the per-cell passes after the fan_ _the magnitude, the CFAR reference, the mask copy, each scan of the peak list_ _run per chunk into one of these, merged serially in tile order. The merge is bit-identical at any thread count because the chunks never move._ |
| struct | [**dp\_correlator\_search**](structdp__correlator__search.md) <br>_The search flavors' policy: everything the two search constructors add on top of the four layers (docs/design/correlator.md section 7, 3.4b). Held by_ [_**dp\_correlator\_state\_t::search**_](structdp__correlator__state__t.md#variable-search) _, allocated once at create; NULL in every other flavor._ |
| struct | [**dp\_correlator\_shape\_t**](structdp__correlator__shape__t.md) <br>_What the engine knows about the SHAPE of the repeated preamble, beyond its samples (doppler#1470)._  |
| struct | [**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) <br>_Correlator state, every flavor._  |
| struct | [**dp\_correlator\_tlm\_t**](structdp__correlator__tlm__t.md) <br>_Telemetry attachment: a borrowed context + this engine's probe ids (docs/design/async-dsss-receiver.md §2.4). NULL ctx (the default) means detached — the one probe site is then a single predicted-not-taken branch per decided dwell. Never in a state blob; preserved across_ [_**dp\_correlator\_set\_state()**_](correlator__core_8h.md#function-dp_correlator_set_state) _like the borrowed code._ |


## Public Types

| Type | Name |
| ---: | :--- |
| enum  | [**det\_noise\_mode\_t**](#enum-det_noise_mode_t)  <br>_How a detector flavor aggregates the noise floor from the correlation magnitudes over the flat bins noise\_lo to noise\_hi, inclusive._  |
| enum  | [**dp\_correlator\_flavor\_t**](#enum-dp_correlator_flavor_t)  <br>_Which constructor built a correlator._  |
| enum  | [**dp\_correlator\_framing\_t**](#enum-dp_correlator_framing_t)  <br>_How_ [_**dp\_correlator\_create()**_](correlator__core_8h.md#function-dp_correlator_create) _frames its input: maps or detections from whole frames, or a search over a stream._ |
| typedef struct [**dp\_correlator\_search**](structdp__correlator__search.md) | [**dp\_correlator\_search\_t**](#typedef-dp_correlator_search_t)  <br>_The search flavors' policy: everything the two search constructors add on top of the four layers (docs/design/correlator.md section 7, 3.4b). Held by_ [_**dp\_correlator\_state\_t::search**_](structdp__correlator__state__t.md#variable-search) _, allocated once at create; NULL in every other flavor._ |
| typedef void(\* | [**dp\_correlator\_surface\_sink\_fn**](#typedef-dp_correlator_surface_sink_fn)  <br>_A surface sink: called once per decided dwell (every_ `decim` _-th) with the dwell's surface in test-statistic units, row-major_`rows` _x_`cols` _(Doppler rows by code-phase columns), and the dwell's_`samples_consumed` _. The pointer is the engine's and is valid only for the call. See_[_**dp\_correlator\_set\_surface\_sink()**_](correlator__core_8h.md#function-dp_correlator_set_surface_sink) _._ |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  size\_t | [**dp\_correlator\_bank\_detect**](#function-dp_correlator_bank_detect) (const [**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state, const float \_Complex \* x, size\_t x\_len, size\_t L\_d, size\_t L\_r, int g\_min, int g\_max, float t\_candidate, float t\_confirm, const float \_Complex \* rates, size\_t rates\_len, float \* out, int32\_t \* out1, uint32\_t \* out2, uint32\_t \* out3, uint32\_t \* out4, size\_t max\_out) <br>_Detect on a coarse carrier grid, refine each candidate at its own cell on a fine one, and keep what confirms._  |
|  size\_t | [**dp\_correlator\_bank\_detect\_max\_out**](#function-dp_correlator_bank_detect_max_out) ([**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state, size\_t x\_len) <br>_Capacity_ [_**dp\_correlator\_bank\_detect()**_](correlator__core_8h.md#function-dp_correlator_bank_detect) _needs: one record per map cell._ |
|  size\_t | [**dp\_correlator\_bank\_fold**](#function-dp_correlator_bank_fold) (const [**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state, const float \_Complex \* x, size\_t x\_len, size\_t L, int g\_min, int g\_max, const float \_Complex \* rates, size\_t rates\_len, const [**dp\_correlator\_fold\_t**](structdp__correlator__fold__t.md) \* fold) <br>_Run one frame through a bank of carrier and rate hypotheses and hand each map to a reducer._  |
|  size\_t | [**dp\_correlator\_bank\_power\_max**](#function-dp_correlator_bank_power_max) (const [**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state, const float \_Complex \* x, size\_t x\_len, size\_t L, int g\_min, int g\_max, const float \_Complex \* rates, size\_t rates\_len, float \* out, uint32\_t \* out1, size\_t max\_out) <br>_The strongest power at every cell over a bank, and which hypothesis it came from._  |
|  size\_t | [**dp\_correlator\_bank\_power\_max\_max\_out**](#function-dp_correlator_bank_power_max_max_out) ([**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state, size\_t x\_len) <br>_Capacity_ [_**dp\_correlator\_bank\_power\_max()**_](correlator__core_8h.md#function-dp_correlator_bank_power_max) _needs: one map._ |
|  size\_t | [**dp\_correlator\_block\_prompt**](#function-dp_correlator_block_prompt) ([**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state, size\_t tile, size\_t col, float \_Complex \* out, size\_t n\_out) <br>_One cell's column of the last whole block: the per-epoch complex correlations at a code phase, the despread stream at epoch rate._  |
|  size\_t | [**dp\_correlator\_block\_raw**](#function-dp_correlator_block_raw) ([**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state, float \_Complex \* out, size\_t n\_out) <br>_The last whole block's raw samples, as pushed._  |
|  void | [**dp\_correlator\_build\_handoff**](#function-dp_correlator_build_handoff) (const [**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state, const [**dp\_correlator\_hit\_t**](structdp__correlator__hit__t.md) \* hit, size\_t code\_len, [**dp\_correlator\_handoff\_t**](structdp__correlator__handoff__t.md) \* out) <br>_Convert one_ [_**dp\_correlator\_push()**_](correlator__core_8h.md#function-dp_correlator_push) _hit into a wire-ready hand-off record._ |
|  int | [**dp\_correlator\_carrier\_freq\_ok**](#function-dp_correlator_carrier_freq_ok) (double carrier\_freq\_hz, double sample\_rate\_hz) <br>_Whether_ `carrier_freq_hz` _is a carrier the code can be coupled to at_`sample_rate_hz:` _0 (uncoupled), or finite and above half the sample rate._ |
|  double \_Complex | [**dp\_correlator\_cell\_corr**](#function-dp_correlator_cell_corr) (const [**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state, const float \_Complex \* x, size\_t col, double f\_hz, double t0) <br>_One epoch's correlation against the replica at ONE code phase and ONE frequency: the engine's surface, evaluated at a single cell, on raw samples._  |
|  void | [**dp\_correlator\_cell\_corr\_grid**](#function-dp_correlator_cell_corr_grid) (const [**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state, const float \_Complex \* x, size\_t n\_epochs, size\_t col, const double \* f\_hz, size\_t n\_f, double t0, double \_Complex \* out) <br>[_**dp\_correlator\_cell\_corr()**_](correlator__core_8h.md#function-dp_correlator_cell_corr) _at many frequencies over consecutive epochs, in one pass: the cells a caller scores once the code phase is settled and only the Doppler is left to search._ |
|  int | [**dp\_correlator\_configure\_search\_raw**](#function-dp_correlator_configure_search_raw) ([**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state, size\_t doppler\_bins, size\_t n\_noncoh) <br>_Pin the search grid directly, bypassing both auto-sizing searches — the advanced escape hatch (mirrors Dll's/Costas's configure\_lock\_raw())._  |
|  size\_t | [**dp\_correlator\_consumed**](#function-dp_correlator_consumed) (const [**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state) <br>_Input samples the last_ [_**dp\_correlator\_push()**_](correlator__core_8h.md#function-dp_correlator_push) _took._ |
|  [**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* | [**dp\_correlator\_create**](#function-dp_correlator_create) (const float \_Complex \* ref, size\_t ny, size\_t nx, [**dp\_correlator\_framing\_t**](correlator__core_8h.md#enum-dp_correlator_framing_t) framing, size\_t doppler\_rows, int detect, size\_t dwell, size\_t ny\_out, size\_t nx\_out, int col\_out, float threshold, size\_t noise\_lo, size\_t noise\_hi, int noise\_mode, double sample\_rate, double chip\_rate, double symbol\_rate, size\_t reps, double cn0\_dbhz, double doppler\_uncertainty, double pfa, double pd, double doppler\_rate, size\_t rate\_hypotheses, size\_t code\_only\_epochs, int nthreads, const char \*\* why) <br>_Build the flavor the knobs name: the one constructor the Python class_ `Correlator` _calls._ |
|  [**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* | [**dp\_correlator\_create\_1d**](#function-dp_correlator_create_1d) (const float \_Complex \* ref, size\_t ref\_len, size\_t dwell, int nthreads, size\_t n\_out) <br>_Allocate a 1-D FFT correlator with coherent integrate-and-dump. Pre-computes conj(FFT(ref)) once at construction so each execute() call costs only two FFTs and n complex multiplies._ `ref` _may be freed after this returns. With_`dwell` _== 1 every call produces output; with larger values the accumulator absorbs_`dwell` _frames before dumping._ |
|  [**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* | [**dp\_correlator\_create\_2d**](#function-dp_correlator_create_2d) (const float \_Complex \* ref, size\_t ny, size\_t nx, size\_t dwell, int nthreads, size\_t ny\_out, size\_t nx\_out, int col\_out) <br>_Allocate a 2-D FFT correlator with coherent integrate-and-dump. Two-dimensional extension of_ [_**dp\_correlator\_create\_1d()**_](correlator__core_8h.md#function-dp_correlator_create_1d) _. The reference is a flat row-major ny×nx CF32 array; its conjugate spectrum is pre-computed once so each execute() call costs two 2-D FFTs plus ny\*nx complex multiplies. The Python wrapper requires_`ref` _to be a 2-D ndarray with shape (ny, nx); it passes a flat view to C._ |
|  [**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* | [**dp\_correlator\_create\_burst**](#function-dp_correlator_create_burst) (const float \_Complex \* tmpl, size\_t n, size\_t reps, double fs, double cn0\_dbhz, double doppler\_uncertainty, double pfa, double pd, int noise\_mode, double doppler\_rate, size\_t rate\_hypotheses) <br>_Create a burst-mode acquisition engine for a repeated preamble: coherent multi-repetition combining, up to_ `reps` _deep._ |
|  [**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* | [**dp\_correlator\_create\_continuous**](#function-dp_correlator_create_continuous) (const uint8\_t \* code, size\_t code\_len, double sample\_rate, double chip\_rate, double symbol\_rate, double cn0\_dbhz, double doppler\_uncertainty, double pfa, double pd, int noise\_mode, size\_t code\_only\_epochs, double doppler\_rate) <br>_Create a continuous-mode acquisition engine: always wideband window-tiling, allowing a block-coherent depth inside the tiles to accommodate waveforms with code-only windows._  |
|  [**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* | [**dp\_correlator\_create\_delay\_doppler**](#function-dp_correlator_create_delay_doppler) (const float \_Complex \* code, size\_t code\_len, size\_t ny, size\_t dwell, int nthreads, size\_t ny\_out) <br>_Allocate the delay-Doppler flavor: a code correlator that maps a raw stream of_ `ny` _code periods onto (Doppler, delay)._ |
|  [**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* | [**dp\_correlator\_create\_detector\_1d**](#function-dp_correlator_create_detector_1d) (const float \_Complex \* ref, size\_t ref\_len, size\_t dwell, size\_t noise\_lo, size\_t noise\_hi, [**det\_noise\_mode\_t**](correlator__core_8h.md#enum-det_noise_mode_t) noise\_mode, float threshold, int nthreads) <br>_Allocate a 1-D streaming detector: the 2-D detector at one row (docs/design/correlator.md section 2.1), so its hits are the 2-D one's at ny = 1, each with row 0 and the lag in col._  |
|  [**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* | [**dp\_correlator\_create\_detector\_2d**](#function-dp_correlator_create_detector_2d) (const float \_Complex \* ref, size\_t ny, size\_t nx, size\_t dwell, size\_t noise\_lo, size\_t noise\_hi, [**det\_noise\_mode\_t**](correlator__core_8h.md#enum-det_noise_mode_t) noise\_mode, float threshold, int nthreads) <br>_Allocate a 2-D streaming detector: the 2-D correlator with the stream and detection layers._  |
|  void | [**dp\_correlator\_destroy**](#function-dp_correlator_destroy) ([**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state) <br>_Destroy and free a correlator, any flavor._  |
|  double | [**dp\_correlator\_dsl\_db**](#function-dp_correlator_dsl_db) ([**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state) <br>_The search's Doppler-offset sidelobe level, dB: 20\*log10 of the largest lag of the code's periodic ambiguity outside the mainlobe, at any Doppler offset the grid's tiles can put between an emitter and a tile, relative to the emitter's own peak (_ [_**dp\_correlator\_shape\_t::dsl**_](structdp__correlator__shape__t.md#variable-dsl) _)._ |
|  size\_t | [**dp\_correlator\_execute**](#function-dp_correlator_execute) ([**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state, const float \_Complex \* in, size\_t n\_in, float \_Complex \* out, size\_t max\_out) <br>_Correlate one 2-D frame and optionally dump the coherent accumulator. Runs the 2-D pipeline: FFT2 → pointwise multiply with ref\_spec → accumulate the cross-spectrum; on dump, IFFT2 → normalise (÷ ny\*nx). Accumulating in the frequency domain and inverting once is exactly the per-frame inverse summed, by linearity of the IFFT — valid because the dwell is_ **coherent** _(a complex sum); a non-coherent (magnitude) integration could not defer the inverse._ |
|  size\_t | [**dp\_correlator\_execute\_all**](#function-dp_correlator_execute_all) ([**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state, const float \_Complex \* in, size\_t n\_in, float \_Complex \* out, size\_t max\_out) <br>_Every map of a batch, unreduced: row h of the (H, n\_out) result is the map of frame h (in C, map h is at_ `out + h * n_out` _)._ |
|  size\_t | [**dp\_correlator\_execute\_all\_max\_out**](#function-dp_correlator_execute_all_max_out) (const [**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state, size\_t n\_in) <br>_Samples_ [_**dp\_correlator\_execute\_all()**_](correlator__core_8h.md#function-dp_correlator_execute_all) _writes for_`n_in` _input samples: H \* n\_out, H = n\_in / (ny \* nx)._ |
|  size\_t | [**dp\_correlator\_execute\_coherent**](#function-dp_correlator_execute_coherent) ([**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state, const float \_Complex \* in, size\_t n\_in, float \_Complex \* out, size\_t max\_out) <br>_The coherent sum of a batch's maps, inverted once._  |
|  size\_t | [**dp\_correlator\_execute\_coherent\_max\_out**](#function-dp_correlator_execute_coherent_max_out) (const [**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state) <br>_Samples_ [_**dp\_correlator\_execute\_coherent()**_](correlator__core_8h.md#function-dp_correlator_execute_coherent) _writes: n\_out._ |
|  size\_t | [**dp\_correlator\_execute\_fold**](#function-dp_correlator_execute_fold) ([**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state, const float \_Complex \* in, size\_t n\_in, const [**dp\_correlator\_fold\_t**](structdp__correlator__fold__t.md) \* fold) <br>_Make the map of every frame in a batch and hand each to a reducer._  |
|  size\_t | [**dp\_correlator\_execute\_max\_out**](#function-dp_correlator_execute_max_out) ([**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state) <br>_Maximum output samples per execute call (_ [_**dp\_correlator\_state\_t::n\_out**_](structdp__correlator__state__t.md#variable-n_out) __ _ny\_out\*nx\_out normally, or ny when a single_[_**dp\_correlator\_state\_t::col\_out**_](structdp__correlator__state__t.md#variable-col_out) _was selected)._ |
|  size\_t | [**dp\_correlator\_execute\_peaks**](#function-dp_correlator_execute_peaks) ([**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state, const float \_Complex \* in, size\_t n\_in, float \* out, uint32\_t \* out1, uint32\_t \* out2, float \* out3, size\_t max\_out) <br>_One record per map of a batch: its peak power, where the peak is, and its mean power._  |
|  size\_t | [**dp\_correlator\_execute\_peaks\_max\_out**](#function-dp_correlator_execute_peaks_max_out) (const [**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state, size\_t n\_in) <br>_Records_ [_**dp\_correlator\_execute\_peaks()**_](correlator__core_8h.md#function-dp_correlator_execute_peaks) _writes for_`n_in` _samples: H._ |
|  size\_t | [**dp\_correlator\_execute\_power\_max**](#function-dp_correlator_execute_power_max) ([**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state, const float \_Complex \* in, size\_t n\_in, float \* out, uint32\_t \* out1, size\_t max\_out) <br>_The largest power over a batch's maps, cell by cell, and the map it came from._  |
|  size\_t | [**dp\_correlator\_execute\_power\_max\_max\_out**](#function-dp_correlator_execute_power_max_max_out) (const [**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state) <br>_Values per output_ [_**dp\_correlator\_execute\_power\_max()**_](correlator__core_8h.md#function-dp_correlator_execute_power_max) _writes: n\_out._ |
|  size\_t | [**dp\_correlator\_execute\_power\_sum**](#function-dp_correlator_execute_power_sum) ([**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state, const float \_Complex \* in, size\_t n\_in, float \* out, size\_t max\_out) <br>_The power of a batch's maps, summed cell by cell: each output cell is the sum over h of that cell's_ `|map_h|^2` _, summed in h order in float._ |
|  size\_t | [**dp\_correlator\_execute\_power\_sum\_max\_out**](#function-dp_correlator_execute_power_sum_max_out) (const [**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state) <br>_Powers_ [_**dp\_correlator\_execute\_power\_sum()**_](correlator__core_8h.md#function-dp_correlator_execute_power_sum) _writes: n\_out._ |
|  [**dp\_correlator\_flavor\_t**](correlator__core_8h.md#enum-dp_correlator_flavor_t) | [**dp\_correlator\_flavor**](#function-dp_correlator_flavor) (const [**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state) <br>_Which constructor built_ `state:` _what_[_**dp\_correlator\_create()**_](correlator__core_8h.md#function-dp_correlator_create) _chose from its knobs, so a caller can ask before reading a member only some flavors serve._ |
|  double | [**dp\_correlator\_gate\_stat**](#function-dp_correlator_gate_stat) (const [**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state) <br>_The gate a dwell's statistic must clear to be a hit, in the units of_ [_**dp\_correlator\_hit\_t::test\_stat**_](structdp__correlator__hit__t.md#variable-test_stat) _:_`threshold` _on the coherent path (one look),_`eta_nc` _on the non-coherent one. A dwell hits when its strongest cell's statistic exceeds this, and every listed peak's statistic does too. Both statistics are linear in the cell's amplitude, so a list consumer can compare a peak's excess over another's against it in the same units. Configuration: fixed by the thresholds the grid was sized with._ |
|  void | [**dp\_correlator\_get\_state**](#function-dp_correlator_get_state) (const [**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state, void \* blob) <br>_Serialize the frame count and spectrum accumulator into_ `blob` _._ |
|  size\_t | [**dp\_correlator\_period\_samples**](#function-dp_correlator_period_samples) (size\_t code\_len) <br>_The samples one code period should be searched at: the smallest power of two that holds at least two samples a chip._  |
|  uint64\_t | [**dp\_correlator\_position**](#function-dp_correlator_position) (const [**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state) <br>_The engine's stream position: every sample its pushes have taken, framed ones and the carry, since create, reset, a regrid or set\_state's restore._  |
|  double | [**dp\_correlator\_psl\_db**](#function-dp_correlator_psl_db) (const [**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state) <br>_The preamble's peak sidelobe level, dB: 20\*log10 of the largest periodic-autocorrelation lag outside the mainlobe, relative to the peak (_ [_**dp\_correlator\_shape\_t::psl**_](structdp__correlator__shape__t.md#variable-psl) _)._ |
|  size\_t | [**dp\_correlator\_push**](#function-dp_correlator_push) ([**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state, const float \_Complex \* in, size\_t n\_in, [**dp\_correlator\_hit\_t**](structdp__correlator__hit__t.md) \* result, size\_t max\_results) <br>_Stream an arbitrary-length CF32 chunk through a detector flavor._  |
|  size\_t | [**dp\_correlator\_rate\_hypotheses\_for**](#function-dp_correlator_rate_hypotheses_for) (double f\_epoch, double span, size\_t depth) <br>_The fewest rate hypotheses over_ `[-span, +span]` _that leave the residual_`span/(n - 1)` _within what a block of_`depth` _epochs tolerates,_`f_epoch^2/(2*depth^2)` __ _the drift rule behind the depth cap, inverted. At least 2 for a positive span; 1 for none; at most_[_**DP\_CORRELATOR\_RATE\_HYPS\_MAX**_](correlator__core_8h.md#define-dp_correlator_rate_hyps_max) _._ |
|  double | [**dp\_correlator\_rate\_hypothesis**](#function-dp_correlator_rate_hypothesis) (double span, size\_t n, size\_t h) <br>_Rate hypothesis_ `h` _of_`n` _over_`[-span, +span]` _, Hz/s:_`-span + 2*span*h/(n - 1)` _; 0 for_`n <= 1` _._ |
|  double \_Complex | [**dp\_correlator\_rate\_phasor**](#function-dp_correlator_rate_phasor) (double rate, double t) <br>_The de-chirp phasor_ `exp(-j*pi*rate*t^2)` _for a rate in Hz/s at time_`t` _seconds from the block's middle._ |
|  void | [**dp\_correlator\_reset**](#function-dp_correlator_reset) ([**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state) <br>_Zero the accumulator and reset the integration counter to 0. Equivalent to starting a fresh dwell cycle without rebuilding FFT plans or recomputing ref\_spec._  |
|  size\_t | [**dp\_correlator\_run**](#function-dp_correlator_run) ([**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state, const void \* state\_in, void \* state\_out, const float \_Complex \* in, size\_t n\_in, [**dp\_correlator\_hit\_t**](structdp__correlator__hit__t.md) \* result, size\_t max\_results) <br>_Pure run: inject_ `state_in` _, stream_`in` _, emit hits, export_`state_out` _—_`(state_in, input) -> (state_out, output)` _over an engine treated as immutable config + scratch._`state_in` _/_`state_out` _may alias. Either may be NULL (NULL in = fresh; NULL out = discard)._ |
|  size\_t | [**dp\_correlator\_search\_period**](#function-dp_correlator_search_period) (size\_t code\_len, double sample\_rate, double chip\_rate) <br>_The samples one code period is searched at, for a caller feeding_ `sample_rate:` _its own period when that is already fast, else_[_**dp\_correlator\_period\_samples()**_](correlator__core_8h.md#function-dp_correlator_period_samples) _._ |
|  int | [**dp\_correlator\_set\_carrier\_freq\_hz**](#function-dp_correlator_set_carrier_freq_hz) ([**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state, double carrier\_freq\_hz) <br>_Couple the code clock to the carrier: the chip rate dilates by_ `doppler_hz / carrier_freq_hz` _, and the engine accounts for it._ |
|  void | [**dp\_correlator\_set\_keep\_surface**](#function-dp_correlator_set_keep_surface) ([**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state, int keep) <br>_Keep, or stop keeping, every decided dwell's surface for_ [_**dp\_correlator\_surface()**_](correlator__core_8h.md#function-dp_correlator_surface) _: 1 normalises each one into the gate's units at its decision, 0 (the default) costs nothing._ |
|  int | [**dp\_correlator\_set\_max\_peaks**](#function-dp_correlator_set_max_peaks) ([**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state, size\_t n) <br>_How many peaks a dwell may report: the peak list's capacity._  |
|  int | [**dp\_correlator\_set\_ref**](#function-dp_correlator_set_ref) ([**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state, const float \_Complex \* ref) <br>_Replace the reference and recompute its spectrum._  |
|  int | [**dp\_correlator\_set\_state**](#function-dp_correlator_set_state) ([**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state, const void \* blob) <br>_Restore a mid-dwell correlator from_ [_**dp\_correlator\_get\_state()**_](correlator__core_8h.md#function-dp_correlator_get_state) _'s blob._ |
|  void | [**dp\_correlator\_set\_surface\_sink**](#function-dp_correlator_set_surface_sink) ([**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state, [**dp\_correlator\_surface\_sink\_fn**](correlator__core_8h.md#typedef-dp_correlator_surface_sink_fn) fn, void \* ctx, uint32\_t decim) <br>_Attach (or detach) a C surface sink: every_ `decim-th` _decided dwell's surface, in test-statistic units, handed to_`fn` _on the pushing thread (docs/design/async-dsss-receiver.md §2.4)._ |
|  int | [**dp\_correlator\_set\_telemetry**](#function-dp_correlator_set_telemetry) ([**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state, [**dp\_tlm\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_t) \* tlm, const char \* prefix, uint32\_t decim) <br>_Attach (or detach) a telemetry context and register the engine's probes on it (docs/design/async-dsss-receiver.md §2.4)._  |
|  int | [**dp\_correlator\_set\_threads**](#function-dp_correlator_set_threads) ([**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state, int n) <br>_Set how many threads the searcher fans its tiles across (docs/design/acq-surface.md §13: a roll per thread on persistent workers)._  |
|  int | [**dp\_correlator\_set\_threshold**](#function-dp_correlator_set_threshold) ([**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state, float threshold) <br>_Change a detector flavor's threshold without rebuilding it._  |
|  double | [**dp\_correlator\_sidelobe\_ratio**](#function-dp_correlator_sidelobe_ratio) ([**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state, const [**dp\_correlator\_hit\_t**](structdp__correlator__hit__t.md) \* from, const [**dp\_correlator\_hit\_t**](structdp__correlator__hit__t.md) \* to) <br>_How far up the sidelobes of the emitter behind one hit can reach at another hit's cell, relative to that emitter's own peak._  |
|  size\_t | [**dp\_correlator\_state\_bytes**](#function-dp_correlator_state_bytes) (const [**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state) <br>_Size in bytes of this correlator's state blob._  |
|  int | [**dp\_correlator\_state\_check**](#function-dp_correlator_state_check) (const [**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state, const void \* blob) <br>_Whether_ [_**dp\_correlator\_set\_state()**_](correlator__core_8h.md#function-dp_correlator_set_state) _would accept_`blob` _, without writing anything._ |
|  int | [**dp\_correlator\_state\_position**](#function-dp_correlator_state_position) (const [**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state, const void \* blob, uint64\_t \* position) <br>_The stream position a blob records_  __[_**dp\_correlator\_position()**_](correlator__core_8h.md#function-dp_correlator_position) _of the engine it would restore_ _read without restoring it._ |
|  size\_t | [**dp\_correlator\_surface**](#function-dp_correlator_surface) ([**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state, float \* out, size\_t n\_out) <br>_The last decided dwell's surface, in the gate's own units._  |
|  size\_t | [**dp\_correlator\_surface\_chip\_phase**](#function-dp_correlator_surface_chip_phase) ([**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state, double \* out, size\_t n\_out) <br>_The surface's code-phase axis: the chip phase of each column._  |
|  size\_t | [**dp\_correlator\_surface\_complex**](#function-dp_correlator_surface_complex) ([**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state, float \_Complex \* out, size\_t n\_out) <br>_The last decided dwell's surface, complex: amplitude and carrier phase per cell, before the magnitude the gate reads._  |
|  size\_t | [**dp\_correlator\_surface\_doppler\_hz**](#function-dp_correlator_surface_doppler_hz) ([**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state, double \* out, size\_t n\_out) <br>_The surface's Doppler axis: the frequency of each row, in Hz._  |


## Public Static Functions

| Type | Name |
| ---: | :--- |
|  double | [**dp\_correlator\_bin\_doppler\_hz**](#function-dp_correlator_bin_doppler_hz) (const [**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state, size\_t doppler\_bin) <br>_A hit's signed Doppler, Hz: its_ `row` _folded over dp\_correlator\_grid\_bins() by numpy's fftfreq convention (dp\_fftfreq\_index) and scaled by the grid's resolution._ |
|  size\_t | [**dp\_correlator\_grid\_bins**](#function-dp_correlator_grid_bins) (const [**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state) <br>_The Doppler grid a hit's_ `row` _indexes:_`window_bins * coherent_bins` _bins._ |

























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**CORRELATOR\_STATE\_MAGIC**](correlator__core_8h.md#define-correlator_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('C','O','R','L')`<br>_State-blob magic ('CORL'); set\_state() rejects any other._  |
| define  | [**CORRELATOR\_STATE\_VERSION**](correlator__core_8h.md#define-correlator_state_version)  `1u`<br>_State-blob layout version; set\_state() rejects any other._  |
| define  | [**DP\_CORRELATOR\_CN0\_NONE**](correlator__core_8h.md#define-dp_correlator_cn0_none)  `NAN`<br>_The design C/N0 that means "none given" (doppler#1484): a burst engine then sizes for the whole preamble, its_ `pd_predicted` _is NaN and it is never_`underpowered` _._ |
| define  | [**DP\_CORRELATOR\_DELAY\_LOSS\_NODES**](correlator__core_8h.md#define-dp_correlator_delay_loss_nodes)  `4`<br> |
| define  | [**DP\_CORRELATOR\_MAX\_PEAKS**](correlator__core_8h.md#define-dp_correlator_max_peaks)  `64u`<br> |
| define  | [**DP\_CORRELATOR\_N\_NONCOH\_SAFETY\_CEILING**](correlator__core_8h.md#define-dp_correlator_n_noncoh_safety_ceiling)  `256u`<br>_Internal safety-valve ceiling on auto-selected non-coherent looks_  _not a public knob (no caller-facing equivalent of the retired_`max_noncoh` _parameter)._ |
| define  | [**DP\_CORRELATOR\_RATE\_HYPS\_MAX**](correlator__core_8h.md#define-dp_correlator_rate_hyps_max)  `64u`<br> |
| define  | [**DP\_CORRELATOR\_SIDELOBE\_DOPPLER\_PTS**](correlator__core_8h.md#define-dp_correlator_sidelobe_doppler_pts)  `9`<br> |
| define  | [**DP\_CORRELATOR\_SIDELOBE\_LAG\_PTS**](correlator__core_8h.md#define-dp_correlator_sidelobe_lag_pts)  `5`<br> |
| define  | [**DP\_CORRELATOR\_SIDELOBE\_SLOTS**](correlator__core_8h.md#define-dp_correlator_sidelobe_slots)  `32`<br> |

## Detailed Description


One object, built by flavor (docs/design/correlator.md section 4):



|constructor   |flavor    |
|-----|-----|
|[**dp\_correlator\_create\_1d()**](correlator__core_8h.md#function-dp_correlator_create_1d)   |a 1-D reference, one row    |
|[**dp\_correlator\_create\_2d()**](correlator__core_8h.md#function-dp_correlator_create_2d)   |a (ny, nx) reference    |
|[**dp\_correlator\_create\_delay\_doppler()**](correlator__core_8h.md#function-dp_correlator_create_delay_doppler)   |a code, mapped to (Doppler, delay)    |
|[**dp\_correlator\_create\_detector\_2d()**](correlator__core_8h.md#function-dp_correlator_create_detector_2d)   |a streaming 2-D detector    |
|[**dp\_correlator\_create\_detector\_1d()**](correlator__core_8h.md#function-dp_correlator_create_detector_1d)   |a streaming 1-D detector    |
|[**dp\_correlator\_create\_continuous()**](correlator__core_8h.md#function-dp_correlator_create_continuous)   |a continuous DSSS search    |
|[**dp\_correlator\_create\_burst()**](correlator__core_8h.md#function-dp_correlator_create_burst)   |a burst preamble search   |






The methods are shared. The 1-D flavor is the 2-D one at ny = 1, which the single-row path below serves, so a length-n correlation and a one-row 2-D correlation are the same bits (test\_corr\_equiv), and so are the two detectors' hits (test\_detector\_equiv). The map flavors serve execute and its batch family; the detector and search flavors serve push, and each refuses the other's (SIZE\_MAX). The search flavors are the acquisition engine (the search flavors' section, at the end).


All buffers are ny×nx row-major flat arrays of length ny\*nx. The correlation theorem extends naturally:


`R_xh[i,j] = IFFT2( FFT2(x) · conj(FFT2(h)) ) / (ny*nx)`


The reference spectrum is pre-computed at create time. The int-dump semantics are identical to the 1-D case: coherently sum `dwell` frames, then dump.


**Single-row-reference fast path.** When `ref` is nonzero only in row 0 (the DSSS acquisition/detection shape: a code replica with no Doppler content of its own) and `ny_out` == `ny` (no row-axis interpolation), `ref_spec[u,v]` is independent of `u` — the row axis of the 2-D transform pair cancels to an exact identity (DFT orthogonality: sum\_u exp(2\*pi\*i\*u\*(i-i)/ny) = ny`iff`i==i'`, else 0), for *any* row content.`dp\_correlator\_execute`then reduces exactly to `R(i,j) = IFFT\_nx(FFT\_nx(row\_i) · conj(FFT\_nx(ref\_row0)))(j) / nx`, applied independently per row — so`dp\_correlator\_create\_2d`detects this shape and runs`ny`independent length-`nx`1-D FFTs instead of a full`(ny,nx)` 2-D FFT, skipping the row-axis work that would otherwise cancel to a no-op. Any other reference shape, or`ny\_out &gt; ny`, uses the general 2-D path unchanged.


Lifecycle: 
```C++
enum { NY = 4, NX = 8 };
float _Complex ref[NY * NX] = { 1.0f };   // row-major 2-D reference
float _Complex frame[NY * NX] = { 1.0f }; // one frame, fed four times
float _Complex out[NY * NX];
dp_correlator_state_t *c
    = dp_correlator_create_2d (ref, NY, NX, 4, 1, 0, 0, -1);
size_t dumps = 0;
for (int i = 0; i < 4; i++)
  dumps += dp_correlator_execute (c, frame, NY * NX, out, NY * NX) != 0;
printf ("dumps: %zu, peak %.1f\n", dumps, crealf (out[0])); // 1, on i == 3
dp_correlator_destroy (c);
```
 


    
## Public Types Documentation




### enum det\_noise\_mode\_t 

_How a detector flavor aggregates the noise floor from the correlation magnitudes over the flat bins noise\_lo to noise\_hi, inclusive._ 
```C++
enum det_noise_mode_t {
    DET_NOISE_MEAN = 0,
    DET_NOISE_MEDIAN = 1,
    DET_NOISE_MIN = 2,
    DET_NOISE_MAX = 3
};
```



The test statistic is peak\_mag / noise\_est. A zero noise\_est (e.g., when all bins in the range are zero) yields test\_stat = 0, and so does a NaN one (a NaN anywhere in the range is a NaN mean, median, min or max). 


        

<hr>



### enum dp\_correlator\_flavor\_t 

_Which constructor built a correlator._ 
```C++
enum dp_correlator_flavor_t {
    DP_CORRELATOR_1D = 1,
    DP_CORRELATOR_2D = 2,
    DP_CORRELATOR_DELAY_DOPPLER = 3,
    DP_CORRELATOR_DETECTOR_1D = 4,
    DP_CORRELATOR_DETECTOR_2D = 5,
    DP_CORRELATOR_SEARCH_CONTINUOUS = 6,
    DP_CORRELATOR_SEARCH_BURST = 7
};
```



Fixed at create and the state blob's first field: a blob restores only into a correlator of its own flavor. 


        

<hr>



### enum dp\_correlator\_framing\_t 

_How_ [_**dp\_correlator\_create()**_](correlator__core_8h.md#function-dp_correlator_create) _frames its input: maps or detections from whole frames, or a search over a stream._
```C++
enum dp_correlator_framing_t {
    DP_CORRELATOR_FRAMING_NONE = 0,
    DP_CORRELATOR_FRAMING_CONTINUOUS = 1,
    DP_CORRELATOR_FRAMING_BURST = 2
};
```




<hr>



### typedef dp\_correlator\_search\_t 

_The search flavors' policy: everything the two search constructors add on top of the four layers (docs/design/correlator.md section 7, 3.4b). Held by_ [_**dp\_correlator\_state\_t::search**_](structdp__correlator__state__t.md#variable-search) _, allocated once at create; NULL in every other flavor._
```C++
typedef struct dp_correlator_search dp_correlator_search_t;
```




<hr>



### typedef dp\_correlator\_surface\_sink\_fn 

_A surface sink: called once per decided dwell (every_ `decim` _-th) with the dwell's surface in test-statistic units, row-major_`rows` _x_`cols` _(Doppler rows by code-phase columns), and the dwell's_`samples_consumed` _. The pointer is the engine's and is valid only for the call. See_[_**dp\_correlator\_set\_surface\_sink()**_](correlator__core_8h.md#function-dp_correlator_set_surface_sink) _._
```C++
typedef void(* dp_correlator_surface_sink_fn) (void *ctx, const float *surface, size_t rows, size_t cols, uint64_t samples_consumed);
```




<hr>
## Public Functions Documentation




### function dp\_correlator\_bank\_detect 

_Detect on a coarse carrier grid, refine each candidate at its own cell on a fine one, and keep what confirms._ 
```C++
size_t dp_correlator_bank_detect (
    const dp_correlator_state_t * state,
    const float _Complex * x,
    size_t x_len,
    size_t L_d,
    size_t L_r,
    int g_min,
    int g_max,
    float t_candidate,
    float t_confirm,
    const float _Complex * rates,
    size_t rates_len,
    float * out,
    int32_t * out1,
    uint32_t * out2,
    uint32_t * out3,
    uint32_t * out4,
    size_t max_out
) 
```



The bank searches on `L_d` and resolves on `L_r` (docs/design/corr2d-hypotheses.md section 6):



* **Detect.** [**dp\_correlator\_bank\_power\_max()**](correlator__core_8h.md#function-dp_correlator_bank_power_max) over carriers `g_min` .. `g_max` on the `1 / (L_d nx)` grid, with every rate.
* **Candidates.** Every cell whose power exceeds `t_candidate` and is at least each of its 8 neighbors, both axes circular (a delay lag and a Doppler bin both wrap).
* **Refine.** For a candidate found by coarse carrier g\_d and rate r, the map value at its cell under every fine carrier g\_r from `m g_d - (m - 1)` to `m g_d + (m - 1)`, m = `L_r / L_d`: the carriers between its coarse neighbors, on the `1 / (L_r nx)` grid. On the fast path and DelayDoppler with a native delay axis each is a time-domain correlation at the cell's lag and a slow-time DFT at its Doppler row, in double, with no FFT: `2 ny nx` multiply-adds a carrier. Any other object makes that hypothesis's map and reads the cell. The best g\_r wins, the lowest on a tie.
* **Confirm.** A candidate whose refined power exceeds `t_confirm` is a record.




Records come out in cell order (row-major), one entry of each output per record: `out` the refined power, `out1` g\_r on the fine grid, `out2` the rate r, `out3` the row and `out4` the column, the cell as execute() lays out the map. Measured at 64 x 512 against an oracle that corrects the exact offset, `L_d` = 2 with `L_r` = 4 holds L = 4's sensitivity within 0.16 dB, for about half the bank (the design page). `L_d` = `L_r` is the one-stage bank. Thresholds are powers in the map's own units; the caller sets them for its noise, as for any detector.




**Parameters:**


* `state` As [**dp\_correlator\_bank\_fold()**](correlator__core_8h.md#function-dp_correlator_bank_fold); `g_min` and `g_max` are on the coarse grid. 
* `L_d` The search grid; at least 1. 
* `L_r` The resolution grid; a multiple of `L_d`. 
* `t_candidate` Candidate threshold, on the coarse power. 
* `t_confirm` Confirm threshold, on the refined power. 
* `out` Refined powers. 
* `out1` Refined carriers, on the `1 / (L_r nx)` grid. 
* `out2` Rates. 
* `out3` Rows. 
* `out4` Columns. 
* `max_out` Capacity of each output; at least `n_out`. 



**Returns:**

Python: the tuple (power, g, rate, row, col), one entry per confirmed record (possibly none): float32 refined power, int32 refined carrier, uint32 rate, row and column, always new arrays (this call takes no `out=`); anything below is a ValueError. C: the record count (0 included), or SIZE\_MAX with nothing written for anything [**dp\_correlator\_bank\_fold()**](correlator__core_8h.md#function-dp_correlator_bank_fold) refuses, `L_d == 0`, an `L_r` that `L_d` does not divide, a refined carrier past int32, or `max_out` below `n_out`. 
```C++
>>> import numpy as np
>>> from doppler.spectral import DelayDoppler
>>> rng = np.random.default_rng(0)
>>> ny, nx, d = 8, 32, 5
>>> code = rng.choice([-1.0, 1.0], nx).astype(np.complex64)
>>> m = np.arange(ny * nx)
>>> x = (np.tile(np.roll(code, d), ny)    # 1.25 bins: between L_d = 2
...      * np.exp(2j * np.pi * 1.25 / nx * m)).astype(np.complex64)
>>> dd = DelayDoppler(code, ny=ny)
>>> no_rates = np.empty(0, np.complex64)
>>> coarse, _ = dd.bank_power_max(x, 2, -4, 4, no_rates)
>>> t = 0.5 * coarse.max()
>>> power, g, rate, row, col = dd.bank_detect(x, 2, 4, -4, 4, t, t,
...                                           no_rates)
>>> len(power), int(g[0]), int(col[0])    # g on the 1/(4 nx) grid
(1, 5, 5)
>>> round(float(power[0] / coarse.max()), 3)  # the straddle recovered
1.233
```
 





        

<hr>



### function dp\_correlator\_bank\_detect\_max\_out 

_Capacity_ [_**dp\_correlator\_bank\_detect()**_](correlator__core_8h.md#function-dp_correlator_bank_detect) _needs: one record per map cell._
```C++
size_t dp_correlator_bank_detect_max_out (
    dp_correlator_state_t * state,
    size_t x_len
) 
```





**Parameters:**


* `state` The correlator. 
* `x_len` The frame's length (the bound does not depend on it). 



**Returns:**

`n_out`. 





        

<hr>



### function dp\_correlator\_bank\_fold 

_Run one frame through a bank of carrier and rate hypotheses and hand each map to a reducer._ 
```C++
size_t dp_correlator_bank_fold (
    const dp_correlator_state_t * state,
    const float _Complex * x,
    size_t x_len,
    size_t L,
    int g_min,
    int g_max,
    const float _Complex * rates,
    size_t rates_len,
    const dp_correlator_fold_t * fold
) 
```



A bank is what a frame batch of corrected copies would compute, without the copies: carrier g corrects an offset of `g / (L * nx)` cycles/sample, for every g from `g_min` to `g_max`, and each rate r weights the slow-time samples by the r-th ny-long vector of `rates`. Map h, for `h = (g - g_min) * R + r` (R = `rates_len / ny`, or 1 with no rates), is what a fresh dwell-1 object's execute() returns for the frame with period i multiplied by element i of rate vector r and by `exp(-j 2 pi g n / (L nx))`, where n counts samples WITHIN period i (0 .. nx-1), up to float rounding.


The correction restarts at every period on purpose: the phase a carrier adds between periods is slow-time Doppler, which the slow-time FFT measures, so a signal stays on its own absolute Doppler row under every carrier hypothesis (docs/design/corr2d-hypotheses.md, rule 4).


Every path runs a bank, and only the single-row fast path and DelayDoppler share work: whole bins (g a multiple of L) are a roll of the row spectrum, each fraction `g mod L` mixes and transforms the rows once for every whole bin that shares it. On DelayDoppler the one delay-Doppler map then runs in the order its rule picks for the bank's carriers per fraction and its rates (docs/design/acq-surface.md §18.1): rows first inverts each carrier's rows once for all its rates and pays a slow-time FFT per rate; slow first shares each fraction's slow time across its whole-bin carriers and pays their rows per rate. On Corr2D, whose rows are its periods, a rate scales the carrier's rows. The general 2-D path and a known column run each hypothesis's corrected frame through their own frame code, which is the definition with nothing shared.


Maps reach `fold` in h order, so a reducer's tie rule is the batch's. The stages split across the object's workers exactly as execute() does, and every map is bit-identical for any worker count. The dwell accumulator, `count` and the state blob are never read or written.




**Parameters:**


* `state` Any Corr2D or DelayDoppler object. 
* `x` One frame, `x_len` samples. 
* `x_len` Must be exactly `ny * nx`. 
* `L` Fractions per whole bin; at least 1. 
* `g_min` Lowest carrier, in units of `1 / (L * nx)` cycles/sample. 
* `g_max` Highest carrier; at least `g_min`. 
* `rates` Per-period weights, R vectors of ny, or NULL with `rates_len` 0 (one unit vector). 
* `rates_len` A whole multiple of ny, 0 included. 
* `fold` The reducer, called once per map. 



**Returns:**

The number of maps, `(g_max - g_min + 1) * R`; or SIZE\_MAX, with nothing folded, for a frame of another length, `L == 0`, `g_min > g_max`, a `rates_len` that is not a whole number of ny-long vectors, a count past 2^32, or scratch that cannot be allocated. 
```C++
#include "doppler/correlator/correlator_core.h"
#include <stdio.h>
static void count_maps (void *ctx, size_t h, const float _Complex *map,
                        size_t n_out)
{
  (void)h, (void)map, (void)n_out;
  ++*(size_t *)ctx;
}
int main (void)
{
  float _Complex code[8] = { 1, -1, 1, 1, -1, 1, -1, -1 }, x[4 * 8] = { 0 };
  dp_correlator_state_t *dd = dp_correlator_create_delay_doppler (code, 8, 4, 1, 1, 0);
  size_t maps = 0;
  dp_correlator_fold_t f = { count_maps, &maps };
  size_t h = dp_correlator_bank_fold (dd, x, 4 * 8, 4, -4, 4, NULL, 0, &f);
  printf ("%zu %zu\n", h, maps); // 9 carriers, one rate: 9 maps
  dp_correlator_destroy (dd);
  return !(h == 9 && maps == 9);
}
```
 





        

<hr>



### function dp\_correlator\_bank\_power\_max 

_The strongest power at every cell over a bank, and which hypothesis it came from._ 
```C++
size_t dp_correlator_bank_power_max (
    const dp_correlator_state_t * state,
    const float _Complex * x,
    size_t x_len,
    size_t L,
    int g_min,
    int g_max,
    const float _Complex * rates,
    size_t rates_len,
    float * out,
    uint32_t * out1,
    size_t max_out
) 
```



[**dp\_correlator\_bank\_fold()**](correlator__core_8h.md#function-dp_correlator_bank_fold) with the `POWER_MAX` reducer: at each cell, `out` holds the largest \|map\_h\|^2 over every hypothesis h, and `out1` that h, `(g - g_min) * R + r`, the lowest h on a tie. Every argument and refusal is [**dp\_correlator\_bank\_fold()**](correlator__core_8h.md#function-dp_correlator_bank_fold)'s; in addition `max_out` below `n_out` is refused with nothing written.




**Parameters:**


* `state` As [**dp\_correlator\_bank\_fold()**](correlator__core_8h.md#function-dp_correlator_bank_fold). 
* `out` `n_out` powers. 
* `out1` `n_out` hypothesis indices. 
* `max_out` Capacity of each output; at least `n_out`. 



**Returns:**

Python: the tuple (power, index), each (n\_out,): float32 powers and uint32 hypothesis indices, always new arrays (this call takes no `out=`); a refusal is a ValueError. C: `n_out`, or SIZE\_MAX with nothing written. 
```C++
>>> import numpy as np
>>> from doppler.spectral import DelayDoppler
>>> rng = np.random.default_rng(0)
>>> ny, nx, d = 8, 32, 5                  # code delayed by 5 samples
>>> code = rng.choice([-1.0, 1.0], nx).astype(np.complex64)
>>> m = np.arange(ny * nx)
>>> x = (np.tile(np.roll(code, d), ny)    # on a carrier 1.5 bins off
...      * np.exp(2j * np.pi * 1.5 / nx * m)).astype(np.complex64)
>>> dd = DelayDoppler(code, ny=ny)
>>> no_rates = np.empty(0, np.complex64)
>>> pw, h = dd.bank_power_max(x, 2, -4, 4, no_rates)  # g / (2 nx), -4..4
>>> cell = int(np.argmax(pw))
>>> cell % nx, int(h[cell]) - 4           # (delay, g = h + g_min)
(5, 3)
```
 





        

<hr>



### function dp\_correlator\_bank\_power\_max\_max\_out 

_Capacity_ [_**dp\_correlator\_bank\_power\_max()**_](correlator__core_8h.md#function-dp_correlator_bank_power_max) _needs: one map._
```C++
size_t dp_correlator_bank_power_max_max_out (
    dp_correlator_state_t * state,
    size_t x_len
) 
```





**Parameters:**


* `state` The correlator. 
* `x_len` The frame's length (one map whatever it is). 



**Returns:**

`n_out`. 





        

<hr>



### function dp\_correlator\_block\_prompt 

_One cell's column of the last whole block: the per-epoch complex correlations at a code phase, the despread stream at epoch rate._ 
```C++
size_t dp_correlator_block_prompt (
    dp_correlator_state_t * state,
    size_t tile,
    size_t col,
    float _Complex * out,
    size_t n_out
) 
```



The block-coherent engine gathers every tile's correlation row for `coherent_bins` epochs before its slow-time transform (file doc, docs/design/acq-surface.md §4). This copies the `coherent_bins` values at column `col` of tile `tile` into `out`, in epoch order: each epoch's complex prompt at that code phase, rolled to the tile's centre frequency and shifted to the block's middle by the tile's code-rate hypothesis, so the phase is continuous along the column for an emitter at the tile's centre and rotates at its offset from it. At an emitter's cell this is what a despreader produces, one value per epoch, phase included (docs/design/async-dsss-receiver-measurements.md §12.21). Valid once a block is whole, until the next epoch is pushed.




**Parameters:**


* `state` Must be non-NULL. 
* `tile` Tile index, `0 … window_bins-1` (native FFT order, the order the surface's rows are cut in). 
* `col` Code-phase column, `0 … lag_bins-1`. 
* `out` At least `coherent_bins` complex floats. 
* `n_out` Capacity of `out`. 



**Returns:**

Values written (`coherent_bins`), or 0 at `coherent_bins == 1` (no block is gathered), while a block is partial, for an index out of range, or when `out` is too small. 
```C++
>>> import numpy as np
>>> from doppler.acquire import Acquisition
>>> from doppler.wfm import PN, mls_poly
>>> code = np.asarray(PN(poly=mls_poly(5), seed=1,
...                      length=5).generate(31)).astype(np.uint8)
>>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), 4).astype(
...     np.complex64)
>>> b = Acquisition(code, sample_rate=4e6, chip_rate=1e6, cn0_dbhz=60.0,
...                 doppler_uncertainty=40e3, code_only_epochs=7)
>>> b.coherent_bins                    # (7 + 1) // 2
4
>>> blk = np.tile(np.roll(s0, 17), b.coherent_bins).astype(np.complex64)
>>> _ = b.push(blk)
>>> p = np.empty(b.coherent_bins, dtype=np.complex64)
>>> b.block_prompt(0, 17, p) == b.coherent_bins
True
>>> bool(np.abs(p).min() > 0.99 * np.abs(p).max())  # every epoch's prompt
True
>>> b.block_prompt(0, 17, np.empty(1, dtype=np.complex64))  # too small
0
```
 





        

<hr>



### function dp\_correlator\_block\_raw 

_The last whole block's raw samples, as pushed._ 
```C++
size_t dp_correlator_block_raw (
    dp_correlator_state_t * state,
    float _Complex * out,
    size_t n_out
) 
```



Copies the `coherent_bins * code_bins` samples the block-coherent engine gathered for its last whole block into `out`, epoch by epoch in stream order — the samples [**dp\_correlator\_push()**](correlator__core_8h.md#function-dp_correlator_push) consumed, untouched. Kept for the tile-edge re-ask (search\_resolve\_tile\_alias()); exposed so a tracker can re-correlate them at any code phase, rate or symbol boundary the engine's own grid does not have — a symbol-rate despreader at the tracked timing runs on exactly this (docs/design/async-dsss-receiver-measurements.md §12.21). Valid once a block is whole, until the next epoch is pushed.




**Parameters:**


* `state` Must be non-NULL. 
* `out` At least `coherent_bins * code_bins` complex floats. 
* `n_out` Capacity of `out`. 



**Returns:**

Samples written (`coherent_bins * code_bins`), or 0 at `coherent_bins == 1`, while a block is partial, or when `out` is too small. 
```C++
>>> import numpy as np
>>> from doppler.acquire import Acquisition
>>> from doppler.wfm import PN, mls_poly
>>> code = np.asarray(PN(poly=mls_poly(5), seed=1,
...                      length=5).generate(31)).astype(np.uint8)
>>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), 4).astype(
...     np.complex64)
>>> b = Acquisition(code, sample_rate=4e6, chip_rate=1e6, cn0_dbhz=60.0,
...                 doppler_uncertainty=40e3, code_only_epochs=7)
>>> blk = np.tile(np.roll(s0, 17), b.coherent_bins).astype(np.complex64)
>>> _ = b.push(blk)
>>> raw = np.empty(b.coherent_bins * b.code_bins, dtype=np.complex64)
>>> b.block_raw(raw) == raw.size
True
>>> bool(np.array_equal(raw, blk))      # the samples as pushed
True
```
 





        

<hr>



### function dp\_correlator\_build\_handoff 

_Convert one_ [_**dp\_correlator\_push()**_](correlator__core_8h.md#function-dp_correlator_push) _hit into a wire-ready hand-off record._
```C++
void dp_correlator_build_handoff (
    const dp_correlator_state_t * state,
    const dp_correlator_hit_t * hit,
    size_t code_len,
    dp_correlator_handoff_t * out
) 
```



Two convention inversions live here, ported verbatim from `dsss_receiver_core.c`'s own (pre-existing, now-shared) handoff logic:



* **Chip phase**: `hit's` `col` is a correlation LAG (0 … lag\_bins-1); a code-tracking loop's `init_chip` wants the code's own instantaneous phase instead — the mirror-image inversion `phase = fmod(code_len - col * code_len / lag_bins, code_len)`, folded non-negative.
* **Doppler**: `state` is assumed built via [**dp\_correlator\_create\_continuous()**](correlator__core_8h.md#function-dp_correlator_create_continuous) (`window_bins` tiles of `coherent_bins` each, the only mode this function supports), so `hit`'s `row` indexes the uniform grid of `window_bins * coherent_bins` bins, mapped to a signed bin by `dp_fftfreq_index()` — the SAME helper the search uses — and scaled by `state->search->doppler_res_hz`.
* **The dwell's dilation** (doppler#1254): a hit is decided on a non-coherent sum over `n_noncoh` looks, and the code phase it reports is that sum's peak  the phase at the MIDDLE of the dwell, not at its end, when the chip clock is dilated by the same Doppler the hit reports (a physically-coupled carrier, `doppler_hz / carrier_freq_hz` chips per chip). The seed a code loop wants is the phase at the next sample, so with the carrier set ([**dp\_correlator\_set\_carrier\_freq\_hz()**](correlator__core_8h.md#function-dp_correlator_set_carrier_freq_hz)) the phase is advanced by the drift over HALF the dwell, `doppler_hz_est / carrier_freq_hz * n_noncoh * coherent_bins * code_len / 2` chips (a coherent block's epochs are aligned to its middle by the same setting, so the block's peak is its middle too). At the specification's 20 ppm the continuous engine's dwell at 45 dB-Hz is 15 epochs (0.15 chip, inside any code loop's pull-in) and at the 40 dB-Hz floor 88 epochs  0.9 chip, measured directly, past the refine Dll's; without this the floor's hand-offs never refined. Uncoupled (0.0): no advance. 

**Parameters:**


  * `state` The engine `hit` came from (non-NULL, built via [**dp\_correlator\_create\_continuous()**](correlator__core_8h.md#function-dp_correlator_create_continuous)). 
  * `hit` One hit from [**dp\_correlator\_push()**](correlator__core_8h.md#function-dp_correlator_push) (non-NULL). 
  * `code_len` Spreading-code length (chips): a hit's lag, in columns of the engine's `lag_bins`-column period, becomes a phase in these units (`code_len / lag_bins` chips a column). For a code engine it is the code's length; a burst engine built from a code held k samples a chip passes the code's length too. 
  * `out` Written on return (non-NULL). 






        

<hr>



### function dp\_correlator\_carrier\_freq\_ok 

_Whether_ `carrier_freq_hz` _is a carrier the code can be coupled to at_`sample_rate_hz:` _0 (uncoupled), or finite and above half the sample rate._
```C++
int dp_correlator_carrier_freq_ok (
    double carrier_freq_hz,
    double sample_rate_hz
) 
```



Every coupled stage divides a Doppler by the carrier: the hand-off's dilation, a coherent block's epoch alignment, a code loop's rate aid. A seed a receiver accepts has a Doppler below half its sample rate ([**dp\_async\_dsss\_receiver\_seed\_ok()**](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_seed_ok)), so a carrier above that keeps \|doppler / carrier\| &lt; 1, the physical v/c, when the hand-off is taken from a seed. The acquisition search itself does not yet bound the Doppler it reports to that range (doppler#2112 item 7); a hit past it is refused as a seed rather than used, so the bound holds at the receiver even where the search does not enforce it. A tiny carrier sent doppler / carrier to inf, so the seed phase came out NaN, and the receiver aborted building its code loop from it (doppler#2103). The one rule for every site that takes the divisor: [**dp\_correlator\_set\_carrier\_freq\_hz()**](correlator__core_8h.md#function-dp_correlator_set_carrier_freq_hz) and the DSSS receivers' creates.




**Parameters:**


* `carrier_freq_hz` Candidate carrier, Hz; 0 means uncoupled. 
* `sample_rate_hz` Sample rate of the stream, Hz. 



**Returns:**

1 if the carrier is taken, 0 otherwise (a NaN is not). 





        

<hr>



### function dp\_correlator\_cell\_corr 

_One epoch's correlation against the replica at ONE code phase and ONE frequency: the engine's surface, evaluated at a single cell, on raw samples._ 
```C++
double _Complex dp_correlator_cell_corr (
    const dp_correlator_state_t * state,
    const float _Complex * x,
    size_t col,
    double f_hz,
    double t0
) 
```



`sum_m x[m] * conj(ref[(m - col) mod nx]) * exp(-j 2 pi f (t0 + m)/fs)` over the `code_bins` samples of `x`. The mixer phase is referenced to `t0`, so epochs evaluated at consecutive `t0` combine COHERENTLY: sum the returned values across epochs and the result is a depth-`len` cell of the slow-time surface at `f_hz`, with no intra-epoch rotation loss.


It is what a caller does once acquisition has SETTLED the code phase and the Doppler: evaluate the same statistic elsewhere without searching the grid again. The engine's tile-alias check uses it; burst\_capture's refine uses it to score each candidate repetition (doppler#1502).




**Parameters:**


* `state` The engine whose replica, `code_bins` and `fs` are used. 
* `x` One epoch, `code_bins` samples. 
* `col` Code phase, samples, in [0, code\_bins). 
* `f_hz` Mixer frequency, Hz. 
* `t0` Time of the first sample of `x`, in samples, on the caller's reference. 



**Returns:**

The complex correlation. 





        

<hr>



### function dp\_correlator\_cell\_corr\_grid 

[_**dp\_correlator\_cell\_corr()**_](correlator__core_8h.md#function-dp_correlator_cell_corr) _at many frequencies over consecutive epochs, in one pass: the cells a caller scores once the code phase is settled and only the Doppler is left to search._
```C++
void dp_correlator_cell_corr_grid (
    const dp_correlator_state_t * state,
    const float _Complex * x,
    size_t n_epochs,
    size_t col,
    const double * f_hz,
    size_t n_f,
    double t0,
    double _Complex * out
) 
```



Element `e * n_f + j` of `out` is [**dp\_correlator\_cell\_corr()**](correlator__core_8h.md#function-dp_correlator_cell_corr) of epoch `e` (`x + e*code_bins`, at `t0 + e*code_bins`) at the j-th frequency, to within 1e-5 of the epoch's `sum |x * ref|`, the most either can reach (measured 2.9e-6 at a 510-sample epoch): a coherent peak 100 dB above the difference.


How: evaluated one cell at a time, every frequency repeats the whole despread-and-mix of the epoch. Here the replica is multiplied once per call by a mixer at the frequencies' centre, so each sample costs one multiply, summed over B equal blocks per epoch with the first and second moments of the time within each block. What is left of a frequency is a residual of at most `r` cycles per epoch either side of the centre; B = ceil(48 r) blocks keep it turning under 1/96 of a cycle either side of a block's centre, and it is applied per block from the moments as a second-order expansion, whose remainder is the error above. So the cost per epoch is `code_bins + 3 * B * n_f` against `n_f * code_bins`, and burst\_capture's refine, which scores every Doppler cell of every candidate period, is the reason it exists (doppler#1538).


Frequencies spread more than one cycle per epoch either side of their centre fall back to [**dp\_correlator\_cell\_corr()**](correlator__core_8h.md#function-dp_correlator_cell_corr) per cell.




**Parameters:**


* `state` The engine whose replica, `code_bins` and `fs` are used. 
* `x` `n_epochs * code_bins` contiguous samples. 
* `n_epochs` Epochs to evaluate. 
* `col` Code phase, samples, in [0, code\_bins). 
* `f_hz` `n_f` mixer frequencies, Hz, in any order. 
* `n_f` Number of frequencies. 
* `t0` Time of the first sample of `x`, in samples, on the caller's reference. 
* `out` Written with `n_epochs * n_f` correlations, epoch-major. 




        

<hr>



### function dp\_correlator\_configure\_search\_raw 

_Pin the search grid directly, bypassing both auto-sizing searches — the advanced escape hatch (mirrors Dll's/Costas's configure\_lock\_raw())._ 
```C++
int dp_correlator_configure_search_raw (
    dp_correlator_state_t * state,
    size_t doppler_bins,
    size_t n_noncoh
) 
```



Resizes every buffer/plan that depends on the grid (the slow-time FFT, the code correlator, the reference, and every per-frame scratch buffer), re-derives the threshold ladder for the pinned grid from the same physics [**dp\_correlator\_create\_burst()**](correlator__core_8h.md#function-dp_correlator_create_burst)/dp\_correlator\_create\_continuous() used, and clears in-flight accumulation (ring contents, the non-coherent power accumulator, dwell bookkeeping) — call between push() calls, never a substitute for one.




**Parameters:**


* `state` Allocated engine (non-NULL). 
* `doppler_bins` Coherent depth to pin, in `[1, reps]`. 
* `n_noncoh` Non-coherent look count to pin, in `[1, DP_CORRELATOR_N_NONCOH_SAFETY_CEILING]`. 



**Returns:**

0 on success, -1 if either argument is out of range (or `n_noncoh` &gt; 1 on an engine with rate hypotheses, which do not compose with non-coherent looks), or an allocation fails (the engine is left usable at its prior grid on failure). 
```C++
>>> import numpy as np
>>> from doppler.acquire import Acquisition
>>> from doppler.wfm import PN, mls_poly
>>> code = np.asarray(PN(poly=mls_poly(5), seed=1,
...                      length=5).generate(31)).astype(np.uint8)
>>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), 4).astype(
...     np.complex64)
>>> a = Acquisition(code, sample_rate=4e6, chip_rate=1e6, cn0_dbhz=50.0)
>>> a.configure_search_raw(doppler_bins=1, n_noncoh=4)  # pin the grid
>>> a.doppler_bins, a.n_noncoh
(1, 4)
>>> burst = np.tile(np.roll(s0, 17), 4).astype(np.complex64)
>>> a.push(burst)[0][:2]      # detects at the pinned grid
(0, 18)
```
 





        

<hr>



### function dp\_correlator\_consumed 

_Input samples the last_ [_**dp\_correlator\_push()**_](correlator__core_8h.md#function-dp_correlator_push) _took._
```C++
size_t dp_correlator_consumed (
    const dp_correlator_state_t * state
) 
```



Equal to its `n_in` unless `result` filled up; then the caller resumes at in + consumed. 0 after create, reset, set\_ref and set\_state, and always 0 for a map flavor.




**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Samples of the last push's input consumed (including any kept as the carry). 





        

<hr>



### function dp\_correlator\_create 

_Build the flavor the knobs name: the one constructor the Python class_ `Correlator` _calls._
```C++
dp_correlator_state_t * dp_correlator_create (
    const float _Complex * ref,
    size_t ny,
    size_t nx,
    dp_correlator_framing_t framing,
    size_t doppler_rows,
    int detect,
    size_t dwell,
    size_t ny_out,
    size_t nx_out,
    int col_out,
    float threshold,
    size_t noise_lo,
    size_t noise_hi,
    int noise_mode,
    double sample_rate,
    double chip_rate,
    double symbol_rate,
    size_t reps,
    double cn0_dbhz,
    double doppler_uncertainty,
    double pfa,
    double pd,
    double doppler_rate,
    size_t rate_hypotheses,
    size_t code_only_epochs,
    int nthreads,
    const char ** why
) 
```



A C caller names the flavor it wants and calls that flavor's own constructor; this one is the binding's entry point (docs/design/correlator-python.md section 3). It reads the knobs, picks the flavor, and calls that constructor with them:



|framing   |ref rows   |doppler\_rows   |detect   |flavor    |
|-----|-----|-----|-----|-----|
|`NONE`   |1   |0   |0   |[**dp\_correlator\_create\_1d()**](correlator__core_8h.md#function-dp_correlator_create_1d)    |
|`NONE`   |1   |0   |1   |[**dp\_correlator\_create\_detector\_1d()**](correlator__core_8h.md#function-dp_correlator_create_detector_1d)    |
|`NONE`   |&gt; 1   |0   |0   |[**dp\_correlator\_create\_2d()**](correlator__core_8h.md#function-dp_correlator_create_2d)    |
|`NONE`   |&gt; 1   |0   |1   |[**dp\_correlator\_create\_detector\_2d()**](correlator__core_8h.md#function-dp_correlator_create_detector_2d)    |
|`NONE`   |1   |&gt; 0   |0   |[**dp\_correlator\_create\_delay\_doppler()**](correlator__core_8h.md#function-dp_correlator_create_delay_doppler)    |
|`CONTINUOUS`   |1   |0   |any   |[**dp\_correlator\_create\_continuous()**](correlator__core_8h.md#function-dp_correlator_create_continuous)    |
|`BURST`   |1   |0   |any   |[**dp\_correlator\_create\_burst()**](correlator__core_8h.md#function-dp_correlator_create_burst)   |






A one-row reference is the 1-D flavor: [**dp\_correlator\_create\_1d()**](correlator__core_8h.md#function-dp_correlator_create_1d) is [**dp\_correlator\_create\_2d()**](correlator__core_8h.md#function-dp_correlator_create_2d) at one row, and `nx_out` is its padded length.


**A value the flavor would not honor is refused**, never ignored: a knob the flavor does not read must hold its default, which is the value named with it below. A search honors `detect` = 1, so it may be either. The refusal returns NULL and points `why` at a sentence that names the knob.


The continuous search's reference is NRZ chips as samples: exactly +1 (chip 0) or -1 (chip 1), with no imaginary part, the mapping of [**dp\_bin\_to\_nrz()**](cvt__core_8h.md#function-dp_bin_to_nrz); any other sample is refused, so a shaped or noisy reference is never quantized silently.




**Parameters:**


* `ref` Row-major `ny` x `nx` reference, CF32; borrowed. 
* `ny` Rows of `ref` (1 for a 1-D reference). 
* `nx` Samples a row. 
* `framing` Maps and detections, or a search; default NONE. 
* `doppler_rows` Doppler rows of the delay-Doppler map, the code periods one frame holds; 0 (default) for no map. 
* `detect` 1 for detections from push(); default 0 (maps). 
* `dwell` Frames integrated a dump; default 1. 
* `ny_out` Padded Doppler rows; 0 (default) for native. 
* `nx_out` Padded lags; 0 (default) for native. 
* `col_out` The one lag to emit; -1 (default) for every lag. 
* `threshold` Detection threshold on peak / noise; default 0. 
* `noise_lo` First noise bin; default 0. 
* `noise_hi` Last noise bin; default SIZE\_MAX, the last bin. 
* `noise_mode` The noise statistic, a [**det\_noise\_mode\_t**](correlator__core_8h.md#enum-det_noise_mode_t); default DET\_NOISE\_MEAN. 
* `sample_rate` Samples a second; 0 (default) is not given, which a search refuses. 
* `chip_rate` Chips a second; 0 (default) is not given, which the continuous search refuses. 
* `symbol_rate` Data symbols a second, read by the continuous search for a diagnostic only; 0 (default) is no known data clock. 
* `reps` Preamble repetitions a burst; default 1. 
* `cn0_dbhz` Design-point C/N0, dB-Hz; default DP\_CORRELATOR\_CN0\_NONE (NaN, none). The continuous search needs a finite one. 
* `doppler_uncertainty` Half-width of the Doppler search, Hz; default 0. 
* `pfa` False-alarm probability a search; default 1e-3. 
* `pd` Detection probability at the design point; default 0.9. 
* `doppler_rate` Doppler rate bound, Hz/s; default 0. 
* `rate_hypotheses` Rate hypotheses of the burst search; default 1. 
* `code_only_epochs` Code-only epochs of the continuous search; default 1. 
* `nthreads` Worker threads of a map or detector; default 1. 
* `why` Where a refusal's sentence goes; may be NULL. A static string, never freed. 



**Returns:**

The correlator, or NULL: refused (`why` says which knob) or out of memory.



```C++
#include "doppler/correlator/correlator_core.h"
#include <stdint.h>
#include <stdio.h>

int main (void)
{
  float _Complex ref[4] = { 1, 0, 0, 0 };
  const char *why = NULL;
  dp_correlator_state_t *c = dp_correlator_create (
      ref, 1, 4, DP_CORRELATOR_FRAMING_NONE, 0, 1, 1, 0, 0, -1, 0.0f, 0,
      SIZE_MAX, DET_NOISE_MEAN, 0.0, 0.0, 0.0, 1, DP_CORRELATOR_CN0_NONE,
      0.0, 1e-3, 0.9, 0.0, 1, 1, 1, &why);
  if (!c || dp_correlator_flavor (c) != DP_CORRELATOR_DETECTOR_1D)
    return 1;
  dp_correlator_destroy (c);
  c = dp_correlator_create (
      ref, 1, 4, DP_CORRELATOR_FRAMING_BURST, 0, 0, 1, 0, 0, -1, 0.0f, 0,
      SIZE_MAX, DET_NOISE_MEAN, 0.0, 0.0, 0.0, 1, DP_CORRELATOR_CN0_NONE,
      0.0, 1e-3, 0.9, 0.0, 1, 1, 1, &why);
  printf ("%s\n", why);
  return c == NULL ? 0 : 1;
}
```
 


        

<hr>



### function dp\_correlator\_create\_1d 

_Allocate a 1-D FFT correlator with coherent integrate-and-dump. Pre-computes conj(FFT(ref)) once at construction so each execute() call costs only two FFTs and n complex multiplies._ `ref` _may be freed after this returns. With_`dwell` _== 1 every call produces output; with larger values the accumulator absorbs_`dwell` _frames before dumping._
```C++
dp_correlator_state_t * dp_correlator_create_1d (
    const float _Complex * ref,
    size_t ref_len,
    size_t dwell,
    int nthreads,
    size_t n_out
) 
```



It is [**dp\_correlator\_create\_2d()**](correlator__core_8h.md#function-dp_correlator_create_2d) at ny = 1, nx = `ref_len:` the single-row path, so the same bits (test\_corr\_equiv). Its frame and its map are both 1-D: execute() takes `ref_len` samples and a dump writes n\_out.




**Parameters:**


* `ref` Reference signal, CF32, length `ref_len`. 
* `ref_len` Reference / FFT length in samples. 
* `dwell` Integration depth; must be &gt;= 1 (0 is refused). Pass 1 for immediate output on every call. 
* `nthreads` Accepted for API compatibility; ignored. 
* `n_out` Inverse/output length; 0 =&gt; native (n). Must be &gt;= n. A larger value zero-pads the cross-spectrum before the inverse, returning the band-limited (Dirichlet) interpolation of the correlation on a finer length-n\_out grid — same peak, sub-bin lag resolution. Native is bit-exact and allocates no extra buffer. 



**Returns:**

Heap-allocated state (free with [**dp\_correlator\_destroy()**](correlator__core_8h.md#function-dp_correlator_destroy)), or NULL on a dwell of 0, an `n_out` below `ref_len`, or allocation failure. 
```C++
>>> from doppler.spectral import Corr
>>> import numpy as np
>>> ref = np.zeros(4, dtype=np.complex64); ref[0] = 1.0
>>> corr = Corr(ref=ref, dwell=1, nthreads=1)
>>> corr.n, corr.dwell, corr.count
(4, 1, 0)
```
 





        

<hr>



### function dp\_correlator\_create\_2d 

_Allocate a 2-D FFT correlator with coherent integrate-and-dump. Two-dimensional extension of_ [_**dp\_correlator\_create\_1d()**_](correlator__core_8h.md#function-dp_correlator_create_1d) _. The reference is a flat row-major ny×nx CF32 array; its conjugate spectrum is pre-computed once so each execute() call costs two 2-D FFTs plus ny\*nx complex multiplies. The Python wrapper requires_`ref` _to be a 2-D ndarray with shape (ny, nx); it passes a flat view to C._
```C++
dp_correlator_state_t * dp_correlator_create_2d (
    const float _Complex * ref,
    size_t ny,
    size_t nx,
    size_t dwell,
    int nthreads,
    size_t ny_out,
    size_t nx_out,
    int col_out
) 
```





**Parameters:**


* `ref` Reference image, 2-D (ny, nx) CF32 ndarray in Python. 
* `ny` Number of rows in the reference and input frames. 
* `nx` Number of columns in the reference and input frames. 
* `dwell` Integration depth; must be &gt;= 1. 
* `nthreads` Workers for the single-row-reference fast path's per-row stage: 1 runs serially (no threads), N runs N workers (the caller included), &lt;= 0 uses every online core. The count is capped at `ny`, one row per worker at most. The output is bit-identical whatever the count. The general 2-D path and the `col_out` path run serially and ignore it. 
* `ny_out` Inverse/output rows; 0 =&gt; native (ny). Must be &gt;= ny. A larger output zero-pads the cross-spectrum before the inverse, returning the band-limited (Dirichlet) interpolation of the correlation on a finer (ny\_out, nx\_out) grid — same peak, sub-bin resolution. Native is bit-exact and allocates no extra buffers. 
* `nx_out` Inverse/output columns; 0 =&gt; native (nx). Must be &gt;= nx. 
* `col_out` Emit ONLY this correlation lag, or &lt; 0 for the whole map. When set, a dump writes `ny` values  one per row  instead of ny\*nx\_out, computed as the time-domain sum over p of `conj(ref(p)) * row((p + col_out) mod nx)`, which is what R(i, col\_out) expands to once the 1/nx cancels. No transform runs in either direction, so the cost per row is O(nx) rather than O(nx log nx). Requires the single-row-reference fast path (a caller that knows its lag is by construction correlating against a code replica) and the NATIVE output grid: an interpolated column is a fractional lag, which no time-domain sum produces. create() returns NULL if `col_out` &gt;= 0 with a reference the fast path rejects, with `ny_out` or `nx_out` decoupled, or with a lag outside 0 .. nx-1. 



**Returns:**

Heap-allocated state, or NULL on failure. 
```C++
>>> from doppler.spectral import Corr2D
>>> import numpy as np
>>> ref = np.zeros((4, 4), dtype=np.complex64); ref[0, 0] = 1.0
>>> c = Corr2D(ref=ref, dwell=1, nthreads=1)
>>> c.ny, c.nx, c.dwell, c.count
(4, 4, 1, 0)
```
 





        

<hr>



### function dp\_correlator\_create\_burst 

_Create a burst-mode acquisition engine for a repeated preamble: coherent multi-repetition combining, up to_ `reps` _deep._
```C++
dp_correlator_state_t * dp_correlator_create_burst (
    const float _Complex * tmpl,
    size_t n,
    size_t reps,
    double fs,
    double cn0_dbhz,
    double doppler_uncertainty,
    double pfa,
    double pd,
    int noise_mode,
    double doppler_rate,
    size_t rate_hypotheses
) 
```



Takes the preamble as its SAMPLES: one period of `n` complex samples at `fs`, repeated up to `reps` times. A PN code is one such preamble  pass its chips mapped by [**dp\_bin\_to\_nrz()**](cvt__core_8h.md#function-dp_bin_to_nrz) and held `spc` samples each, at `fs = chip_rate * spc`. The engine sees one chip = one sample: `sf = n`, `spc = 1`, `chip_rate = fs`, the native Doppler span is `+/- fs/(2n)`, and `col` is the delay into the repetition, in samples  for a preamble whose ambiguity function is a ridge (Zadoff-Chu: one native bin of Doppler moves the peak `u^-1 mod n` samples), the delay plus that shift whenever the Doppler is not resolved before correlation. It converts `cn0_dbhz` to a per-sample amplitude SNR (snr = sqrt(10^(cn0\_dbhz/10) / fs)), and picks the _smallest_ coherent depth `coherent_bins` in `[1, reps]` whose burst Pd `pd_burst` meets `pd` at the Sidak threshold (minimum latency for a strong signal). `pd_burst` treats the preamble as exactly `reps` periods at a uniform offset against the stream-aligned dwells, and credits every dwell it spans (doppler#1498). If no depth meets `pd` the engine takes the one with the most burst Pd and is `underpowered`; it does NOT add non-coherent looks. A burst has one frame of preamble, so looks beyond it add noise to the statistic and move the hit  `samples_consumed` is stamped at the end of the LAST accumulated look, so a consumer resolving the preamble's position sees an anchor up to n\_noncoh\*coherent\_bins periods late (doppler#1181). Intended for an unmodulated burst or preamble window  a continuous, data-modulated signal should use [**dp\_correlator\_create\_continuous()**](correlator__core_8h.md#function-dp_correlator_create_continuous) instead (coherent combining under continuous data is a structural aliasing mislock, not a tunable SNR trade-off  see the file doc comment).


`cn0_dbhz` is the DESIGN (minimum) C/N0 and is optional: NaN ([**DP\_CORRELATOR\_CN0\_NONE**](correlator__core_8h.md#define-dp_correlator_cn0_none)) means none was given, and the engine then integrates the whole preamble (`coherent_bins = reps`; 1 in a tiled search, where a depth multiplies the buffers and the latency and only a stated C/N0 asks for one) with the threshold set by `pfa` alone. `pd` is a sizing target only when a design C/N0 is given; without one `pd_predicted` and `pd_burst` are NAN and `underpowered` is never set.


A tighter `doppler_uncertainty` narrows the scanned Doppler band, lowering the per-cell threshold (more sensitive). When `doppler_uncertainty` exceeds the native span `fs/(2n)`, engages the wideband window-tiling mechanism (see the file doc comment): `2*ceil((du - span)/(2*span)) + 1` windows, with a coherent depth chosen inside each (1 with no design C/N0). Use [**dp\_correlator\_configure\_search\_raw()**](correlator__core_8h.md#function-dp_correlator_configure_search_raw) to pin the grid directly instead of relying on this auto-sizer.


What a code once knew analytically, the preamble's own correlation gives numerically, from one FFT at construction ([**dp\_correlator\_shape\_t**](structdp__correlator__shape__t.md)):



* the peak zone (the twin rule's reach and the exclusion around each listed peak) is the autocorrelation's first null, the lag of the first local minimum of \|R(k)\|  one chip of samples for a code;
* the delay straddle the Pd model averages is the band-limited autocorrelation over a half-sample offset.




For a PN code the numeric model is the more accurate one: against a rectangular-chip signal it stays conservative (never optimistic) and sits 0.01-0.08 below the measured Pd at spc 2-4, where the analytic triangle sat 0.03-0.21 below (doppler#1470).


The within-repetition rotation loss keeps the engine's `sinc` model: it is the zero-delay cut of the ambiguity function, which a periodic preamble's envelope does not move  measured within 0.007 dB of sinc for RRC-shaped QPSK at 4.7 dB peak-to-average.


`tmpl` is scaled to unit RMS (a copy  the caller's buffer is only read), so `peak_mag` and `noise_est` come out in the signal's units whatever the preamble's amplitude. C/N0 is the preamble's mean power.


**What hypotheses change**, when `rate_hypotheses` is `n > 1`:
* **Depth.** The drift bound is applied to the RESIDUAL rate, the farthest a true rate can sit from a hypothesis, `R/(n - 1)`, so `coherent_bins` can grow by up to `sqrt(n - 1)` at a fixed span.
* **False alarms.** A dwell now offers `n` looks at each cell: the per-cell Pfa is split over `n` times the cells the dwell's maximum ranges over (every searched row of the Doppler-interpolated surface, times `code_bins`), and the gate is the larger count's. Each hypothesis is CFAR-tested against its own surface and the dwell takes the strongest; [**dp\_correlator\_hit\_t**](structdp__correlator__hit__t.md):: doppler\_rate\_hz\_s names the winner.
* **Cost.** A dwell does `n` slow-time passes (and a recompute of the winner when it was not the last): about `n` times the per-dwell cost.
* **Pd.** The model prices a rate ON a hypothesis. Between two, the coherent gain falls with the residual; the depth bound limits that to half a slow-time row, and validate\_acq\_rate\_pd measures it (a rate half a spacing from every hypothesis, the worst case, against a rate drawn uniformly).




The count is config, not running state, and it is a reject key of the state blob: a blob from an engine with another count is refused, not reinterpreted.




**Parameters:**


* `tmpl` One period of the preamble, `n` samples; not all zero, every sample finite. 
* `n` Samples per repetition (&gt;= 1). 
* `reps` Max coherent repetitions, the coherence ceiling (&gt;= 1). 
* `fs` Sample rate in Hz (&gt; 0); 1 for normalized units. 
* `cn0_dbhz` Design carrier-to-noise density in dB-Hz, of the preamble's mean power: any finite value (at fs = 1 the per-sample SNR, usually negative), or [**DP\_CORRELATOR\_CN0\_NONE**](correlator__core_8h.md#define-dp_correlator_cn0_none) (NaN) for no design point  size for the whole preamble. 
* `doppler_uncertainty` One-sided Doppler search half-range in Hz; 0 uses the full native span +/- fs/(2n). A value greater than the native span engages wideband mode (see the file doc comment above): the uncertainty is tiled with parallel frequency-window hypotheses and a coherent depth is chosen inside every tile (1 with no design C/N0); the surface is `window_bins * coherent_bins` rows. 
* `pfa` Target system (max-of-N) false-alarm probability (0,1). 
* `pd` Target detection probability (0,1); a sizing target only when `cn0_dbhz` is given. 
* `noise_mode` CFAR mode index: 0=mean, 1=median, 2=min, 3=max. 
* `doppler_rate` Doppler rate in Hz/s (&gt;= 0) the coherent depth is bounded against: at most `f_epoch/sqrt(2*doppler_rate)` repetitions (`f_epoch = fs/n`, the repetition rate), so the carrier's drift over one block stays inside half a slow-time row (doppler#1482). 0 is no bound: the depth is sized up to `reps`. With `rate_hypotheses` &gt; 1 it is the SPAN of the rates searched, below. 
* `rate_hypotheses` Doppler-rate hypotheses to search (0, or &gt;= 1 and at most [**DP\_CORRELATOR\_RATE\_HYPS\_MAX**](correlator__core_8h.md#define-dp_correlator_rate_hyps_max)); 1 searches none and `doppler_rate` only caps the depth. Above 1 the engine COMPENSATES the rate: `doppler_rate` is the span `[-R, +R]` and the coherent block is searched at `rate_hypotheses` rates evenly spaced across it, `a_h = -R + 2 R h / (n - 1)`. For each the slow-time axis is multiplied by `exp(-j pi a_h t^2)`, `t` the epoch's time from the block's middle, before the slow-time transform  so the Doppler an engine reports is the carrier's at the block's middle, where a hand-off wants it, and a signal whose rate sits within half a spacing of a hypothesis integrates as a still one (see below). A block of one or two epochs cannot see a rate  every hypothesis is then the same surface  so an engine sized to D &lt;= 2 searches ONE look (the gate of an engine without hypotheses), runs once, and reports a rate of 0. Needs a non-zero `doppler_rate:` it is the span. **0 is AUTO**: the engine picks the smallest `n` whose residual `R/(n - 1)` no longer limits the depth the sizer would choose with no drift bound at all (priced at that `n`), or [**DP\_CORRELATOR\_RATE\_HYPS\_MAX**](correlator__core_8h.md#define-dp_correlator_rate_hyps_max) if none does; the choice is the engine's `rate_hyps`. Burst only, and needs a non-zero `doppler_rate` like any `n > 1`.



**Returns:**

Heap-allocated state, or NULL on bad arguments (including a `rate_hypotheses` above DP\_CORRELATOR\_RATE\_HYPS\_MAX, or 0 or above 1 with `doppler_rate == 0`) / allocation failure. 
```C++
// 127-sample Zadoff-Chu preamble at 1 MS/s, up to 8 repetitions
float _Complex zc[127];
for (int k = 0; k < 127; k++)
  zc[k] = cexpf (-I * (float)(M_PI * 5.0 * k * (k + 1) / 127.0));
dp_correlator_state_t *a = dp_correlator_create_burst (zc, 127, 8, 1.0e6, 50.0, 0.0, 1e-3,
                                   0.9, 0, 0.0, 1);
dp_correlator_destroy (a);
```
 





        

<hr>



### function dp\_correlator\_create\_continuous 

_Create a continuous-mode acquisition engine: always wideband window-tiling, allowing a block-coherent depth inside the tiles to accommodate waveforms with code-only windows._ 
```C++
dp_correlator_state_t * dp_correlator_create_continuous (
    const uint8_t * code,
    size_t code_len,
    double sample_rate,
    double chip_rate,
    double symbol_rate,
    double cn0_dbhz,
    double doppler_uncertainty,
    double pfa,
    double pd,
    int noise_mode,
    size_t code_only_epochs,
    double doppler_rate
) 
```



Builds the single-row oversampled BPSK reference from `code`, infers sf = `code_len`, converts `cn0_dbhz` to a per-sample amplitude SNR, and ALWAYS tiles `window_bins = 2*ceil((doppler_uncertainty - span) / (2*span)) + 1` parallel frequency-window hypotheses, with `span = chip_rate/(2*sf)` one native span (see the file doc comment's "Wideband
window-tiling mode")  unconditionally, so `window_bins = 1` when `doppler_uncertainty` is no wider than one native span. A continuous, data-modulated signal's own bit transitions make coherent multi-epoch combining across DATA a structural aliasing mislock (see docs/design/acq-surface.md §9), so the depth is bounded by what a waveform's code-only window holds: `coherent_bins = D = min((code_only_epochs+1)/2, f_epoch/sqrt(2*doppler_rate))`, at least 1, rounded down to a 7-smooth length ([**dp\_fft\_smooth\_floor()**](fft__core_8h.md#function-dp_fft_smooth_floor): 94 runs as 90), run in non-overlapping D-epoch blocks inside every tile (file doc, docs/design/acq-surface.md §4). With `code_only_epochs` = 1 it is 1 and the engine is exactly the epoch-by-epoch search. Sensitivity margin beyond the depth comes from auto-selected non-coherent looks over blocks (up to the internal [**DP\_CORRELATOR\_N\_NONCOH\_SAFETY\_CEILING**](correlator__core_8h.md#define-dp_correlator_n_noncoh_safety_ceiling)).




**Parameters:**


* `code` PN chips (0/1), length `code_len`. 
* `code_len` Number of chips supplied (= sf, the spreading factor). 
* `sample_rate` The rate of the samples pushed, Hz. One code period is then `n = sample_rate * code_len / chip_rate` samples, which must be a whole number of at least `code_len:` the search's transforms are n points long, so a smooth n (a power of two, [**dp\_correlator\_period\_samples()**](correlator__core_8h.md#function-dp_correlator_period_samples)) is what makes them fast. When n is a multiple of `code_len` each chip is held n / code\_len samples; otherwise each sample takes the chip its centre falls in, and the correlation shape is measured from that replica. 
* `chip_rate` Chip rate in Hz (&gt; 0). 
* `symbol_rate` Continuous data-symbol rate in Hz; &lt;= 0 means no known clock. Diagnostic only (exposed via [**dp\_correlator\_search\_t::epochs\_per\_symbol**](structdp__correlator__search.md#variable-epochs_per_symbol)), doesn't feed sizing: this engine never coherently combines regardless of the data-modulation clock. 
* `cn0_dbhz` Carrier-to-noise density in dB-Hz: any finite value. A continuous engine needs one  its non-coherent looks cannot be chosen without a target. 
* `doppler_uncertainty` One-sided Doppler search half-range in Hz; 0 uses the full native span +/- chip\_rate/(2\*sf) (still window-tiled, at window\_bins=1). 
* `pfa` Target system (max-of-N) false-alarm probability (0,1). 
* `pd` Target detection probability (0,1). 
* `noise_mode` CFAR mode index: 0=mean, 1=median, 2=min, 3=max. 
* `code_only_epochs` Whole code-only epochs a waveform's code-only window holds at any chip phase (docs/design/async-dsss-receiver.md §2.1: `floor(W_symbols * chips_per_symbol / sf) - 1`); 1 (&gt;= 1) means no window and a depth of 1. 
* `doppler_rate` Doppler rate in Hz/s the depth is bounded against (&gt;= 0); 0 leaves the window as the only bound. 



**Returns:**

Heap-allocated state, or NULL on bad arguments / allocation failure. 
```C++
>>> import numpy as np
>>> from doppler.acquire import Acquisition
>>> from doppler.wfm import PN, mls_poly
>>> code = np.asarray(PN(poly=mls_poly(5), seed=1,
...                      length=5).generate(31)).astype(np.uint8)
>>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), 4).astype(
...     np.complex64)
>>> burst = np.tile(np.roll(s0, 17), 23).astype(np.complex64)
>>> a = Acquisition(code, sample_rate=4e6, chip_rate=1e6, cn0_dbhz=50.0)
>>> a.push(burst)[0][:2]    # detects (Doppler-window bin, code phase)
(0, 18)
>>> (a.code_bins, a.lag_bins)  # 124 samples, read on 128 columns
(124, 128)
>>> round(18 * a.code_bins / a.lag_bins, 2)  # column 18, in samples
17.44
>>> a.coherent_bins            # no window given: one epoch
1
>>> b = Acquisition(code, sample_rate=4e6, chip_rate=1e6, cn0_dbhz=50.0,
...                 code_only_epochs=7)
>>> b.coherent_bins            # (7 + 1) // 2: a whole block fits
4
```
 





        

<hr>



### function dp\_correlator\_create\_delay\_doppler 

_Allocate the delay-Doppler flavor: a code correlator that maps a raw stream of_ `ny` _code periods onto (Doppler, delay)._
```C++
dp_correlator_state_t * dp_correlator_create_delay_doppler (
    const float _Complex * code,
    size_t code_len,
    size_t ny,
    size_t dwell,
    int nthreads,
    size_t ny_out
) 
```



Input to [**dp\_correlator\_execute()**](correlator__core_8h.md#function-dp_correlator_execute) is the raw stream  `ny` periods of `code_len` samples, back to back, no reshaping or reference padding by the caller. Each period (row) is correlated against `code` exactly as Corr2D's single-row fast path does (FFT, multiply by `conj(FFT(code))`, coherent `dwell` accumulation of the product spectrum, inverse scaled by `1/nx`), and on the dump the slow-time FFT runs down every delay column. The library's one delay-Doppler map computes it (docs/design/acq-surface.md §18) in the order its rule picks: here the rows first, each row inverted as it is accumulated, which is what makes the row stage fused. In numpy terms, for one frame `x` (dwell 1):  The output is `ny_out x nx` row-major: rows are Doppler bins, centered (zero Doppler on row `ny_out / 2`), columns are delay lags. A signal of amplitude `a` that is the code delayed by `d` samples and rotated by `f` cycles/sample peaks at column `d` and row `ny_out/2 + f / doppler_step`, where [**dp\_correlator\_state\_t::doppler\_step**](structdp__correlator__state__t.md#variable-doppler_step) is `1 / (ny_out * nx)` cycles/sample (times the sample rate for Hz). Its peak is `a * dwell * ny * sum(abs(code)**2)`: the 1/nx of the row inverse cancels the row correlation's own gain of nx, and the slow-time FFT is unnormalized, as numpy's is, so it sums the ny rows coherently.


`ny_out > ny` zero-pads the slow-time samples after the ny rows: the ny\_out-point DFT then samples the same Doppler response on a grid ny\_out/ny times finer (band-limited interpolation). The pad adds no energy, so the peak at a bin center is unchanged and a Doppler between native bins is found at its fractional row.


Both stages fan out over `nthreads` workers  the rows, then the columns  each worker on its own FFT plans, and the result is bit-identical for every thread count.




**Parameters:**


* `code` The code replica, `code_len` samples (one period). 
* `code_len` Samples per period, nx; must be &gt;= 1. 
* `ny` Periods per frame, the slow-time length; must be &gt;= 1. 
* `dwell` Frames coherently accumulated per dump; must be &gt;= 1. 
* `nthreads` Workers: 1 serial, N for N (the caller included), &lt;= 0 every online core; capped at the most chunks a stage can use, `max(ny, ceil(code_len / 16))`. 
* `ny_out` Doppler bins in the map; 0 means `ny`. Must be &gt;= `ny`. 



**Returns:**

Heap-allocated state (free with [**dp\_correlator\_destroy()**](correlator__core_8h.md#function-dp_correlator_destroy)), or NULL if `code` is NULL, `code_len` or `ny` is 0, `dwell` is 0, or `ny_out` is below `ny`. 
```C++
>>> import numpy as np
>>> from doppler.spectral import DelayDoppler
>>> rng = np.random.default_rng(0)
>>> ny, nx, d, k = 16, 64, 5, 3       # delay 5, Doppler 3 bins
>>> code = np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)
>>> n = np.arange(ny * nx)
>>> f = k / (ny * nx)                 # cycles/sample
>>> x = (np.tile(np.roll(code, d), ny)
...      * np.exp(2j * np.pi * f * n)).astype(np.complex64)
>>> dd = DelayDoppler(code, ny=ny, dwell=2)
>>> out = np.empty(dd.execute_max_out(), np.complex64)
>>> for _ in range(2):                # one preallocated map per dump
...     m = dd.execute(x, out=out)
>>> m is not None and m.base is out
True
>>> row, col = np.unravel_index(np.argmax(np.abs(out)), (ny, nx))
>>> int(row) - ny // 2, int(col)      # (Doppler bins, delay)
(3, 5)
>>> dd.doppler_step == 1 / (ny * nx)
True
```
 





        

<hr>



### function dp\_correlator\_create\_detector\_1d 

_Allocate a 1-D streaming detector: the 2-D detector at one row (docs/design/correlator.md section 2.1), so its hits are the 2-D one's at ny = 1, each with row 0 and the lag in col._ 
```C++
dp_correlator_state_t * dp_correlator_create_detector_1d (
    const float _Complex * ref,
    size_t ref_len,
    size_t dwell,
    size_t noise_lo,
    size_t noise_hi,
    det_noise_mode_t noise_mode,
    float threshold,
    int nthreads
) 
```



After every dump the peak-to-noise test statistic is compared against `threshold`, and a hit is emitted when it passes; 0.0 fires on every dump. The ring capacity is next\_pow\_two(max(n, 512)) complex samples.




**Parameters:**


* `ref` Reference signal, CF32 ndarray of length ref\_len. 
* `ref_len` Reference / FFT length in complex samples. 
* `dwell` Int-dump depth; must be &gt;= 1. 
* `noise_lo` Lower noise bin index (inclusive, 0-based). 
* `noise_hi` Upper noise bin index (inclusive, &lt; n). A value at or beyond the window clamps to n - 1, so the default sentinel selects the full window. 
* `noise_mode` Noise aggregation: "mean", "median", "min", or "max". 
* `threshold` Test-stat gate; 0.0 = always emit. 
* `nthreads` Accepted for API compatibility; ignored. 



**Returns:**

Heap-allocated state, or NULL when the arguments are refused (an empty reference, `dwell` 0) or the stream's ring cannot be made. Out of memory for the detection buffers aborts, by the library's dp\_x\* convention ([**clib\_common.h**](clib__common_8h.md)). 
```C++
>>> from doppler.spectral import CorrDetector
>>> import numpy as np
>>> ref = np.zeros(8, dtype=np.complex64); ref[0] = 1.0
>>> det = CorrDetector(ref=ref, dwell=1, noise_lo=1, noise_hi=7,
...                noise_mode="mean", threshold=0.0)
>>> det.n, det.dwell, det.ring_cap
(8, 1, 512)
```
 





        

<hr>



### function dp\_correlator\_create\_detector\_2d 

_Allocate a 2-D streaming detector: the 2-D correlator with the stream and detection layers._ 
```C++
dp_correlator_state_t * dp_correlator_create_detector_2d (
    const float _Complex * ref,
    size_t ny,
    size_t nx,
    size_t dwell,
    size_t noise_lo,
    size_t noise_hi,
    det_noise_mode_t noise_mode,
    float threshold,
    int nthreads
) 
```



Input frames are flat row-major CF32 arrays of length ny\*nx, cut from the stream by the ring's framer. On every dump the peak's flat index is decomposed into (row, col), and a [**dp\_correlator\_hit\_t**](structdp__correlator__hit__t.md) is emitted when test\_stat &gt; threshold. The Python class accepts a (ny, nx) CF32 ndarray for both `ref` and the push input. The ring capacity is next\_pow\_two(max(ny\*nx, 512)) complex samples.




**Parameters:**


* `ref` 2-D reference image, (ny, nx) CF32 ndarray in Python. 
* `ny` Number of rows in the reference and input frames. 
* `nx` Number of columns in the reference and input frames. 
* `dwell` Int-dump depth; must be &gt;= 1. 
* `noise_lo` Lower flat-index noise bin (inclusive, 0-based). 
* `noise_hi` Upper flat-index noise bin (inclusive, &lt; ny\*nx). A value at or beyond the window clamps to ny\*nx - 1, so the default sentinel selects the full window. 
* `noise_mode` Noise aggregation: "mean", "median", "min", or "max". 
* `threshold` Test-stat gate; 0.0 = always emit. 
* `nthreads` Workers for the single-row-reference fast path (1 serial, &lt;= 0 every online core); the general 2-D path ignores it. 



**Returns:**

Heap-allocated state, or NULL when the arguments are refused (an empty axis, `dwell` 0) or the stream's ring cannot be made. Out of memory for the detection buffers aborts, by the library's dp\_x\* convention ([**clib\_common.h**](clib__common_8h.md)), as the map's plans do. 
```C++
>>> from doppler.spectral import CorrDetector2D
>>> import numpy as np
>>> ref = np.zeros((4, 4), dtype=np.complex64); ref[0, 0] = 1.0
>>> det = CorrDetector2D(ref=ref, dwell=1, noise_lo=1, noise_hi=15,
...                  noise_mode="mean", threshold=0.0)
>>> det.ny, det.nx, det.n, det.dwell
(4, 4, 16, 1)
```
 





        

<hr>



### function dp\_correlator\_destroy 

_Destroy and free a correlator, any flavor._ 
```C++
void dp_correlator_destroy (
    dp_correlator_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function dp\_correlator\_dsl\_db 

_The search's Doppler-offset sidelobe level, dB: 20\*log10 of the largest lag of the code's periodic ambiguity outside the mainlobe, at any Doppler offset the grid's tiles can put between an emitter and a tile, relative to the emitter's own peak (_ [_**dp\_correlator\_shape\_t::dsl**_](structdp__correlator__shape__t.md#variable-dsl) _)._
```C++
double dp_correlator_dsl_db (
    dp_correlator_state_t * state
) 
```



What it predicts: one emitter's peak list carries peaks up to this far below its own, more than a mainlobe from its own lag, in the tiles it is not in  and they stay there dwell after dwell, being the code's, not the noise's. A list consumer that skips the strongest peak (a rank, an exclusion) must not take one for a second emitter. It is at least [**dp\_correlator\_psl\_db()**](correlator__core_8h.md#function-dp_correlator_psl_db), which is its zero-offset case, and equals it on a single-tile grid searched at one Doppler. -16.95 dB for the receivers' Gold-1023 search at two samples a chip over +-5 kHz (five 3 kHz tiles), against a -23.9 dB psl. Configuration, not state: fixed at construction, kept by a grid pinned later, and not serialized. Computed on the first call and cached in `state` (hence non-const).




**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Doppler-offset sidelobe level in dB, or -INFINITY when there is none.



```C++
#include "doppler/correlator/correlator_core.h"
#include "doppler/gold/gold_core.h"
#include <stdio.h>

int main (void)
{
  uint8_t          code[1023];
  dp_gold_state_t *g = dp_gold_create (934, 350, 567, 73, 10);
  dp_gold_generate (g, 1023, code, 1023); // the specification Gold-1023 code
  dp_gold_destroy (g);
  dp_correlator_state_t *a = dp_correlator_create_continuous (
      code, 1023, 2 * 3.069e6, 3.069e6, 2700.0, 47.0, 5000.0, 1e-3, 0.9,
      0, 1, 0.0);
  printf ("%.1f %.1f\n", dp_correlator_psl_db (a), dp_correlator_dsl_db (a));
  // -23.9 -16.9
  dp_correlator_destroy (a);
  return 0;
}
```
 


        

<hr>



### function dp\_correlator\_execute 

_Correlate one 2-D frame and optionally dump the coherent accumulator. Runs the 2-D pipeline: FFT2 → pointwise multiply with ref\_spec → accumulate the cross-spectrum; on dump, IFFT2 → normalise (÷ ny\*nx). Accumulating in the frequency domain and inverting once is exactly the per-frame inverse summed, by linearity of the IFFT — valid because the dwell is_ **coherent** _(a complex sum); a non-coherent (magnitude) integration could not defer the inverse._
```C++
size_t dp_correlator_execute (
    dp_correlator_state_t * state,
    const float _Complex * in,
    size_t n_in,
    float _Complex * out,
    size_t max_out
) 
```



**Layout.** A frame is ny\*nx complex samples in row-major order: row i is samples i\*nx .. (i+1)\*nx - 1. For Corr2D that is a (ny, nx) image; for DelayDoppler it is the raw stream, ny code periods of nx = code\_len samples back to back. The map a dump produces is flat, n\_out = ny\_out \* nx\_out values (ny values with `col_out`), also row-major: reshape it to (ny\_out, nx\_out). On a DelayDoppler, axis 0 is Doppler (centered, zero Doppler on row ny\_out / 2) and axis 1 is delay (the lag, in samples).


**Ownership.** In Python, a call that dumps returns a new array when no `out=` is given, and a view of `out=` when one is (a 1-D array holding at least n\_out values; it is written on a dump only, and the next dump overwrites it). A call that does not dump returns None and touches neither.




**Parameters:**


* `state` Allocated 2-D correlator (non-NULL). 
* `in` Input frame, flat row-major CF32, length ny\*nx. 
* `n_in` Number of input samples; must equal ny\*nx; else REFUSED. 
* `out` Output buffer for the correlation map (CF32, length n\_out: ny\_out\*nx\_out, or ny with `col_out`); written only on a dump call. 
* `max_out` Capacity of `out` in elements. Emission stops there, so the return value is the number actually written. 



**Returns:**

Python: the map on a dump, None otherwise; a frame that is not ny\*nx samples is REFUSED with nothing read, written or counted, as a ValueError (#1925). C: n\_out on a dump (or `max_out` if smaller), 0 otherwise, and SIZE\_MAX for the refused frame. 
```C++
>>> from doppler.spectral import Corr2D
>>> import numpy as np
>>> ref = np.zeros((2, 2), dtype=np.complex64); ref[0, 0] = 1.0
>>> c = Corr2D(ref=ref, dwell=2)
>>> x = np.ones((2, 2), dtype=np.complex64)
>>> c.execute(x) is None   # frame 1 — no dump
True
>>> c.execute(x).tolist()  # frame 2 — dump
[(2+0j), (2+0j), (2+0j), (2+0j)]
```
 





        

<hr>



### function dp\_correlator\_execute\_all 

_Every map of a batch, unreduced: row h of the (H, n\_out) result is the map of frame h (in C, map h is at_ `out + h * n_out` _)._
```C++
size_t dp_correlator_execute_all (
    dp_correlator_state_t * state,
    const float _Complex * in,
    size_t n_in,
    float _Complex * out,
    size_t max_out
) 
```



Python: `x` is the H frames back to back, a flat array of H \* ny \* nx samples (H &gt;= 1): frame h is the samples from h \* ny \* nx up to, not including, (h + 1) \* ny \* nx. With no `out=` each call returns a new array; with `out=` it returns a view of it, and the next call into the same `out=` overwrites that view. `out=` is a 2-D complex64 array of shape (rows, n\_out) with at least H rows (a shorter one is refused); the result is its first H rows. The object's dwell is untouched.




**Parameters:**


* `state` Must be non-NULL. 
* `in` H frames back to back (CF32). 
* `n_in` A nonzero multiple of ny \* nx. 
* `out` H \* n\_out values (CF32). 
* `max_out` Capacity of `out`; at least [**dp\_correlator\_execute\_all\_max\_out()**](correlator__core_8h.md#function-dp_correlator_execute_all_max_out). 



**Returns:**

Python: the (H, n\_out) array; a refused batch (empty, not a whole number of frames, or an `out=` with fewer than H rows) is a ValueError with nothing written. C: H \* n\_out, or SIZE\_MAX for the refused batch. 
```C++
>>> import numpy as np
>>> from doppler.spectral import DelayDoppler
>>> rng = np.random.default_rng(0)
>>> ny, nx = 8, 32
>>> code = np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)
>>> dd = DelayDoppler(code, ny=ny)
>>> frames = np.concatenate([np.tile(np.roll(code, d), ny)
...                          for d in (3, 11)])   # delays 3 and 11
>>> maps = dd.execute_all(frames)
>>> maps.shape
(2, 256)
>>> [int(np.argmax(np.abs(m))) % nx for m in maps]   # each map's delay
[3, 11]
>>> dd.count                    # the dwell never moved
0
```
 





        

<hr>



### function dp\_correlator\_execute\_all\_max\_out 

_Samples_ [_**dp\_correlator\_execute\_all()**_](correlator__core_8h.md#function-dp_correlator_execute_all) _writes for_`n_in` _input samples: H \* n\_out, H = n\_in / (ny \* nx)._
```C++
size_t dp_correlator_execute_all_max_out (
    const dp_correlator_state_t * state,
    size_t n_in
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `n_in` Input samples about to be passed. 



**Returns:**

The capacity `execute_all` needs for `n_in` samples: H \* n\_out values (C: SIZE\_MAX if that overflows). 
```C++
>>> import numpy as np
>>> from doppler.spectral import DelayDoppler
>>> rng = np.random.default_rng(0)
>>> ny, nx = 8, 32
>>> code = np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)
>>> dd = DelayDoppler(code, ny=ny)
>>> frames = np.concatenate([np.tile(np.roll(code, d), ny)
...                          for d in (3, 11)])   # delays 3 and 11
>>> dd.execute_all_max_out(len(frames))   # 2 maps of 8 x 32
512
```
 





        

<hr>



### function dp\_correlator\_execute\_coherent 

_The coherent sum of a batch's maps, inverted once._ 
```C++
size_t dp_correlator_execute_coherent (
    dp_correlator_state_t * state,
    const float _Complex * in,
    size_t n_in,
    float _Complex * out,
    size_t max_out
) 
```



Python: `x` is the H frames back to back, a flat array of H \* ny \* nx samples (H &gt;= 1).


The frames' product spectra are summed and the sum is inverted, as a dwell does: the result is bit-identical to what a fresh object with dwell = H dumps for the same frames, and equal to the sum of the H maps up to rounding. The object's own dwell is untouched.




**Parameters:**


* `state` Must be non-NULL. 
* `in` H frames back to back (CF32). 
* `n_in` A nonzero multiple of ny \* nx. 
* `out` n\_out values (CF32). 
* `max_out` Capacity of `out`; at least n\_out. 



**Returns:**

Python: the one map, a new (n\_out,) array; a refused batch is a ValueError with nothing written. C: n\_out, or SIZE\_MAX for the refused batch. 
```C++
>>> import numpy as np
>>> from doppler.spectral import DelayDoppler
>>> rng = np.random.default_rng(0)
>>> ny, nx = 8, 32
>>> code = np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)
>>> dd = DelayDoppler(code, ny=ny)
>>> frames = np.concatenate([np.tile(np.roll(code, d), ny)
...                          for d in (3, 11)])   # delays 3 and 11
>>> x = np.tile(code, ny)
>>> total = dd.execute_coherent(np.concatenate([x, -x]))
>>> bool(np.abs(total).max() < 1e-3)          # x and -x cancel
True
```
 





        

<hr>



### function dp\_correlator\_execute\_coherent\_max\_out 

_Samples_ [_**dp\_correlator\_execute\_coherent()**_](correlator__core_8h.md#function-dp_correlator_execute_coherent) _writes: n\_out._
```C++
size_t dp_correlator_execute_coherent_max_out (
    const dp_correlator_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

n\_out, fixed at create(). 
```C++
>>> import numpy as np
>>> from doppler.spectral import DelayDoppler
>>> rng = np.random.default_rng(0)
>>> ny, nx = 8, 32
>>> code = np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)
>>> dd = DelayDoppler(code, ny=ny)
>>> frames = np.concatenate([np.tile(np.roll(code, d), ny)
...                          for d in (3, 11)])   # delays 3 and 11
>>> dd.execute_coherent_max_out()
256
```
 





        

<hr>



### function dp\_correlator\_execute\_fold 

_Make the map of every frame in a batch and hand each to a reducer._ 
```C++
size_t dp_correlator_execute_fold (
    dp_correlator_state_t * state,
    const float _Complex * in,
    size_t n_in,
    const dp_correlator_fold_t * fold
) 
```



The one loop every typed batch call runs on. Maps are made as described above and passed to `fold->fold(fold->ctx, h, map, n_out)` for h = 0 .. H-1, in that order, from the calling thread.




**Parameters:**


* `state` Must be non-NULL. 
* `in` H frames, each ny \* nx samples (CF32), back to back. 
* `n_in` Samples in `in:` a nonzero multiple of ny \* nx. 
* `fold` The reducer; must be non-NULL. 



**Returns:**

H, or SIZE\_MAX (nothing folded) when `n_in` is 0 or not a multiple of the frame. 
```C++
#include "doppler/correlator/correlator_core.h"
#include <stdio.h>

// A reducer: the energy of each map, one value per frame.
static void
energy (void *ctx, size_t h, const float _Complex *m, size_t n_out)
{
  double e = 0.0;
  for (size_t k = 0; k < n_out; k++)
    e += crealf (m[k]) * crealf (m[k]) + cimagf (m[k]) * cimagf (m[k]);
  ((double *)ctx)[h] = e;
}

int
main (void)
{
  const float _Complex code[4] = { 1.0f, 1.0f, -1.0f, 1.0f };
  dp_correlator_state_t   *dd
      = dp_correlator_create_delay_doppler (code, 4, 2, 1, 1, 0);
  float _Complex frames[3 * 2 * 4]; // 3 frames of ny x nx = 2 x 4
  for (size_t k = 0; k < 3 * 2 * 4; k++)
    frames[k] = code[k % 4] * (float)(1 + k / 8); // frame h is (h+1) x
  double           e[3];
  dp_correlator_fold_t f = { energy, e };
  size_t           H = dp_correlator_execute_fold (dd, frames, 3 * 2 * 4, &f);
  printf ("%zu maps, energy ratios %.0f %.0f\n", H, e[1] / e[0],
          e[2] / e[0]); // 3 maps, energy ratios 4 9
  dp_correlator_destroy (dd);
  return !(H == 3 && e[1] > 3.99 * e[0] && e[1] < 4.01 * e[0]);
}
```
 





        

<hr>



### function dp\_correlator\_execute\_max\_out 

_Maximum output samples per execute call (_ [_**dp\_correlator\_state\_t::n\_out**_](structdp__correlator__state__t.md#variable-n_out) __ _ny\_out\*nx\_out normally, or ny when a single_[_**dp\_correlator\_state\_t::col\_out**_](structdp__correlator__state__t.md#variable-col_out) _was selected)._
```C++
size_t dp_correlator_execute_max_out (
    dp_correlator_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

n\_out, fixed at create(). 





        

<hr>



### function dp\_correlator\_execute\_peaks 

_One record per map of a batch: its peak power, where the peak is, and its mean power._ 
```C++
size_t dp_correlator_execute_peaks (
    dp_correlator_state_t * state,
    const float _Complex * in,
    size_t n_in,
    float * out,
    uint32_t * out1,
    uint32_t * out2,
    float * out3,
    size_t max_out
) 
```



Python: `x` is the H frames back to back, a flat array of H \* ny \* nx samples (H &gt;= 1).


Entry h of each output describes map h: `out` its largest `|map|^2`, `out1` and `out2` the row and column of that cell (the first in row-major order on a tie), and `out3` the mean `|map|^2` over its n\_out cells. A map is ny\_out x nx\_out; with col\_out it is ny x 1, and the column reported is col\_out. The peak-to-mean of each map is `out` over `out3`, entry by entry: a statistic of each map, which the same statistic of a pointwise maximum over maps does not give.




**Parameters:**


* `state` Must be non-NULL. 
* `in` H frames back to back (CF32). 
* `n_in` A nonzero multiple of ny \* nx. 
* `out` H peak powers (float). 
* `out1` H peak rows (uint32). 
* `out2` H peak columns (uint32). 
* `out3` H mean powers (float). 
* `max_out` Capacity of each output; at least H. 



**Returns:**

Python: the tuple (peak, row, col, mean), each (H,): float32 peak power, uint32 row and column, float32 mean power, always new arrays (this call takes no `out=`); a refused batch is a ValueError. C: H, or SIZE\_MAX with nothing written. 
```C++
>>> import numpy as np
>>> from doppler.spectral import DelayDoppler
>>> rng = np.random.default_rng(0)
>>> ny, nx = 8, 32
>>> code = np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)
>>> dd = DelayDoppler(code, ny=ny)
>>> frames = np.concatenate([np.tile(np.roll(code, d), ny)
...                          for d in (3, 11)])   # delays 3 and 11
>>> peak, row, col, mean = dd.execute_peaks(frames)
>>> row.tolist(), col.tolist()     # zero Doppler is row ny / 2
([4, 4], [3, 11])
>>> bool(np.all(peak / mean > 100))   # each map's own peak-to-mean
True
```
 





        

<hr>



### function dp\_correlator\_execute\_peaks\_max\_out 

_Records_ [_**dp\_correlator\_execute\_peaks()**_](correlator__core_8h.md#function-dp_correlator_execute_peaks) _writes for_`n_in` _samples: H._
```C++
size_t dp_correlator_execute_peaks_max_out (
    const dp_correlator_state_t * state,
    size_t n_in
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `n_in` Input samples about to be passed. 



**Returns:**

n\_in / (ny \* nx). 
```C++
const float _Complex code[4] = { 1.0f, 1.0f, -1.0f, 1.0f };
dp_correlator_state_t *dd = dp_correlator_create_delay_doppler (code, 4, 2, 1, 1,
                                                         0);
size_t h = dp_correlator_execute_peaks_max_out (dd, 3 * 2 * 4); // 3 frames
dp_correlator_destroy (dd);
return h != 3;
```
 





        

<hr>



### function dp\_correlator\_execute\_power\_max 

_The largest power over a batch's maps, cell by cell, and the map it came from._ 
```C++
size_t dp_correlator_execute_power_max (
    dp_correlator_state_t * state,
    const float _Complex * in,
    size_t n_in,
    float * out,
    uint32_t * out1,
    size_t max_out
) 
```



Python: `x` is the H frames back to back, a flat array of H \* ny \* nx samples (H &gt;= 1).


Each cell of `out` is the largest `|map_h|^2` that cell takes over the maps, and the same cell of `out1` is the h it came from. A tie goes to the lowest h: a later map replaces the running maximum only when it is strictly greater.




**Parameters:**


* `state` Must be non-NULL. 
* `in` H frames back to back (CF32); H at most 2^32. 
* `n_in` A nonzero multiple of ny \* nx. 
* `out` n\_out powers (float). 
* `out1` n\_out map indices (uint32). 
* `max_out` Capacity of each of `out` and `out1`; at least n\_out. 



**Returns:**

Python: the tuple (power, index), each (n\_out,): float32 powers and uint32 map indices, always new arrays (this call takes no `out=`); a refused batch, or H above 2\*\*32, is a ValueError. C: n\_out, or SIZE\_MAX with nothing written. 
```C++
>>> import numpy as np
>>> from doppler.spectral import DelayDoppler
>>> rng = np.random.default_rng(0)
>>> ny, nx = 8, 32
>>> code = np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)
>>> dd = DelayDoppler(code, ny=ny)
>>> frames = np.concatenate([np.tile(np.roll(code, d), ny)
...                          for d in (3, 11)])   # delays 3 and 11
>>> power, index = dd.execute_power_max(frames)
>>> int(index[4 * nx + 3]), int(index[4 * nx + 11])  # which map peaks
(0, 1)
```
 





        

<hr>



### function dp\_correlator\_execute\_power\_max\_max\_out 

_Values per output_ [_**dp\_correlator\_execute\_power\_max()**_](correlator__core_8h.md#function-dp_correlator_execute_power_max) _writes: n\_out._
```C++
size_t dp_correlator_execute_power_max_max_out (
    const dp_correlator_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

n\_out, fixed at create(). 
```C++
const float _Complex code[4] = { 1.0f, 1.0f, -1.0f, 1.0f };
dp_correlator_state_t *dd = dp_correlator_create_delay_doppler (code, 4, 2, 1, 1,
                                                         0);
size_t n = dp_correlator_execute_power_max_max_out (dd); // 2 x 4 = 8
dp_correlator_destroy (dd);
return n != 8;
```
 





        

<hr>



### function dp\_correlator\_execute\_power\_sum 

_The power of a batch's maps, summed cell by cell: each output cell is the sum over h of that cell's_ `|map_h|^2` _, summed in h order in float._
```C++
size_t dp_correlator_execute_power_sum (
    dp_correlator_state_t * state,
    const float _Complex * in,
    size_t n_in,
    float * out,
    size_t max_out
) 
```



Python: `x` is the H frames back to back, a flat array of H \* ny \* nx samples (H &gt;= 1).


Unlike the coherent sum, phase does not cancel: frames x and -x sum to twice one map's power, where their coherent sum is zero.




**Parameters:**


* `state` Must be non-NULL. 
* `in` H frames back to back (CF32). 
* `n_in` A nonzero multiple of ny \* nx. 
* `out` n\_out powers (float). 
* `max_out` Capacity of `out`; at least n\_out. 



**Returns:**

Python: the (n\_out,) float32 powers, a new array; a refused batch is a ValueError with nothing written. C: n\_out, or SIZE\_MAX for the refused batch. 
```C++
>>> import numpy as np
>>> from doppler.spectral import DelayDoppler
>>> rng = np.random.default_rng(0)
>>> ny, nx = 8, 32
>>> code = np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)
>>> dd = DelayDoppler(code, ny=ny)
>>> frames = np.concatenate([np.tile(np.roll(code, d), ny)
...                          for d in (3, 11)])   # delays 3 and 11
>>> x = np.tile(code, ny)
>>> power = dd.execute_power_sum(np.concatenate([x, -x]))
>>> one = np.abs(dd.execute_all(x)[0]) ** 2
>>> bool(np.allclose(power, 2 * one))        # x and -x do not cancel
True
```
 





        

<hr>



### function dp\_correlator\_execute\_power\_sum\_max\_out 

_Powers_ [_**dp\_correlator\_execute\_power\_sum()**_](correlator__core_8h.md#function-dp_correlator_execute_power_sum) _writes: n\_out._
```C++
size_t dp_correlator_execute_power_sum_max_out (
    const dp_correlator_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

n\_out, fixed at create(). 
```C++
>>> import numpy as np
>>> from doppler.spectral import DelayDoppler
>>> rng = np.random.default_rng(0)
>>> ny, nx = 8, 32
>>> code = np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)
>>> dd = DelayDoppler(code, ny=ny)
>>> frames = np.concatenate([np.tile(np.roll(code, d), ny)
...                          for d in (3, 11)])   # delays 3 and 11
>>> dd.execute_power_sum_max_out()
256
```
 





        

<hr>



### function dp\_correlator\_flavor 

_Which constructor built_ `state:` _what_[_**dp\_correlator\_create()**_](correlator__core_8h.md#function-dp_correlator_create) _chose from its knobs, so a caller can ask before reading a member only some flavors serve._
```C++
dp_correlator_flavor_t dp_correlator_flavor (
    const dp_correlator_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

The flavor, fixed at create. 





        

<hr>



### function dp\_correlator\_gate\_stat 

_The gate a dwell's statistic must clear to be a hit, in the units of_ [_**dp\_correlator\_hit\_t::test\_stat**_](structdp__correlator__hit__t.md#variable-test_stat) _:_`threshold` _on the coherent path (one look),_`eta_nc` _on the non-coherent one. A dwell hits when its strongest cell's statistic exceeds this, and every listed peak's statistic does too. Both statistics are linear in the cell's amplitude, so a list consumer can compare a peak's excess over another's against it in the same units. Configuration: fixed by the thresholds the grid was sized with._
```C++
double dp_correlator_gate_stat (
    const dp_correlator_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

The gate, in [**dp\_correlator\_hit\_t::test\_stat**](structdp__correlator__hit__t.md#variable-test_stat) units. 





        

<hr>



### function dp\_correlator\_get\_state 

_Serialize the frame count and spectrum accumulator into_ `blob` _._
```C++
void dp_correlator_get_state (
    const dp_correlator_state_t * state,
    void * blob
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `blob` Destination; at least [**dp\_correlator\_state\_bytes()**](correlator__core_8h.md#function-dp_correlator_state_bytes) bytes. 




        

<hr>



### function dp\_correlator\_period\_samples 

_The samples one code period should be searched at: the smallest power of two that holds at least two samples a chip._ 
```C++
size_t dp_correlator_period_samples (
    size_t code_len
) 
```



The search's fast-time transform is one code period long, so its length decides its speed: 2 \* 1023 = 2,046 = 2 \* 3 \* 11 \* 31 runs 5x slower than 2,048 (docs/design/async-dsss-receiver.md). An object that resamples its input for the search resamples to `chip_rate * dp_correlator_period_samples(code_len) / code_len`, and builds the engine at that rate. Two samples a chip is the least a code loop's early and late taps need.




**Parameters:**


* `code_len` Chips in one code period (&gt;= 1). 



**Returns:**

The period in samples, a power of two: 2048 for 1023 or 1024 chips, 64 for 31; 0 for `code_len` 0 or one whose period would not fit a size\_t. 





        

<hr>



### function dp\_correlator\_position 

_The engine's stream position: every sample its pushes have taken, framed ones and the carry, since create, reset, a regrid or set\_state's restore._ 
```C++
uint64_t dp_correlator_position (
    const dp_correlator_state_t * state
) 
```



The timebase [**dp\_correlator\_hit\_t::samples\_consumed**](structdp__correlator__hit__t.md#variable-samples_consumed) is counted in. Read at the start of a push, it is where that push's input begins, so a hit from that push ended at x + (hit.samples\_consumed - position): the hand-off a tracker resumes from. A caller's own count of what it has fed is a different timebase  configure\_search\_raw() and set\_state() reset or restore the engine and not the caller's count  and subtracting the two reads outside x.



```C++
float _Complex pre[124];               // a 31-chip code at 4x
for (size_t i = 0; i < 124; i++)
  pre[i] = (i / 4) % 3 ? 1.0f : -1.0f;
dp_correlator_state_t *a = dp_correlator_create_burst (pre, 124, 16, 4.0e6, 45.0,
                                         0.0, 1e-3, 0.9, 0, 0.0, 1);
float _Complex x[300] = { 0 };
uint64_t       before = dp_correlator_position (a);           // 0
dp_correlator_hit_t   hit;
if (dp_correlator_push (a, x, 300, &hit, 1) == 1)
  printf ("hit ended at x + %llu\n",
          (unsigned long long)(hit.samples_consumed - before));
uint64_t after = dp_correlator_position (a); // 300: framed and carried
dp_correlator_destroy (a);
if (before != 0 || after != 300)
  return 1;
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Samples taken since create, reset, a regrid or a restore: those framed plus the carry still pending. 





        

<hr>



### function dp\_correlator\_psl\_db 

_The preamble's peak sidelobe level, dB: 20\*log10 of the largest periodic-autocorrelation lag outside the mainlobe, relative to the peak (_ [_**dp\_correlator\_shape\_t::psl**_](structdp__correlator__shape__t.md#variable-psl) _)._
```C++
double dp_correlator_psl_db (
    const dp_correlator_state_t * state
) 
```



What it predicts: a detection's sidelobes sit this far below it, at delays outside the peak zone  where the peak list's exclusion does not reach. So a burst that clears the threshold by more than `-psl_db` also lists its own sidelobe as a second peak (with `max_peaks > 1`), and a strong burst's sidelobe can mask a weak one there. A code's is its periodic floor, 20\*log10(1/31) = -29.8 dB for a 31-chip m-sequence; a perfect sequence (Zadoff-Chu, a Frank-type chirp) has none and reads -INFINITY. Configuration, not state: it is fixed at construction and not serialized.




**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Peak sidelobe level in dB (&lt;= 0), or -INFINITY.



```C++
>>> import numpy as np
>>> from doppler.acquire import BurstAcquisition
>>> from doppler.cvt import bin_to_nrz
>>> from doppler.wfm import PN
>>> chips = (np.asarray(PN(poly=0, seed=1, length=5).generate(31)) & 1
...          ).astype(np.uint8)
>>> nrz = np.zeros(31, dtype=np.float32)
>>> _ = bin_to_nrz(chips, nrz)
>>> pre = np.repeat(nrz, 4).astype(np.complex64)    # 4 samples a chip
>>> a = BurstAcquisition(pre, reps=4, fs=4.0e6, cn0_dbhz=61.0)
>>> round(a.psl_db, 2)          # an m-sequence: 1/31
-29.83
>>> k = np.arange(127)
>>> zc = np.exp(-1j * np.pi * 5 * k * (k + 1) / 127).astype(np.complex64)
>>> BurstAcquisition(zc, reps=8, fs=1.0e6, cn0_dbhz=53.0).psl_db
-inf
```
 


        

<hr>



### function dp\_correlator\_push 

_Stream an arbitrary-length CF32 chunk through a detector flavor._ 
```C++
size_t dp_correlator_push (
    dp_correlator_state_t * state,
    const float _Complex * in,
    size_t n_in,
    dp_correlator_hit_t * result,
    size_t max_results
) 
```



Takes the input in order, runs each complete n-sample frame (n = ny\*nx) through the correlator, and on every dump computes the test statistic peak\_mag / noise\_est. Each dump that passes the threshold is one hit. In Python the result is always a list: of (row, col, peak\_mag, noise\_est, test\_stat) tuples from CorrDetector2D, and of (lag, peak\_mag, noise\_est, test\_stat) tuples from CorrDetector, whose lag is the hit's col.


Python's push() has room for 1024 detections a call, so a push that completes at most 1024 frames, counting the carry, loses nothing. One that fills the room loses the rest of its input, every later frame whether or not it would have made a detection, unless the rest is shorter than a frame: that is kept as the carry. The next push stays on the frame grid only when what was lost is a whole number of frames; otherwise its peaks are off by the remainder. #1992 and just-buildit/just-makeit#2184 track sizing the list to the call.




**Parameters:**


* `state` A detector flavor (non-NULL). 
* `in` CF32 input chunk of arbitrary length. 
* `n_in` Number of input samples in `in`. 
* `result` Caller-supplied array of at least `max_results` hits; filled on return. 
* `max_results` Capacity of `result` (maximum detections to emit). A full `result` never loses input: a frame yields at most one detection, and once `result` is full the push takes the rest of its input only if the rest completes no frame (it is then the carry); otherwise it takes nothing more, stopping on the boundary of its last frame. [**dp\_correlator\_consumed()**](correlator__core_8h.md#function-dp_correlator_consumed) says how many samples it took, and the caller offers the rest again. Input that runs out mid-frame is the carry, held inside (fewer than n samples). With room 0 the push is full from the start, so a resume loop needs room for at least one; with that, a push of any input takes at least one sample. 



**Returns:**

Python: the list of hits, empty when no dump passed the gate. C: the number of hits written to `result`, or SIZE\_MAX (nothing taken, nothing changed) when `state` is a map flavor. 
```C++
>>> from doppler.spectral import CorrDetector, CorrDetector2D
>>> import numpy as np
>>> ref = np.zeros((4, 4), dtype=np.complex64); ref[0, 0] = 1.0
>>> det = CorrDetector2D(ref=ref, dwell=1, noise_lo=1, noise_hi=15,
...                  noise_mode="mean", threshold=0.0)
>>> results = det.push(np.ones((4, 4), dtype=np.complex64))
>>> row, col, peak, noise, stat = results[0]
>>> row, col, round(peak, 4), round(noise, 4), round(stat, 4)
(0, 0, 1.0, 1.0, 1.0)
>>> det1 = CorrDetector(ref=ref[0], dwell=1, noise_lo=1, noise_hi=3,
...                 noise_mode="mean", threshold=0.0)
>>> lag, peak, noise, stat = det1.push(np.ones(4, dtype=np.complex64))[0]
>>> lag, round(peak, 4), round(noise, 4), round(stat, 4)
(0, 1.0, 1.0, 1.0)
```
 





        

<hr>



### function dp\_correlator\_rate\_hypotheses\_for 

_The fewest rate hypotheses over_ `[-span, +span]` _that leave the residual_`span/(n - 1)` _within what a block of_`depth` _epochs tolerates,_`f_epoch^2/(2*depth^2)` __ _the drift rule behind the depth cap, inverted. At least 2 for a positive span; 1 for none; at most_[_**DP\_CORRELATOR\_RATE\_HYPS\_MAX**_](correlator__core_8h.md#define-dp_correlator_rate_hyps_max) _._
```C++
size_t dp_correlator_rate_hypotheses_for (
    double f_epoch,
    double span,
    size_t depth
) 
```





**Parameters:**


* `f_epoch` Epoch rate, Hz (`fs/n` for a burst engine). 
* `span` Rate span, Hz/s (&gt;= 0). 
* `depth` Block depth in epochs (&gt;= 1). 



**Returns:**

The hypothesis count; 1 when `span` is not positive and finite, `f_epoch` is not positive and finite, or `depth` is 0. 





        

<hr>



### function dp\_correlator\_rate\_hypothesis 

_Rate hypothesis_ `h` _of_`n` _over_`[-span, +span]` _, Hz/s:_`-span + 2*span*h/(n - 1)` _; 0 for_`n <= 1` _._
```C++
double dp_correlator_rate_hypothesis (
    double span,
    size_t n,
    size_t h
) 
```



The engine's grid, exported so a composing object searches the SAME rates and not a restatement of them (dp\_burst\_capture's refine).




**Parameters:**


* `span` Half-width of the rate search, Hz/s. 
* `n` Number of hypotheses on the grid. 
* `h` Hypothesis index, 0 .. n - 1 (not range-checked). 



**Returns:**

The rate of hypothesis `h`, Hz/s. 





        

<hr>



### function dp\_correlator\_rate\_phasor 

_The de-chirp phasor_ `exp(-j*pi*rate*t^2)` _for a rate in Hz/s at time_`t` _seconds from the block's middle._
```C++
double _Complex dp_correlator_rate_phasor (
    double rate,
    double t
) 
```



What the engine multiplies a block's slow-time axis by, and what refine multiplies its per-period correlations by: one definition of the centring and the sign.




**Parameters:**


* `rate` Doppler rate hypothesis, Hz/s. 
* `t` Time from the block's middle, seconds. 



**Returns:**

`exp(-j*pi*rate*t^2)`, unit magnitude. 





        

<hr>



### function dp\_correlator\_reset 

_Zero the accumulator and reset the integration counter to 0. Equivalent to starting a fresh dwell cycle without rebuilding FFT plans or recomputing ref\_spec._ 
```C++
void dp_correlator_reset (
    dp_correlator_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL.


```C++
>>> from doppler.spectral import Corr2D
>>> import numpy as np
>>> ref = np.zeros((2, 2), dtype=np.complex64); ref[0, 0] = 1.0
>>> c = Corr2D(ref=ref, dwell=3)
>>> _ = c.execute(np.ones((2, 2), dtype=np.complex64))
>>> c.count
1
>>> c.reset()
>>> c.count
0
```
 


        

<hr>



### function dp\_correlator\_run 

_Pure run: inject_ `state_in` _, stream_`in` _, emit hits, export_`state_out` _—_`(state_in, input) -> (state_out, output)` _over an engine treated as immutable config + scratch._`state_in` _/_`state_out` _may alias. Either may be NULL (NULL in = fresh; NULL out = discard)._
```C++
size_t dp_correlator_run (
    dp_correlator_state_t * state,
    const void * state_in,
    void * state_out,
    const float _Complex * in,
    size_t n_in,
    dp_correlator_hit_t * result,
    size_t max_results
) 
```



The run is one [**dp\_correlator\_push()**](correlator__core_8h.md#function-dp_correlator_push), so it stops where a push stops: once `result` is full, or before a frame that ends a dwell when fewer than min(max\_peaks, `max_results`) slots are left. It may then leave part of `in` untaken. dp\_correlator\_consumed(`state`) says how much it took; the caller calls again with `state_out` as the next `state_in` and the rest, in + consumed, and the two runs together equal one over the whole input. A refused `state_in` runs nothing and returns 0, with [**dp\_correlator\_consumed()**](correlator__core_8h.md#function-dp_correlator_consumed) reading 0.




**Parameters:**


* `state` Engine (config + scratch); its running state is replaced by `state_in`, or reset when that is NULL. 
* `state_in` Blob from [**dp\_correlator\_get\_state()**](correlator__core_8h.md#function-dp_correlator_get_state), or NULL for fresh. 
* `state_out` Destination blob ([**dp\_correlator\_state\_bytes()**](correlator__core_8h.md#function-dp_correlator_state_bytes) bytes), or NULL to discard. 
* `in` Input cf32 samples. 
* `n_in` Number of input samples. 
* `result` Output hits. 
* `max_results` Capacity of `result`. 



**Returns:**

Number of events written (0 … max\_results). 





        

<hr>



### function dp\_correlator\_search\_period 

_The samples one code period is searched at, for a caller feeding_ `sample_rate:` _its own period when that is already fast, else_[_**dp\_correlator\_period\_samples()**_](correlator__core_8h.md#function-dp_correlator_period_samples) _._
```C++
size_t dp_correlator_search_period (
    size_t code_len,
    double sample_rate,
    double chip_rate
) 
```



The caller's period, `sample_rate * code_len / chip_rate`, is kept when it is a whole number of samples (to a part in 1e9) and at least `code_len:` searching the samples as they come needs no resampler. Its length need not be fast for the FFT: one with a larger prime (2 \* 1,023 = 2 \* 3 \* 11 \* 31) is read on a padded lag grid, `lag_bins` columns, the next power of two. Otherwise  a fractional period (11 MSa/s over 5 Mcps for 1,023 chips is 2,250.6)  it is the power of two [**dp\_correlator\_period\_samples()**](correlator__core_8h.md#function-dp_correlator_period_samples) derives, and the caller resamples to `chip_rate * period / code_len`. 4 samples a chip of a 7-chip code keeps 28; 2 a chip of 1,023 chips keeps 2,046 (read on 2,048 lag columns); 11 MSa/s over 5 Mcps takes 2,048.




**Parameters:**


* `code_len` Chips in one code period (&gt;= 1). 
* `sample_rate` The caller's sample rate, Hz (&gt; 0). 
* `chip_rate` Chip rate, Hz (&gt; 0). 



**Returns:**

The period in samples; 0 for an argument outside the domain. 





        

<hr>



### function dp\_correlator\_set\_carrier\_freq\_hz 

_Couple the code clock to the carrier: the chip rate dilates by_ `doppler_hz / carrier_freq_hz` _, and the engine accounts for it._
```C++
int dp_correlator_set_carrier_freq_hz (
    dp_correlator_state_t * state,
    double carrier_freq_hz
) 
```



A physically-coupled Doppler moves the code as well as the carrier  100 chips/s at 20 ppm of 5 Mcps  and the engine's two long integrations both smear over it (doppler#1256, #1254):
* **Inside a coherent block** of D epochs every tile's epoch correlations are shifted along the code axis by the drift the tile's own frequency implies, `f_tile / carrier` chips per chip, aligned to the block's middle (a linear phase on each epoch's product). The one delay-Doppler map applies it at the block's end, read on a slow-time grid 4 times finer and rounded. The bound is 0.09 dB, the worst over 33 tones and three dilations (docs/design/acq-surface.md §18.1); the engine measures 0.020-0.026 dB on its equivalence test's carrier scenes. [**dp\_correlator\_block\_prompt()**](correlator__core_8h.md#function-dp_correlator_block_prompt) applies it exactly. A change of carrier mid-block applies to the whole block. Measured at the specification's 20 ppm with D = 154 (3.1 chips of drift across the block): without it the block's peak is 13 dB down and 3 chips wide and the depth detects nothing at 34 dB-Hz; with it the block reads as a still one.
* **The hand-off** ([**dp\_correlator\_build\_handoff()**](correlator__core_8h.md#function-dp_correlator_build_handoff)) advances the hit's code phase by the drift over half the dwell  the non-coherent sum's peak is the phase at the dwell's middle, the seed is wanted at its end: 0.9 chip at the 40 dB-Hz floor, past a refine loop's pull-in.




A mutator's value (#2022): it rides in the state blob (v8), and a restore takes it through this setter's rebuild, so a resumed engine carries it on. Setting it rebuilds the map's fine grid and every tile's ramp tables when they move (the regrid that re-applies the thread request). Default 0.0 (uncoupled) is the engine exactly as it ran without it.




**Parameters:**


* `state` Must be non-NULL. 
* `carrier_freq_hz` RF carrier, Hz; 0.0 = uncoupled. 



**Returns:**

`DP_OK`, `DP_ERR_INVALID` for a carrier outside [**dp\_correlator\_carrier\_freq\_ok()**](correlator__core_8h.md#function-dp_correlator_carrier_freq_ok) at this engine's sample rate, or `DP_ERR_MEMORY` when the rebuild cannot allocate (the engine is left as it was). 
```C++
>>> import numpy as np
>>> from doppler.acquire import Acquisition
>>> from doppler.wfm import Gold
>>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
>>> a = Acquisition(code, sample_rate=1e7, chip_rate=5e6, symbol_rate=2700.0,
...                 cn0_dbhz=45.0, doppler_uncertainty=50e3)
>>> a.carrier_freq_hz
0.0
>>> a.set_carrier_freq_hz(2.5e9)
>>> a.carrier_freq_hz
2500000000.0
```
 





        

<hr>



### function dp\_correlator\_set\_keep\_surface 

_Keep, or stop keeping, every decided dwell's surface for_ [_**dp\_correlator\_surface()**_](correlator__core_8h.md#function-dp_correlator_surface) _: 1 normalises each one into the gate's units at its decision, 0 (the default) costs nothing._
```C++
void dp_correlator_set_keep_surface (
    dp_correlator_state_t * state,
    int keep
) 
```



Python's writable Acquisition.keep\_surface; [**dp\_correlator\_set\_surface\_sink()**](correlator__core_8h.md#function-dp_correlator_set_surface_sink) sets it too, and a search flavor's other setters leave it as it is. A flavor with no search does nothing.




**Parameters:**


* `state` Must be non-NULL. 
* `keep` 1 to keep, 0 not to. 




        

<hr>



### function dp\_correlator\_set\_max\_peaks 

_How many peaks a dwell may report: the peak list's capacity._ 
```C++
int dp_correlator_set_max_peaks (
    dp_correlator_state_t * state,
    size_t n
) 
```



One (the default) is the classic detector  the maximum of the surface, gated. More is the list of docs/design/async-dsss-receiver.md §7.1: every peak above the same gate, strongest first, each with an exclusion zone of one Doppler bin by the reference's first autocorrelation null around it (one chip for a PN code; one emitter's main lobe, so its own shoulders are not the next peak), and the two-epoch rule for a peak at an already-listed code phase  a data transition inside the epoch splits one emitter into twins at its own code phase on other tiles, so such a peak is held for one dwell and listed only if it was there, at the same tile, on the previous one. Each listed peak is one [**dp\_correlator\_hit\_t**](structdp__correlator__hit__t.md) from [**dp\_correlator\_push()**](correlator__core_8h.md#function-dp_correlator_push), all of a dwell's sharing its `samples_consumed` and `noise_est`. A held twin takes a slot of the `n` for that dwell but is not reported. The threshold does not change: a second peak is another draw from the same cells against the same union bound. Clears the held candidates.




**Parameters:**


* `state` Must be non-NULL. 
* `n` 1 … DP\_CORRELATOR\_MAX\_PEAKS. 



**Returns:**

0, or -1 (state untouched) when `n` is out of range. 
```C++
>>> import numpy as np
>>> from doppler.acquire import Acquisition
>>> code = (np.arange(31) * 5 % 2).astype(np.uint8)
>>> a = Acquisition(code, sample_rate=2e6, chip_rate=1e6, symbol_rate=1e3,
...                 cn0_dbhz=50.0, doppler_uncertainty=50e3)
>>> a.max_peaks
1
>>> a.set_max_peaks(8)
>>> a.max_peaks
8
```
 





        

<hr>



### function dp\_correlator\_set\_ref 

_Replace the reference and recompute its spectrum._ 
```C++
int dp_correlator_set_ref (
    dp_correlator_state_t * state,
    const float _Complex * ref
) 
```



Also resets accumulator and counter on success. The object's fast-path mode (see the file doc comment) is fixed at create() time and never changes here: on a fast-path object, `ref` must still be nonzero only in row 0, or the call is rejected and the object's existing reference and spectrum are left completely untouched (never a silent partial update) — a caller that needs a genuinely different reference shape must build a new [**dp\_correlator\_state\_t**](structdp__correlator__state__t.md). A general-path object accepts any (ny,nx) `ref` and always succeeds. A detector flavor resets first (see [**dp\_correlator\_reset()**](correlator__core_8h.md#function-dp_correlator_reset)), so it is reset even when the reference is then refused.




**Parameters:**


* `state` Must be non-NULL. 
* `ref` New reference, flat row-major CF32, length ny\*nx. 



**Returns:**

0 on success, -1 if `state` is fast-path and `ref` is no longer single-row. 





        

<hr>



### function dp\_correlator\_set\_state 

_Restore a mid-dwell correlator from_ [_**dp\_correlator\_get\_state()**_](correlator__core_8h.md#function-dp_correlator_get_state) _'s blob._
```C++
int dp_correlator_set_state (
    dp_correlator_state_t * state,
    const void * blob
) 
```





**Parameters:**


* `state` Must be non-NULL, created with the same (ny, nx) and reference (the reference is not in the blob). 
* `blob` Source blob of [**dp\_correlator\_state\_bytes()**](correlator__core_8h.md#function-dp_correlator_state_bytes) bytes. 



**Returns:**

`DP_OK`, or `DP_ERR_INVALID` (state untouched) when the header's magic, version, endianness or size does not match, when the blob's flavor, ny, ny\_out, fast\_path, col\_out or nx\_out differs from `state's` (a blob of one flavor never restores into another), when its frame count is not below `state's` dwell, or, for a detector flavor, when noise\_lo, noise\_hi or noise\_mode differs, the framer's child blob is refused, the frames that blob has handed out are not the dwell's position (frames mod dwell must equal the count, as every push, reset and set\_ref keeps it), or the last peak is not a cell of the map. Everything is checked before anything is written. 





        

<hr>



### function dp\_correlator\_set\_surface\_sink 

_Attach (or detach) a C surface sink: every_ `decim-th` _decided dwell's surface, in test-statistic units, handed to_`fn` _on the pushing thread (docs/design/async-dsss-receiver.md §2.4)._
```C++
void dp_correlator_set_surface_sink (
    dp_correlator_state_t * state,
    dp_correlator_surface_sink_fn fn,
    void * ctx,
    uint32_t decim
) 
```



Sets `keep_surface`, so [**dp\_correlator\_surface()**](correlator__core_8h.md#function-dp_correlator_surface) reads the same dwell afterwards. The pointer handed to `fn` is the engine's and is valid only for the call: copy or write it out there. This is how a long run records the surface decimated in time without a copy per dwell it does not keep.




**Parameters:**


* `state` Must be non-NULL. 
* `fn` The sink, or NULL to detach. 
* `ctx` Passed through to `fn`. 
* `decim` Hand over every decim-th dwell; 0 reads as 1. 




        

<hr>



### function dp\_correlator\_set\_telemetry 

_Attach (or detach) a telemetry context and register the engine's probes on it (docs/design/async-dsss-receiver.md §2.4)._ 
```C++
int dp_correlator_set_telemetry (
    dp_correlator_state_t * state,
    dp_tlm_t * tlm,
    const char * prefix,
    uint32_t decim
) 
```



Registers ten probes, emitted once per DECIDED dwell (a coherent dump, or the dwell that completes `n_noncoh` looks) and further thinned by `decim:` "&lt;prefix&gt;.stat" (the dwell's test statistic — the strongest cell against the CFAR reference, in the units the gate is set in), "&lt;prefix&gt;.gate" (that gate: `threshold` on the coherent path, `eta_nc` on the non-coherent one — plotted together they show exactly where a hit fired), "&lt;prefix&gt;.noise" (the CFAR reference `noise_est`), "&lt;prefix&gt;.peak" (the strongest cell's raw value), "&lt;prefix&gt;.row" and "&lt;prefix&gt;.col" (its native Doppler row and code-phase column — a surface coordinate, not a physical unit; [**dp\_correlator\_surface\_doppler\_hz()**](correlator__core_8h.md#function-dp_correlator_surface_doppler_hz) and [**dp\_correlator\_surface\_chip\_phase()**](correlator__core_8h.md#function-dp_correlator_surface_chip_phase) convert), "&lt;prefix&gt;.n\_peaks" (picks in the dwell, held twins included), "&lt;prefix&gt;.n\_held" (picks held as same-code-phase twins rather than listed, §7.1), "&lt;prefix&gt;.conc" (the strongest pick's concentration — see `peak_conc`: its main lobe's power over its whole column's, near 1 for one clean emitter even when it straddles two tiles, about 0.5 when a data transition splits it into twins two or more tiles away, lower still when a coherent block straddles data — the discriminator between one emitter's splatter and a second emitter) and "&lt;prefix&gt;.hit" (1 when the gate fired). Passing NULL detaches. Setup path, never hot; the context is borrowed and must outlive the attachment (SPSC rules in [**dp\_tlm/dp\_tlm\_core.h**](dp__tlm__core_8h.md)).




**Parameters:**


* `state` Must be non-NULL. 
* `tlm` Telemetry context to attach, or NULL to detach. 
* `prefix` Probe-name prefix, e.g. "acq" or "ch0.acq". 
* `decim` Emit every decim-th decided dwell; &gt;= 1. 



**Returns:**

DP\_OK; DP\_ERR\_INVALID for an overlong `prefix`, a `decim` of 0, or a probe table without room for all ten probes; DP\_ERR\_MEMORY when the dry run's scratch context cannot be allocated. A refusal registers nothing on `tlm` and leaves the previous attachment (or detachment) exactly as it was. 
```C++
>>> import numpy as np
>>> from doppler.acquire import Acquisition
>>> from doppler.telemetry import Telemetry
>>> from doppler.wfm import PN, mls_poly
>>> code = np.asarray(
...     PN(poly=mls_poly(9), seed=1, length=9).generate(511), np.uint8)
>>> a = Acquisition(code, sample_rate=2e6, chip_rate=1e6, cn0_dbhz=50.0)
>>> tlm = Telemetry(1 << 12)
>>> a.set_telemetry(tlm, "acq")
>>> sorted(tlm.probe_names)[:3]
['acq.col', 'acq.conc', 'acq.gate']
>>> x = np.zeros(a.n_noncoh * 511 * 2 * 3, dtype=np.complex64)
>>> _ = a.push(x)
>>> len(tlm.read()) % 10      # ten records per decided dwell
0
```
 





        

<hr>



### function dp\_correlator\_set\_threads 

_Set how many threads the searcher fans its tiles across (docs/design/acq-surface.md §13: a roll per thread on persistent workers)._ 
```C++
int dp_correlator_set_threads (
    dp_correlator_state_t * state,
    int n
) 
```



A continuous engine is created with a pool of the machine's online cores when it has more than one tile; a burst engine, and a single-tile one, run serially. This sets the count: 0 auto-selects the online core count, 1 runs everything on the calling thread, n runs on n workers (the caller included)  but never more than `window_bins`: every fan hands out one tile a task, so a worker past that only wakes and waits. The request is kept, and every regrid re-applies it, so an engine narrowed to one tile and widened again runs on min(n, tiles), not on the 1 the narrow grid ran on. The workers are created here, once, and parked between pushes; nothing is created per push. The surface is bit-identical at every count  the tiles are independent after the one forward transform and each writes its own rows  so this changes the cost of a push and nothing about its result. Setup path, never hot; not while another thread is inside push().




**Parameters:**


* `state` Must be non-NULL. 
* `n` Thread count; 0 = online cores, 1 = serial. 



**Returns:**

DP\_OK. The count actually running is `threads`. 
```C++
>>> import numpy as np
>>> from doppler.acquire import Acquisition
>>> from doppler.wfm import PN, mls_poly
>>> code = np.asarray(
...     PN(poly=mls_poly(9), seed=1, length=9).generate(511), np.uint8)
>>> a = Acquisition(code, sample_rate=2e6, chip_rate=1e6, cn0_dbhz=50.0,
...                 doppler_uncertainty=4000.0)
>>> a.threads >= 1               # at most one worker a tile
True
>>> a.set_threads(1)
>>> a.threads
1
```
 





        

<hr>



### function dp\_correlator\_set\_threshold 

_Change a detector flavor's threshold without rebuilding it._ 
```C++
int dp_correlator_set_threshold (
    dp_correlator_state_t * state,
    float threshold
) 
```



The value is part of the state blob, so set\_state restores it. A search flavor refuses it: its gate is sized from Pfa and Pd at create, together with the raw and non-coherent thresholds it implies.




**Parameters:**


* `state` Must be non-NULL. 
* `threshold` New threshold; 0.0 = always fire. 



**Returns:**

DP\_OK, or DP\_ERR\_INVALID (nothing changed) for a map or search flavor. 





        

<hr>



### function dp\_correlator\_sidelobe\_ratio 

_How far up the sidelobes of the emitter behind one hit can reach at another hit's cell, relative to that emitter's own peak._ 
```C++
double dp_correlator_sidelobe_ratio (
    dp_correlator_state_t * state,
    const dp_correlator_hit_t * from,
    const dp_correlator_hit_t * to
) 
```



One emitter puts its code's cross-ambiguity on every cell of the surface. This computes that row the way the surface itself is made: one period of the replica, carrying `from's` Doppler less the cell's tile centre and delayed by `from's` lag (a phase ramp, so a lag between two samples is exact), its spectrum times the replica's conjugate, zero-padded to lag\_bins and inverted ([**dp\_fft\_pad\_spectrum()**](fft__core_8h.md#function-dp_fft_pad_spectrum)), read at `to's` column. The emitter's own peak is the same product's zero lag at its residual from its own tile's centre. The ratio is the most it can be over what the hits do not pin down:



* `from's` lag is known to half a lag column either side of its column: five points across that are read;
* `from's` Doppler is known to half a native row either side: nine points across that row are read, its own residual moving with each;
* a cell on a tile edge, or on the combined axis's wrap, is read in every tile it touches.




The cell's fast-time mixing frequency is its tile's centre (a multiple of the epoch rate, chip\_rate / sf; DC on a single-tile engine): within a tile the slow-time transform only lowers a sidelobe that sits off the emitter's own row, and no credit is taken for that. The value is a SAMPLED maximum, not a proven bound: the cross-ambiguity varies on a one-sample scale, and between the five lag and nine Doppler points it reads, it can sit higher than any of them. What holds it is measurement: native/validation/acq\_emitter\_floor.c holds every cell of a clean emitter's tile and the two beside it under it, on the scale rule 2 uses (the emitter's statistic on its own lag): least margin +0.61 dB over 6,129 cells, on the Gold-1023 floor. With a data transition inside the epoch it does not bound the surface (a cell reads 20.4 dB above it: the model is one clean period, doppler#2734), and async\_dsss\_near\_far.c takes no sidelobe over the operating range, where the gate covers that. It is the code's worst sidelobe ([**dp\_correlator\_dsl\_db()**](correlator__core_8h.md#function-dp_correlator_dsl_db)) only at the worst cells: most cells sit well below it, which is the room a weak emitter beside a strong one needs. Times `from's` statistic on its own lag ([**dp\_correlator\_hit\_t::test\_stat\_peak**](structdp__correlator__hit__t.md#variable-test_stat_peak)), it is the statistic `from's` sidelobe can read at `to`.


Cost: the first read against a hit, in a tile, sweeps that hit's whole row there: 5 inverse transforms of code\_bins (the lag points), then per lag and Doppler point one forward of code\_bins and one inverse of lag\_bins (45), every complex exponential a resynced recurrence. Every column is kept, so later reads against the same hit and tile, any cell, are lookups: DP\_CORRELATOR\_SIDELOBE\_SLOTS (hit, tile) rows, keyed on the hit's cell and the tile, round-robin when full, and emptied by every regrid (each slot was read on the grid it was filled on). A pick reads many cells against its few stronger hits, dwell after dwell, so nearly every read is a lookup. The plans, buffers and cache are made on the first call and kept on `state`.




**Parameters:**


* `state` The engine both hits came from (non-NULL). 
* `from` The hit whose emitter's sidelobes are bounded. 
* `to` The hit whose cell they are bounded at. 



**Returns:**

The ratio, from 0 to 1. 





        

<hr>



### function dp\_correlator\_state\_bytes 

_Size in bytes of this correlator's state blob._ 
```C++
size_t dp_correlator_state_bytes (
    const dp_correlator_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Header + flavor, ny, ny\_out, fast\_path, col\_out, nx\_out and frame count (u64 each) + the ny\*nx complex-float accumulator; a detector flavor adds 5 u64, 4 f32 and the framer's child blob. 





        

<hr>



### function dp\_correlator\_state\_check 

_Whether_ [_**dp\_correlator\_set\_state()**_](correlator__core_8h.md#function-dp_correlator_set_state) _would accept_`blob` _, without writing anything._
```C++
int dp_correlator_state_check (
    const dp_correlator_state_t * state,
    const void * blob
) 
```



DP\_OK exactly when set\_state would accept it, and DP\_ERR\_INVALID exactly when it would refuse it: both read the blob through one check. What lets a composition check this child before it restores any other, so a refusal of either changes nothing (carrier\_acq's 'CAQR' does).




**Parameters:**


* `state` Must be non-NULL. 
* `blob` Candidate blob of [**dp\_correlator\_state\_bytes()**](correlator__core_8h.md#function-dp_correlator_state_bytes) bytes. 



**Returns:**

`DP_OK` or `DP_ERR_INVALID`; `state` is never written. 





        

<hr>



### function dp\_correlator\_state\_position 

_The stream position a blob records_  __[_**dp\_correlator\_position()**_](correlator__core_8h.md#function-dp_correlator_position) _of the engine it would restore_ _read without restoring it._
```C++
int dp_correlator_state_position (
    const dp_correlator_state_t * state,
    const void * blob,
    uint64_t * position
) 
```



What lets a composition check its child's blob against its own count before it writes anything, so a refusal never has to be undone (an undo is a second set\_state, and set\_state can fail). Accepts exactly the blobs [**dp\_correlator\_set\_state()**](correlator__core_8h.md#function-dp_correlator_set_state) checks and accepts before its carrier rebuild, through the same check.



```C++
float _Complex pre[124];               // a 31-chip code at 4x
for (size_t i = 0; i < 124; i++)
  pre[i] = (i / 4) % 3 ? 1.0f : -1.0f;
dp_correlator_state_t *a = dp_correlator_create_burst (pre, 124, 16, 4.0e6, 45.0,
                                         0.0, 1e-3, 0.9, 0, 0.0, 1);
float _Complex x[300] = { 0 };
dp_correlator_hit_t   hit;
(void)dp_correlator_push (a, x, 300, &hit, 1);
void    *blob = dp_xmalloc (dp_correlator_state_bytes (a));
uint64_t at   = 0;
dp_correlator_get_state (a, blob);
int ok = dp_correlator_state_position (a, blob, &at); // DP_OK, 300
int same = at == dp_correlator_position (a);
free (blob);
dp_correlator_destroy (a);
if (ok != DP_OK || at != 300 || !same)
  return 1;
```





**Parameters:**


* `state` The engine the blob would be restored into. 
* `blob` A blob from [**dp\_correlator\_get\_state()**](correlator__core_8h.md#function-dp_correlator_get_state). 
* `position` Out: the blob's position. Unchanged on a refusal. 



**Returns:**

DP\_OK, or DP\_ERR\_INVALID where [**dp\_correlator\_set\_state()**](correlator__core_8h.md#function-dp_correlator_set_state) would refuse the blob as invalid. 





        

<hr>



### function dp\_correlator\_surface 

_The last decided dwell's surface, in the gate's own units._ 
```C++
size_t dp_correlator_surface (
    dp_correlator_state_t * state,
    float * out,
    size_t n_out
) 
```



Copies the surface the last dwell was decided on into `out`, row-major `surface_rows` (Doppler: tiles, or interpolated slow-time rows) by `lag_bins` (code phase), every cell divided by the same CFAR reference the gate used — so a cell reads as its own test statistic, to a float rounding (the SIMD build's fast-math may take a reciprocal in this loop and a divide in the gate's), and the gate (`threshold`, or `eta_nc` on the non-coherent path) is a flat plane on a plot. The engine keeps this only while `keep_surface` is set (a caller sets it, or [**dp\_correlator\_set\_surface\_sink()**](correlator__core_8h.md#function-dp_correlator_set_surface_sink) does): set it, push, then read. `surface_at` says which dwell it is; a time-decimated record is the caller reading every k-th dwell, or a sink with `decim`.




**Parameters:**


* `state` Must be non-NULL. 
* `out` At least `surface_rows * lag_bins` floats. 
* `n_out` Capacity of `out`. 



**Returns:**

Cells written (`surface_rows * lag_bins`), or 0 when no dwell has been decided with `keep_surface` set, or `out` is too small. 
```C++
>>> import numpy as np
>>> from doppler.acquire import Acquisition
>>> from doppler.wfm import PN, mls_poly
>>> code = np.asarray(
...     PN(poly=mls_poly(9), seed=1, length=9).generate(511), np.uint8)
>>> a = Acquisition(code, sample_rate=2e6, chip_rate=1e6, cn0_dbhz=50.0)
>>> a.keep_surface = 1
>>> x = np.zeros(a.n_noncoh * 511 * 2, dtype=np.complex64)
>>> _ = a.push(x)
>>> s = np.empty(a.surface_rows * a.lag_bins, dtype=np.float32)
>>> a.surface(s) == s.size
True
>>> a.code_bins, a.lag_bins  # 1022 is 2 x 7 x 73: read on 1024 lags
(1022, 1024)
>>> s.reshape(a.surface_rows, a.lag_bins).shape == (a.surface_rows, 1024)
True
```
 





        

<hr>



### function dp\_correlator\_surface\_chip\_phase 

_The surface's code-phase axis: the chip phase of each column._ 
```C++
size_t dp_correlator_surface_chip_phase (
    dp_correlator_state_t * state,
    double * out,
    size_t n_out
) 
```



One value per surface column, in chips, the same mapping [**dp\_correlator\_build\_handoff()**](correlator__core_8h.md#function-dp_correlator_build_handoff) applies to a hit's `col` — so a plotted peak sits at the chip phase the DetectionEvent would carry.




**Parameters:**


* `state` Must be non-NULL. 
* `out` At least `lag_bins` doubles. 
* `n_out` Capacity of `out`. 



**Returns:**

Values written (`lag_bins`), or 0 if `out` is too small. 
```C++
>>> import numpy as np
>>> from doppler.acquire import Acquisition
>>> from doppler.wfm import PN, mls_poly
>>> code = np.asarray(
...     PN(poly=mls_poly(9), seed=1, length=9).generate(511), np.uint8)
>>> a = Acquisition(code, sample_rate=2e6, chip_rate=1e6, cn0_dbhz=50.0)
>>> c = np.empty(a.lag_bins, dtype=np.float64)
>>> a.surface_chip_phase(c) == a.lag_bins
True
>>> bool(c[0] == 0.0 and c[1] == 511 - 511 / 1024)
True
```
 





        

<hr>



### function dp\_correlator\_surface\_complex 

_The last decided dwell's surface, complex: amplitude and carrier phase per cell, before the magnitude the gate reads._ 
```C++
size_t dp_correlator_surface_complex (
    dp_correlator_state_t * state,
    float _Complex * out,
    size_t n_out
) 
```



Copies the coherent sum the last dwell was decided on into `out`, row-major `surface_rows` x `lag_bins` like [**dp\_correlator\_surface()**](correlator__core_8h.md#function-dp_correlator_surface), in the correlation's own units rather than the gate's. A cell's phase is the carrier at the block's middle, relative to its tile's centre; its neighbours along the code axis are complex early and late arms, so a tracker can form the coherent discriminator `Re(conj(P) (L - E)) / |P|^2`, which the magnitude surface cannot (docs/design/async-dsss-receiver-measurements.md §12.21). Coherent path only: a non-coherent dwell (`n_noncoh > 1`) is a power sum with no phase, and reads 0.




**Parameters:**


* `state` Must be non-NULL. 
* `out` At least `surface_rows * lag_bins` complex floats. 
* `n_out` Capacity of `out`. 



**Returns:**

Cells written (`surface_rows * lag_bins`), or 0 when no dwell has been decided, the path is non-coherent, or `out` is too small. 
```C++
>>> import numpy as np
>>> from doppler.acquire import Acquisition
>>> from doppler.wfm import PN, mls_poly
>>> code = np.asarray(PN(poly=mls_poly(5), seed=1,
...                      length=5).generate(31)).astype(np.uint8)
>>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), 4).astype(
...     np.complex64)
>>> a = Acquisition(code, sample_rate=4e6, chip_rate=1e6, cn0_dbhz=70.0)
>>> a.n_noncoh                 # one look: the dwell is the coherent dump
1
>>> _ = a.push(np.roll(s0, 17).astype(np.complex64))
>>> sc = np.empty(a.surface_rows * a.lag_bins, dtype=np.complex64)
>>> a.surface_complex(sc) == sc.size
True
>>> int(np.argmax(np.abs(sc)) % a.lag_bins)   # the peak's lag column
18
```
 





        

<hr>



### function dp\_correlator\_surface\_doppler\_hz 

_The surface's Doppler axis: the frequency of each row, in Hz._ 
```C++
size_t dp_correlator_surface_doppler_hz (
    dp_correlator_state_t * state,
    double * out,
    size_t n_out
) 
```



One value per surface row, the fold and scale a hit's `doppler_hz_est` uses (dp\_fftfreq\_index() times `doppler_res_hz`, on the interpolated grid where the slow-time axis is interpolated), so a plot of [**dp\_correlator\_surface()**](correlator__core_8h.md#function-dp_correlator_surface) carries the same axis a DetectionEvent reports on.




**Parameters:**


* `state` Must be non-NULL. 
* `out` At least `surface_rows` doubles. 
* `n_out` Capacity of `out`. 



**Returns:**

Values written (`surface_rows`), or 0 if `out` is too small. 
```C++
>>> import numpy as np
>>> from doppler.acquire import Acquisition
>>> from doppler.wfm import PN, mls_poly
>>> code = np.asarray(
...     PN(poly=mls_poly(9), seed=1, length=9).generate(511), np.uint8)
>>> a = Acquisition(code, sample_rate=2e6, chip_rate=1e6, cn0_dbhz=50.0,
...                 doppler_uncertainty=4000.0)
>>> f = np.empty(a.surface_rows, dtype=np.float64)
>>> a.surface_doppler_hz(f) == a.surface_rows
True
>>> bool(f[0] == 0.0 and f.min() < 0.0 < f.max())
True
```
 





        

<hr>
## Public Static Functions Documentation




### function dp\_correlator\_bin\_doppler\_hz 

_A hit's signed Doppler, Hz: its_ `row` _folded over dp\_correlator\_grid\_bins() by numpy's fftfreq convention (dp\_fftfreq\_index) and scaled by the grid's resolution._
```C++
static inline double dp_correlator_bin_doppler_hz (
    const dp_correlator_state_t * state,
    size_t doppler_bin
) 
```



The one conversion from a bin to Hz; [**dp\_correlator\_build\_handoff()**](correlator__core_8h.md#function-dp_correlator_build_handoff) and every composing object call it rather than restating the fold. 


        

<hr>



### function dp\_correlator\_grid\_bins 

_The Doppler grid a hit's_ `row` _indexes:_`window_bins * coherent_bins` _bins._
```C++
static inline size_t dp_correlator_grid_bins (
    const dp_correlator_state_t * state
) 
```



One bin in a native search; the window-tile count once a `doppler_uncertainty` wider than the native span is tiled; tiles times the block depth on a continuous engine with code-only epochs. Every consumer that folds a `row` must fold it over THIS, not over `coherent_bins`: a capture that did read every tiled hit as 0 Hz (doppler#1512). 


        

<hr>
## Macro Definition Documentation





### define CORRELATOR\_STATE\_MAGIC 

_State-blob magic ('CORL'); set\_state() rejects any other._ 
```C++
#define CORRELATOR_STATE_MAGIC `DP_FOURCC ('C','O','R','L')`
```




<hr>



### define CORRELATOR\_STATE\_VERSION 

_State-blob layout version; set\_state() rejects any other._ 
```C++
#define CORRELATOR_STATE_VERSION `1u`
```




<hr>



### define DP\_CORRELATOR\_CN0\_NONE 

_The design C/N0 that means "none given" (doppler#1484): a burst engine then sizes for the whole preamble, its_ `pd_predicted` _is NaN and it is never_`underpowered` _._
```C++
#define DP_CORRELATOR_CN0_NONE `NAN`
```



NaN, because every finite value is a real design point  including a negative one: at `fs = 1` (normalized units) the C/N0 IS the per-sample SNR in dB, which is negative wherever acquisition is hard. It used to be 0, which made exactly 0 dB unreachable and a negative per-sample SNR unstatable. Test with isnan(), never ==. 


        

<hr>



### define DP\_CORRELATOR\_DELAY\_LOSS\_NODES 

```C++
#define DP_CORRELATOR_DELAY_LOSS_NODES `4`
```



Quadrature nodes the Pd model averages the delay straddle over. 


        

<hr>



### define DP\_CORRELATOR\_MAX\_PEAKS 

```C++
#define DP_CORRELATOR_MAX_PEAKS `64u`
```



The largest `max_peaks` [**dp\_correlator\_set\_max\_peaks()**](correlator__core_8h.md#function-dp_correlator_set_max_peaks) accepts: the bindings' push result arrays (1024) are at least this many, so one dwell can always be reported whole. 


        

<hr>



### define DP\_CORRELATOR\_N\_NONCOH\_SAFETY\_CEILING 

_Internal safety-valve ceiling on auto-selected non-coherent looks_  _not a public knob (no caller-facing equivalent of the retired_`max_noncoh` _parameter)._
```C++
#define DP_CORRELATOR_N_NONCOH_SAFETY_CEILING `256u`
```



The semi-analytical Pd model both auto-sizers ascend against turns non-monotonic and unreliable past a few hundred looks (this project's own geometry found ~256 empirically  see docs/design/async-dsss-receiver.md). Hitting this ceiling without meeting `pd` leaves [**dp\_correlator\_search\_t::underpowered**](structdp__correlator__search.md#variable-underpowered) set, same as any other infeasible operating point  no separate bookkeeping needed. 


        

<hr>



### define DP\_CORRELATOR\_RATE\_HYPS\_MAX 

```C++
#define DP_CORRELATOR_RATE_HYPS_MAX `64u`
```



Most Doppler-rate hypotheses an engine searches. Each one costs a full slow-time pass per dwell, so this bounds the cost of a typo, not a design: the residual rate falls as 1/(R-1), and a few dozen hypotheses already take a rate span out to depths no preamble repeats to. 


        

<hr>



### define DP\_CORRELATOR\_SIDELOBE\_DOPPLER\_PTS 

```C++
#define DP_CORRELATOR_SIDELOBE_DOPPLER_PTS `9`
```



[**dp\_correlator\_sidelobe\_ratio()**](correlator__core_8h.md#function-dp_correlator_sidelobe_ratio)'s Doppler points: across the emitter's native row, its own bin's width. 


        

<hr>



### define DP\_CORRELATOR\_SIDELOBE\_LAG\_PTS 

```C++
#define DP_CORRELATOR_SIDELOBE_LAG_PTS `5`
```



[**dp\_correlator\_sidelobe\_ratio()**](correlator__core_8h.md#function-dp_correlator_sidelobe_ratio)'s lag points: across the emitter's own lag column, one column (code\_bins / lag\_bins samples) wide. 


        

<hr>



### define DP\_CORRELATOR\_SIDELOBE\_SLOTS 

```C++
#define DP_CORRELATOR_SIDELOBE_SLOTS `32`
```



(hit, tile) rows [**dp\_correlator\_sidelobe\_ratio()**](correlator__core_8h.md#function-dp_correlator_sidelobe_ratio) keeps every column of: room for a pick's few stronger emitters on the tiles their cells touch. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/correlator/correlator_core.h`

