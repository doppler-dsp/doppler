

# File acq\_core.h



[**FileList**](files.md) **>** [**acq**](dir_25a1e6db36731e5901b5cfb158eaa462.md) **>** [**acq\_core.h**](acq__core_8h.md)

[Go to the source code of this file](acq__core_8h_source.md)

_Streaming DSSS acquisition engine — burst and continuous front doors over one shared engine._ [More...](#detailed-description)

* `#include "buffer/buffer.h"`
* `#include "clib_common.h"`
* `#include "corr2d/corr2d_core.h"`
* `#include "detection/detection_core.h"`
* `#include "dp_state.h"`
* `#include "fft/fft_core.h"`
* `#include "jm_perf.h"`
* `#include "detector2d/detector2d_core.h"`
* `#include "fft2d/fft2d_core.h"`
* `#include "dp_parallel.h"`
* `#include "dp_tlm/dp_tlm_core.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**acq\_extra\_t**](structacq__extra__t.md) <br>_Per-object extra header for an engine's cross-call state._  |
| struct | [**acq\_handoff\_t**](structacq__handoff__t.md) <br>_Wire-ready hand-off record built from one_ [_**acq\_result\_t**_](structacq__result__t.md) _hit._ |
| struct | [**acq\_part\_t**](structacq__part__t.md) <br>_One tile's share of a decided surface (design §2.3): the surface is cut into_ `window_bins` _chunks of whole rows, and the per-cell passes after the fan_ _the magnitude, the CFAR reference, the mask copy, each scan of the peak list_ _run per chunk into one of these, merged serially in tile order. The merge is bit-identical at any thread count because the chunks never move._ |
| struct | [**acq\_result\_t**](structacq__result__t.md) <br>_One acquisition detection event._  |
| struct | [**acq\_state\_t**](structacq__state__t.md) <br>_Streaming acquisition-engine state._  |
| struct | [**acq\_tlm\_t**](structacq__tlm__t.md) <br>_Telemetry attachment: a borrowed context + this engine's probe ids (design §2.4). NULL ctx (the default) means detached — the one probe site is then a single predicted-not-taken branch per decided dwell. Never in a state blob; preserved across_ [_**acq\_set\_state()**_](acq__core_8h.md#function-acq_set_state) _like the borrowed code._ |


## Public Types

| Type | Name |
| ---: | :--- |
| typedef void(\* | [**acq\_surface\_sink\_fn**](#typedef-acq_surface_sink_fn)  <br>_A surface sink: called once per decided dwell (every_ `decim` _-th) with the dwell's surface in test-statistic units, row-major_`rows` _x_`cols` _(Doppler rows by code-phase columns), and the dwell's_`samples_consumed` _. The pointer is the engine's and is valid only for the call. See_[_**acq\_set\_surface\_sink()**_](acq__core_8h.md#function-acq_set_surface_sink) _._ |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  size\_t | [**acq\_block\_prompt**](#function-acq_block_prompt) ([**acq\_state\_t**](structacq__state__t.md) \* state, size\_t tile, size\_t col, float \_Complex \* out, size\_t n\_out) <br>_One cell's column of the last whole block: the per-epoch complex correlations at a code phase, the despread stream at epoch rate._  |
|  size\_t | [**acq\_block\_raw**](#function-acq_block_raw) ([**acq\_state\_t**](structacq__state__t.md) \* state, float \_Complex \* out, size\_t n\_out) <br>_The last whole block's raw samples, as pushed._  |
|  void | [**acq\_build\_handoff**](#function-acq_build_handoff) (const [**acq\_state\_t**](structacq__state__t.md) \* state, const [**acq\_result\_t**](structacq__result__t.md) \* hit, size\_t code\_len, size\_t spc, [**acq\_handoff\_t**](structacq__handoff__t.md) \* out) <br>_Convert one_ [_**acq\_push()**_](acq__core_8h.md#function-acq_push) _hit into a wire-ready hand-off record._ |
|  int | [**acq\_configure\_search\_raw**](#function-acq_configure_search_raw) ([**acq\_state\_t**](structacq__state__t.md) \* state, size\_t doppler\_bins, size\_t n\_noncoh) <br>_Pin the search grid directly, bypassing both auto-sizing searches — the advanced escape hatch (mirrors Dll's/Costas's configure\_lock\_raw())._  |
|  [**acq\_state\_t**](structacq__state__t.md) \* | [**acq\_create\_burst**](#function-acq_create_burst) (const uint8\_t \* code, size\_t code\_len, size\_t reps, size\_t spc, double chip\_rate, double cn0\_dbhz, double doppler\_uncertainty, double pfa, double pd, int noise\_mode) <br>_Create a burst-mode acquisition engine: coherent multi-epoch combining, up to_ `reps` _deep (today's classic behavior)._ |
|  [**acq\_state\_t**](structacq__state__t.md) \* | [**acq\_create\_continuous**](#function-acq_create_continuous) (const uint8\_t \* code, size\_t code\_len, size\_t spc, double chip\_rate, double symbol\_rate, double cn0\_dbhz, double doppler\_uncertainty, double pfa, double pd, int noise\_mode, size\_t code\_only\_epochs, double doppler\_rate) <br>_Create a continuous-mode acquisition engine: always wideband window-tiling, allowing a block-coherent depth inside the tiles to accommodate waveforms with code-only windows._  |
|  void | [**acq\_destroy**](#function-acq_destroy) ([**acq\_state\_t**](structacq__state__t.md) \* state) <br>_Destroy and free an engine._  |
|  void | [**acq\_get\_state**](#function-acq_get_state) (const [**acq\_state\_t**](structacq__state__t.md) \* state, void \* blob) <br>_Serialize_ `state's` _cross-call state into_`blob` _(caller-owned,_[_**acq\_state\_bytes()**_](acq__core_8h.md#function-acq_state_bytes) _long). Call between pushes (no partial dump pending)._ |
|  size\_t | [**acq\_push**](#function-acq_push) ([**acq\_state\_t**](structacq__state__t.md) \* state, const float \_Complex \* x, size\_t n\_in, [**acq\_result\_t**](structacq__result__t.md) \* result, size\_t max\_results) <br>_Stream raw samples; emit one event per CFAR dump above threshold._  |
|  void | [**acq\_reset**](#function-acq_reset) ([**acq\_state\_t**](structacq__state__t.md) \* state) <br>_Drain the input ring and reset the coherent accumulator._  |
|  size\_t | [**acq\_run**](#function-acq_run) ([**acq\_state\_t**](structacq__state__t.md) \* state, const void \* state\_in, void \* state\_out, const float \_Complex \* in, size\_t n\_in, [**acq\_result\_t**](structacq__result__t.md) \* result, size\_t max\_results) <br>_Pure run: inject_ `state_in` _, stream_`in` _, emit hits, export_`state_out` _—_`(state_in, input) -> (state_out, output)` _over an engine treated as immutable config + scratch._`state_in` _/_`state_out` _may alias. Either may be NULL (NULL in = fresh; NULL out = discard)._ |
|  int | [**acq\_set\_carrier\_freq\_hz**](#function-acq_set_carrier_freq_hz) ([**acq\_state\_t**](structacq__state__t.md) \* state, double carrier\_freq\_hz) <br>_Couple the code clock to the carrier: the chip rate dilates by_ `doppler_hz / carrier_freq_hz` _, and the engine accounts for it._ |
|  int | [**acq\_set\_max\_peaks**](#function-acq_set_max_peaks) ([**acq\_state\_t**](structacq__state__t.md) \* state, size\_t n) <br>_How many peaks a dwell may report: the peak list's capacity._  |
|  int | [**acq\_set\_state**](#function-acq_set_state) ([**acq\_state\_t**](structacq__state__t.md) \* state, const void \* blob) <br>_Restore cross-call state from_ `blob` _into_`state` _(replacing it)._ |
|  void | [**acq\_set\_surface\_sink**](#function-acq_set_surface_sink) ([**acq\_state\_t**](structacq__state__t.md) \* state, [**acq\_surface\_sink\_fn**](acq__core_8h.md#typedef-acq_surface_sink_fn) fn, void \* ctx, uint32\_t decim) <br>_Attach (or detach) a C surface sink: every_ `decim-th` _decided dwell's surface, in test-statistic units, handed to_`fn` _on the pushing thread (design §2.4)._ |
|  int | [**acq\_set\_telemetry**](#function-acq_set_telemetry) ([**acq\_state\_t**](structacq__state__t.md) \* state, [**dp\_tlm\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_t) \* tlm, const char \* prefix, uint32\_t decim) <br>_Attach (or detach) a telemetry context and register the engine's probes on it (design §2.4)._  |
|  int | [**acq\_set\_threads**](#function-acq_set_threads) ([**acq\_state\_t**](structacq__state__t.md) \* state, int n) <br>_Set how many threads the searcher fans its tiles across (design §2.3: a roll per thread on persistent workers)._  |
|  size\_t | [**acq\_state\_bytes**](#function-acq_state_bytes) (const [**acq\_state\_t**](structacq__state__t.md) \* state) <br>_Byte size of_ `state's` _blob (header + unconsumed + nc)._ |
|  size\_t | [**acq\_surface**](#function-acq_surface) ([**acq\_state\_t**](structacq__state__t.md) \* state, float \* out, size\_t n\_out) <br>_The last decided dwell's surface, in the gate's own units._  |
|  size\_t | [**acq\_surface\_chip\_phase**](#function-acq_surface_chip_phase) ([**acq\_state\_t**](structacq__state__t.md) \* state, double \* out, size\_t n\_out) <br>_The surface's code-phase axis: the chip phase of each column._  |
|  size\_t | [**acq\_surface\_complex**](#function-acq_surface_complex) ([**acq\_state\_t**](structacq__state__t.md) \* state, float \_Complex \* out, size\_t n\_out) <br>_The last decided dwell's surface, complex: amplitude and carrier phase per cell, before the magnitude the gate reads._  |
|  size\_t | [**acq\_surface\_doppler\_hz**](#function-acq_surface_doppler_hz) ([**acq\_state\_t**](structacq__state__t.md) \* state, double \* out, size\_t n\_out) <br>_The surface's Doppler axis: the frequency of each row, in Hz._  |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**ACQ\_COL\_CHUNK**](acq__core_8h.md#define-acq_col_chunk)  `32u`<br> |
| define  | [**ACQ\_MAX\_PEAKS**](acq__core_8h.md#define-acq_max_peaks)  `64u`<br> |
| define  | [**ACQ\_N\_NONCOH\_SAFETY\_CEILING**](acq__core_8h.md#define-acq_n_noncoh_safety_ceiling)  `256u`<br>_Internal safety-valve ceiling on auto-selected non-coherent looks_  _not a public knob (no caller-facing equivalent of the retired_`max_noncoh` _parameter)._ |
| define  | [**ACQ\_STATE\_MAGIC**](acq__core_8h.md#define-acq_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('A', 'C', 'Q', 'R')`<br> |
| define  | [**ACQ\_STATE\_VERSION**](acq__core_8h.md#define-acq_state_version)  `4u /\* v4: the block's raw epochs ride beside it \*/`<br> |

## Detailed Description


Acquires a direct-sequence spread-spectrum signal — repeated, BPSK -modulated PN-code segments — arriving with an unknown integer code phase and an unknown carrier-frequency (Doppler) offset, buried in AWGN. It jointly estimates the (Doppler bin, code phase) and declares a detection whenever the CFAR test statistic crosses an automatically configured threshold.


Pipeline (owned end to end, one object): push(raw cf32) -&gt; ring buffer -&gt; reframe to (coherent\_bins, code\_bins) -&gt; slow-time Doppler FFT (FFT along the segment axis) -&gt; 2-D code correlation against a single-row PN reference (corr2d) -&gt; argmax + CFAR noise estimate -&gt; threshold gate -&gt; [**acq\_result\_t**](structacq__result__t.md).


The fast-time axis (code\_bins = sf\*spc columns) is the circular code matched filter; the slow-time axis (coherent\_bins rows, one row per code repetition) is the coherent Doppler search. A carrier offset f (cycles/sample) lands the peak at row = round(f\*code\_bins\*coherent\_bins) mod coherent\_bins, column = code phase.


**Two mode-fixed public constructors, one shared engine.** A coherent slow-time Doppler FFT can only ever resolve frequency _within_ one native span `chip_rate/(2*sf)` — more coherent depth subdivides that SAME fixed range more finely, it never widens it — and for a continuous (async, data-modulated) signal, a multi-epoch coherent axis wide enough to matter aliases the data's own bit transitions across the whole Doppler-bin axis (a structural mislock, not a graceful SNR loss — see docs/design/dsss-acquisition.md). So the two constructors fix a mode each, never a per-call knob:



* [**acq\_create\_burst()**](acq__core_8h.md#function-acq_create_burst) — today's classic behavior: the smallest coherent depth `coherent_bins` in `[1, reps]` whose coherent\_bins\*code\_bins coherent samples meet `pd` (det\_threshold / det\_pd) — minimum latency for a strong signal, unmodulated bursts/preambles only. A tighter `doppler_uncertainty` shrinks the searched cell count, lowering the Bonferroni threshold (more sensitive). When `doppler_uncertainty` exceeds the native span, falls back to the wideband window-tiling mechanism below instead (coherent depth structurally can't cover more than one span, regardless of mode).
* [**acq\_create\_continuous()**](acq__core_8h.md#function-acq_create_continuous) — for a continuous, data-modulated signal: ALWAYS uses the wideband window-tiling mechanism below, unconditionally (never attempts coherent multi-epoch combining, even when `doppler_uncertainty` is narrower than one native span) — closes the aliasing footgun structurally rather than pricing it as a tunable loss. Sensitivity margin comes entirely from auto-selected non-coherent looks.




Both convert C/N0 to a per-sample amplitude SNR (snr = sqrt(10^(cn0\_dbhz/10) / (chip\_rate\*spc))). Every reported detection inverts this same relationship to report an estimated C/N0 ([**acq\_result\_t::cn0\_dbhz\_est**](structacq__result__t.md#variable-cn0_dbhz_est)) — a bandwidth/integration-time-independent figure of merit directly comparable to `cn0_dbhz`, unlike a raw per-sample or coherently-integrated ratio (both scale with `spc/ reps and` so aren't portable across configurations).


**Wideband window-tiling mode**: instead of coherent combining, tiles the requested uncertainty with `window_bins = ceil(doppler_uncertainty / (chip_rate/(2*sf)))` parallel frequency-window hypotheses, each one native span wide, searched every epoch from a SINGLE shared forward FFT of that epoch: hypothesis r's spectrum is the shared FFT circularly rolled by r bins (exact — the window spacing IS this code\_bins-point FFT's own bin spacing) against one fixed precomputed replica spectrum, then inverse-FFT'd — `window_bins` inverse FFTs plus the one shared forward FFT per epoch, not `window_bins` independent down-conversions. Empirically the cheaper of the two realizations benchmarked for this (a frequency-bank benchmark): ~1.2x-1.55x faster than an equivalent tuned-mixer bank, measured with real doppler.spectral.FFT. SNR margin in this mode comes entirely from auto-selected non-coherent looks (magnitude-squared accumulation, immune to data-modulation sign flips) rather than coherent depth, sized against an internal safety-valve ceiling rather than a caller-supplied cap (the semi-analytical Pd model this engine sizes against grows unreliable past a few hundred looks — not a public knob to tune around that). `doppler_bin` in [**acq\_result\_t**](structacq__result__t.md) reports the frequency-window index (0 … window\_bins-1, native FFT-bin ordering) instead of a slow-time-FFT row when this mode is active; `doppler_res_hz` reports the per-window spacing (chip\_rate/sf) at coherent\_bins=1.


**Block-coherent depth inside the tiles** (docs/design/async-dsss-receiver.md §2.3): the engine allows a coherent depth, to accommodate waveforms with code-only windows. Given `code_only_epochs > 1`  the whole code-only epochs such a window holds at any chip phase  it runs a coherent depth `D` inside every tile: the per-tile epoch correlations are gathered for `D` epochs, then a zero-padded slow-time FFT per code-phase column turns each tile into `D` Doppler rows `chip_rate/(sf*D)` apart, detected per block. Blocks are non-overlapping and the engine does not know any emitter's window phase, so `D` is at most `(code_only_epochs+1)/2` (a whole block always lands inside the window) and, when `doppler_rate` is given, at most `f_epoch/sqrt(2*doppler_rate)` (the drift over a block stays inside half a row). The Doppler axis is then ONE uniform grid of `window_bins*coherent_bins` bins of `doppler_res_hz = chip_rate/(sf*D)` over the tiled span, in native FFT-bin order (0 = DC, ascending, then wrapping negative): `doppler_bin` indexes it, and [**acq\_build\_handoff()**](acq__core_8h.md#function-acq_build_handoff) folds it with dp\_fftfreq\_index() over that count. A block that straddles data spreads that emitter over its rows, `10*log10(D)` below an aligned block, at its own code phase  the `conc` probe (§2.4) reads it. `code_only_epochs = 1` (the default) is `D = 1` and the engine exactly as described above. A tile de-rotates by its own centre, so an emitter on the edge between two tiles reads the same in both (within 0.03 dB) and the slow-time transform folds it to the same row index of each: every listed peak is therefore asked at its ROW's frequency before it is reported  the block's raw epochs correlated with the replica at the pick's code phase, mixed by the row's frequency and by that one span down and up, the winner reported (design §2.3, docs/design/async-dsss-receiver-measurements.md §12.18, doppler#1270).


**A roll per thread** (design §2.3): the tiles are independent after the one forward transform, so the per-epoch tile loop and, at `D > 1`, the block-end column loop run across a persistent pool of workers ([**dp\_parallel.h**](dp__parallel_8h.md)'s `dp_pool_*`), created once with the engine and parked between pushes. Each tile owns its inverse plan and scratch, so the result is bit-identical at any thread count. The per-cell passes that decide a surface  the magnitude, the CFAR reference, the working mask and every scan of the peak list  run per tile too, each into a slot of its own, and are merged serially in tile order (a mean of the tiles' means over equal cells, the first of the tiles' first maxima), so the serial remainder is per tile, not per cell. [**acq\_set\_threads()**](acq__core_8h.md#function-acq_set_threads) sets the count.



```C++
// 31-chip PN, 4x oversample, up to 16 coherent reps; 1 MHz chips, 45 dB-Hz
uint8_t code[31] = { 0 };   // ... fill with PN chips (0/1) ...
acq_state_t *a = acq_create_burst(code, 31, 16, 4, 1.0e6, 45.0,
                                  0.0, 1e-3, 0.9, 0);
acq_result_t hits[64];
size_t nh = acq_push(a, samples, n_samples, hits, 64);
for (size_t i = 0; i < nh; i++)
  printf("Doppler %zu, code phase %zu, C/N0 %.1f dB-Hz\n",
         hits[i].doppler_bin, hits[i].code_phase,
         hits[i].cn0_dbhz_est);
acq_destroy(a);
```
 


    
## Public Types Documentation




### typedef acq\_surface\_sink\_fn 

_A surface sink: called once per decided dwell (every_ `decim` _-th) with the dwell's surface in test-statistic units, row-major_`rows` _x_`cols` _(Doppler rows by code-phase columns), and the dwell's_`samples_consumed` _. The pointer is the engine's and is valid only for the call. See_[_**acq\_set\_surface\_sink()**_](acq__core_8h.md#function-acq_set_surface_sink) _._
```C++
typedef void(* acq_surface_sink_fn) (void *ctx, const float *surface, size_t rows, size_t cols, uint64_t samples_consumed);
```




<hr>
## Public Functions Documentation




### function acq\_block\_prompt 

_One cell's column of the last whole block: the per-epoch complex correlations at a code phase, the despread stream at epoch rate._ 
```C++
size_t acq_block_prompt (
    acq_state_t * state,
    size_t tile,
    size_t col,
    float _Complex * out,
    size_t n_out
) 
```



The block-coherent engine gathers every tile's correlation row for `coherent_bins` epochs before its slow-time transform (file doc, design §2.3). This copies the `coherent_bins` values at column `col` of tile `tile` into `out`, in epoch order: each epoch's complex prompt at that code phase, rolled to the tile's centre frequency and shifted to the block's middle by the tile's code-rate hypothesis, so the phase is continuous along the column for an emitter at the tile's centre and rotates at its offset from it. At an emitter's cell this is what a despreader produces, one value per epoch, phase included (docs/design/async-dsss-receiver-measurements.md §12.21). Valid once a block is whole, until the next epoch is pushed.




**Parameters:**


* `state` Must be non-NULL. 
* `tile` Tile index, `0 … window_bins-1` (native FFT order, the order the surface's rows are cut in). 
* `col` Code-phase column, `0 … code_bins-1`. 
* `out` At least `coherent_bins` complex floats. 
* `n_out` Capacity of `out`. 



**Returns:**

Values written (`coherent_bins`), or 0 at `coherent_bins == 1` (no block is gathered), while a block is partial, for an index out of range, or when `out` is too small. 
```C++
>>> import numpy as np
>>> from doppler.dsss import Acquisition
>>> from doppler.wfm import PN, mls_poly
>>> code = np.asarray(PN(poly=mls_poly(5), seed=1,
...                      length=5).generate(31)).astype(np.uint8)
>>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), 4).astype(
...     np.complex64)
>>> b = Acquisition(code, spc=4, chip_rate=1e6, cn0_dbhz=60.0,
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



### function acq\_block\_raw 

_The last whole block's raw samples, as pushed._ 
```C++
size_t acq_block_raw (
    acq_state_t * state,
    float _Complex * out,
    size_t n_out
) 
```



Copies the `coherent_bins * code_bins` samples the block-coherent engine gathered for its last whole block into `out`, epoch by epoch in stream order — the samples [**acq\_push()**](acq__core_8h.md#function-acq_push) consumed, untouched. Kept for the tile-edge re-ask (acq\_resolve\_tile\_alias()); exposed so a tracker can re-correlate them at any code phase, rate or symbol boundary the engine's own grid does not have — a symbol-rate despreader at the tracked timing runs on exactly this (docs/design/async-dsss-receiver-measurements.md §12.21). Valid once a block is whole, until the next epoch is pushed.




**Parameters:**


* `state` Must be non-NULL. 
* `out` At least `coherent_bins * code_bins` complex floats. 
* `n_out` Capacity of `out`. 



**Returns:**

Samples written (`coherent_bins * code_bins`), or 0 at `coherent_bins == 1`, while a block is partial, or when `out` is too small. 
```C++
>>> import numpy as np
>>> from doppler.dsss import Acquisition
>>> from doppler.wfm import PN, mls_poly
>>> code = np.asarray(PN(poly=mls_poly(5), seed=1,
...                      length=5).generate(31)).astype(np.uint8)
>>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), 4).astype(
...     np.complex64)
>>> b = Acquisition(code, spc=4, chip_rate=1e6, cn0_dbhz=60.0,
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



### function acq\_build\_handoff 

_Convert one_ [_**acq\_push()**_](acq__core_8h.md#function-acq_push) _hit into a wire-ready hand-off record._
```C++
void acq_build_handoff (
    const acq_state_t * state,
    const acq_result_t * hit,
    size_t code_len,
    size_t spc,
    acq_handoff_t * out
) 
```



Two convention inversions live here, ported verbatim from `dsss_receiver_core.c`'s own (pre-existing, now-shared) handoff logic:



* **Chip phase**: `hit's` `code_phase` is a correlation LAG (0 … code\_bins-1); a code-tracking loop's `init_chip` wants the code's own instantaneous phase instead — the mirror-image inversion `phase = fmod(code_len - code_phase/spc, code_len)`, folded non-negative.
* **Doppler**: `state` is assumed built via [**acq\_create\_continuous()**](acq__core_8h.md#function-acq_create_continuous) (coherent\_bins pinned at 1, `window_bins` the active mechanism, the only mode this function supports), so `hit`'s `doppler_bin` is a frequency-WINDOW index, mapped to a signed bin by `dp_fftfreq_index()` — the SAME helper the search uses — and scaled by `state->doppler_res_hz`.
* **The dwell's dilation** (doppler#1254): a hit is decided on a non-coherent sum over `n_noncoh` looks, and the code phase it reports is that sum's peak  the phase at the MIDDLE of the dwell, not at its end, when the chip clock is dilated by the same Doppler the hit reports (a physically-coupled carrier, `doppler_hz / carrier_freq_hz` chips per chip). The seed a code loop wants is the phase at the next sample, so with the carrier set ([**acq\_set\_carrier\_freq\_hz()**](acq__core_8h.md#function-acq_set_carrier_freq_hz)) the phase is advanced by the drift over HALF the dwell, `doppler_hz_est / carrier_freq_hz * n_noncoh * coherent_bins * code_len / 2` chips (a coherent block's epochs are aligned to its middle by the same setting, so the block's peak is its middle too). At SPEC's 20 ppm the continuous engine's dwell at 45 dB-Hz is 15 epochs (0.15 chip, inside any code loop's pull-in) and at the 40 dB-Hz floor 88 epochs  0.9 chip, measured directly, past the refine Dll's; without this the floor's hand-offs never refined. Uncoupled (0.0): no advance. 

**Parameters:**


  * `state` The engine `hit` came from (non-NULL, built via [**acq\_create\_continuous()**](acq__core_8h.md#function-acq_create_continuous)). 
  * `hit` One hit from [**acq\_push()**](acq__core_8h.md#function-acq_push) (non-NULL). 
  * `code_len` Spreading-code length (chips) — the same value passed to whichever acq\_create\_\*() built `state`. 
  * `spc` Samples/chip — likewise. 
  * `out` Written on return (non-NULL). 






        

<hr>



### function acq\_configure\_search\_raw 

_Pin the search grid directly, bypassing both auto-sizing searches — the advanced escape hatch (mirrors Dll's/Costas's configure\_lock\_raw())._ 
```C++
int acq_configure_search_raw (
    acq_state_t * state,
    size_t doppler_bins,
    size_t n_noncoh
) 
```



Resizes every buffer/plan that depends on the grid (the slow-time FFT, the code correlator, the reference, and every per-frame scratch buffer), re-derives the threshold ladder for the pinned grid from the same physics [**acq\_create\_burst()**](acq__core_8h.md#function-acq_create_burst)/acq\_create\_continuous() used, and clears in-flight accumulation (ring contents, the non-coherent power accumulator, dwell bookkeeping) — call between push() calls, never a substitute for one.




**Parameters:**


* `state` Allocated engine (non-NULL). 
* `doppler_bins` Coherent depth to pin, in `[1, reps]`. 
* `n_noncoh` Non-coherent look count to pin, in `[1, ACQ_N_NONCOH_SAFETY_CEILING]`. 



**Returns:**

0 on success, -1 if either argument is out of range or an allocation fails (the engine is left usable at its prior grid on failure). 
```C++
>>> import numpy as np
>>> from doppler.dsss import Acquisition
>>> from doppler.wfm import PN, mls_poly
>>> code = np.asarray(PN(poly=mls_poly(5), seed=1,
...                      length=5).generate(31)).astype(np.uint8)
>>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), 4).astype(
...     np.complex64)
>>> a = Acquisition(code, spc=4, chip_rate=1e6, cn0_dbhz=50.0)
>>> a.configure_search_raw(doppler_bins=1, n_noncoh=4)  # pin the grid
>>> a.doppler_bins, a.n_noncoh
(1, 4)
>>> burst = np.tile(np.roll(s0, 17), 4).astype(np.complex64)
>>> a.push(burst)[0][:2]      # detects at the pinned grid
(0, 17)
```
 





        

<hr>



### function acq\_create\_burst 

_Create a burst-mode acquisition engine: coherent multi-epoch combining, up to_ `reps` _deep (today's classic behavior)._
```C++
acq_state_t * acq_create_burst (
    const uint8_t * code,
    size_t code_len,
    size_t reps,
    size_t spc,
    double chip_rate,
    double cn0_dbhz,
    double doppler_uncertainty,
    double pfa,
    double pd,
    int noise_mode
) 
```



Builds the single-row oversampled BPSK reference from `code`, infers sf = `code_len`, converts `cn0_dbhz` to a per-sample amplitude SNR (snr = sqrt(10^(cn0\_dbhz/10) / (chip\_rate\*spc))), and picks the _smallest_ coherent depth `coherent_bins` in `[1, reps]` whose coherent\_bins\*code\_bins coherent samples meet `pd` at the Bonferroni threshold (minimum latency for a strong signal). If the full ceiling still falls short the engine is `underpowered`; it does NOT add non-coherent looks. A burst has one frame of preamble, so looks beyond it add noise to the statistic and move the hit  `samples_consumed` is stamped at the end of the LAST accumulated look, so a consumer resolving the preamble's position sees an anchor up to n\_noncoh\*coherent\_bins periods late (doppler#1181). Intended for an unmodulated burst or preamble window  a continuous, data-modulated signal should use [**acq\_create\_continuous()**](acq__core_8h.md#function-acq_create_continuous) instead (coherent combining under continuous data is a structural aliasing mislock, not a tunable SNR trade-off  see the file doc comment).


`cn0_dbhz` is the DESIGN (minimum) C/N0 and is optional: 0 means none was given, and the engine then integrates the whole preamble (`coherent_bins = reps`) with the threshold set by `pfa` alone. `pd` is a sizing target only when a design C/N0 is given; without one `pd_predicted` is NAN and `underpowered` is never set.


A tighter `doppler_uncertainty` narrows the scanned Doppler band, lowering the per-cell threshold (more sensitive). When `doppler_uncertainty` exceeds the native span `chip_rate/(2*sf)`, falls back to the wideband window-tiling mechanism (see the file doc comment) instead  coherent depth structurally can't cover more than one native span, regardless of `reps`. Use [**acq\_configure\_search\_raw()**](acq__core_8h.md#function-acq_configure_search_raw) to pin the grid directly instead of relying on this auto-sizer.




**Parameters:**


* `code` PN chips (0/1), length `code_len`. 
* `code_len` Number of chips supplied (= sf, the spreading factor). 
* `reps` Max coherent code repetitions, the coherence ceiling (&gt;=1). 
* `spc` Samples per chip (&gt;= 1). 
* `chip_rate` Chip rate in Hz (&gt; 0). 
* `cn0_dbhz` Design carrier-to-noise density in dB-Hz (&gt;= 0; 0 = no design point, size for the whole preamble). 
* `doppler_uncertainty` One-sided Doppler search half-range in Hz; 0 uses the full native span +/- chip\_rate/(2\*sf). A value greater than the native span engages wideband mode (see the file doc comment above): coherent\_bins is forced to 1 and the uncertainty is tiled with parallel frequency-window hypotheses instead. 
* `pfa` Target system (max-of-N) false-alarm probability (0,1). 
* `pd` Target detection probability (0,1); a sizing target only when `cn0_dbhz` is given. 
* `noise_mode` CFAR mode index: 0=mean, 1=median, 2=min, 3=max. 



**Returns:**

Heap-allocated state, or NULL on bad arguments / allocation failure. 





        

<hr>



### function acq\_create\_continuous 

_Create a continuous-mode acquisition engine: always wideband window-tiling, allowing a block-coherent depth inside the tiles to accommodate waveforms with code-only windows._ 
```C++
acq_state_t * acq_create_continuous (
    const uint8_t * code,
    size_t code_len,
    size_t spc,
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



Builds the single-row oversampled BPSK reference from `code`, infers sf = `code_len`, converts `cn0_dbhz` to a per-sample amplitude SNR, and ALWAYS tiles `window_bins = max(1, ceil(doppler_uncertainty / (chip_rate/(2*sf))))` parallel frequency-window hypotheses (see the file doc comment's "Wideband window-tiling mode")  unconditionally, even when `doppler_uncertainty` is narrower than one native span. A continuous, data-modulated signal's own bit transitions make coherent multi-epoch combining across DATA a structural aliasing mislock (see docs/design/dsss-acquisition.md), so the depth is bounded by what a waveform's code-only window holds: `coherent_bins = D = min((code_only_epochs+1)/2, f_epoch/sqrt(2*doppler_rate))`, at least 1, run in non-overlapping D-epoch blocks inside every tile (file doc, design §2.3). With `code_only_epochs` = 1 it is 1 and the engine is exactly the epoch-by-epoch search. Sensitivity margin beyond the depth comes from auto-selected non-coherent looks over blocks (up to the internal [**ACQ\_N\_NONCOH\_SAFETY\_CEILING**](acq__core_8h.md#define-acq_n_noncoh_safety_ceiling)).




**Parameters:**


* `code` PN chips (0/1), length `code_len`. 
* `code_len` Number of chips supplied (= sf, the spreading factor). 
* `spc` Samples per chip (&gt;= 1). 
* `chip_rate` Chip rate in Hz (&gt; 0). 
* `symbol_rate` Continuous data-symbol rate in Hz; &lt;= 0 means no known clock. Diagnostic only (exposed via [**acq\_state\_t::epochs\_per\_symbol**](structacq__state__t.md#variable-epochs_per_symbol)), doesn't feed sizing: this engine never coherently combines regardless of the data-modulation clock. 
* `cn0_dbhz` Carrier-to-noise density in dB-Hz (&gt; 0). 
* `doppler_uncertainty` One-sided Doppler search half-range in Hz; 0 uses the full native span +/- chip\_rate/(2\*sf) (still window-tiled, at window\_bins=1). 
* `pfa` Target system (max-of-N) false-alarm probability (0,1). 
* `pd` Target detection probability (0,1). 
* `noise_mode` CFAR mode index: 0=mean, 1=median, 2=min, 3=max. 
* `code_only_epochs` Whole code-only epochs a waveform's code-only window holds at any chip phase (design §2.1: `floor(W_symbols * chips_per_symbol / sf) - 1`); 1 (&gt;= 1) means no window and a depth of 1. 
* `doppler_rate` Doppler rate in Hz/s the depth is bounded against (&gt;= 0); 0 leaves the window as the only bound. 



**Returns:**

Heap-allocated state, or NULL on bad arguments / allocation failure. 
```C++
>>> import numpy as np
>>> from doppler.dsss import Acquisition
>>> from doppler.wfm import PN, mls_poly
>>> code = np.asarray(PN(poly=mls_poly(5), seed=1,
...                      length=5).generate(31)).astype(np.uint8)
>>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), 4).astype(
...     np.complex64)
>>> burst = np.tile(np.roll(s0, 17), 23).astype(np.complex64)
>>> a = Acquisition(code, spc=4, chip_rate=1e6, cn0_dbhz=50.0)
>>> a.push(burst)[0][:2]    # detects (Doppler-window bin, code phase)
(0, 17)
>>> a.coherent_bins            # no window given: one epoch
1
>>> b = Acquisition(code, spc=4, chip_rate=1e6, cn0_dbhz=50.0,
...                 code_only_epochs=7)
>>> b.coherent_bins            # (7 + 1) // 2: a whole block fits
4
```
 





        

<hr>



### function acq\_destroy 

_Destroy and free an engine._ 
```C++
void acq_destroy (
    acq_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function acq\_get\_state 

_Serialize_ `state's` _cross-call state into_`blob` _(caller-owned,_[_**acq\_state\_bytes()**_](acq__core_8h.md#function-acq_state_bytes) _long). Call between pushes (no partial dump pending)._
```C++
void acq_get_state (
    const acq_state_t * state,
    void * blob
) 
```




<hr>



### function acq\_push 

_Stream raw samples; emit one event per CFAR dump above threshold._ 
```C++
size_t acq_push (
    acq_state_t * state,
    const float _Complex * x,
    size_t n_in,
    acq_result_t * result,
    size_t max_results
) 
```



Buffers `x`, then for every complete frame applies the slow-time Doppler FFT, correlates against the PN reference, dumps the coherent surface (or, when n\_noncoh &gt; 1, accumulates \|·\|² over n\_noncoh looks first), gates the peak on the auto-configured threshold, and appends an [**acq\_result\_t**](structacq__result__t.md). Each event carries the peak's Doppler bin and code phase (the two search axes), its CFAR statistic, and an estimated C/N0 — see [**acq\_result\_t**](structacq__result__t.md).




**Parameters:**


* `state` Allocated engine (non-NULL). 
* `x` Raw input, interleaved CF32, `n_in` complex samples. 
* `n_in` Number of complex input samples. 
* `result` Output array for detection events. 
* `max_results` Capacity of `result`. 



**Returns:**

Number of events written (0 … max\_results). 
```C++
>>> import numpy as np
>>> from doppler.dsss import Acquisition
>>> from doppler.wfm import PN, mls_poly
>>> code = np.asarray(PN(poly=mls_poly(5), seed=1,
...                      length=5).generate(31)).astype(np.uint8)
>>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), 4).astype(
...     np.complex64)
>>> a = Acquisition(code, spc=4, chip_rate=1e6, cn0_dbhz=50.0,
...                 doppler_uncertainty=40e3)
>>> fs = 1e6 * 4                    # sample rate = chip_rate * spc
>>> t = np.arange(a.code_bins * a.n_noncoh)
>>> carrier = np.exp(2j * np.pi * (a.doppler_res_hz / fs) * t)
>>> sig = (np.tile(np.roll(s0, 17), a.n_noncoh)
...        * carrier).astype(np.complex64)
>>> a.push(sig)[0][:2]              # (Doppler-window bin, code phase)
(1, 17)
```
 





        

<hr>



### function acq\_reset 

_Drain the input ring and reset the coherent accumulator._ 
```C++
void acq_reset (
    acq_state_t * state
) 
```



Discards any buffered samples that have not yet completed a frame and clears the non-coherent power accumulator and dwell bookkeeping, so the next push() begins a fresh search from an empty ring. The construction parameters — grid, thresholds, and PN reference — are untouched; only the in-flight streaming state is dropped.




**Parameters:**


* `state` Must be non-NULL. 
```C++
>>> import numpy as np
>>> from doppler.dsss import Acquisition
>>> from doppler.wfm import PN, mls_poly
>>> code = np.asarray(PN(poly=mls_poly(5), seed=1,
...                      length=5).generate(31)).astype(np.uint8)
>>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), 4).astype(
...     np.complex64)
>>> burst = np.tile(np.roll(s0, 17), 23).astype(np.complex64)
>>> a = Acquisition(code, spc=4, chip_rate=1e6, cn0_dbhz=50.0)
>>> _ = a.push(burst[:100])   # a partial frame, buffered mid-stream
>>> a.reset()                 # drop it before it can bias a detection
>>> a.push(burst)[0][:2]      # (Doppler bin, code phase)
(0, 17)
```
 




        

<hr>



### function acq\_run 

_Pure run: inject_ `state_in` _, stream_`in` _, emit hits, export_`state_out` _—_`(state_in, input) -> (state_out, output)` _over an engine treated as immutable config + scratch._`state_in` _/_`state_out` _may alias. Either may be NULL (NULL in = fresh; NULL out = discard)._
```C++
size_t acq_run (
    acq_state_t * state,
    const void * state_in,
    void * state_out,
    const float _Complex * in,
    size_t n_in,
    acq_result_t * result,
    size_t max_results
) 
```





**Returns:**

Number of events written (0 … max\_results). 





        

<hr>



### function acq\_set\_carrier\_freq\_hz 

_Couple the code clock to the carrier: the chip rate dilates by_ `doppler_hz / carrier_freq_hz` _, and the engine accounts for it._
```C++
int acq_set_carrier_freq_hz (
    acq_state_t * state,
    double carrier_freq_hz
) 
```



A physically-coupled Doppler moves the code as well as the carrier  100 chips/s at 20 ppm of 5 Mcps  and the engine's two long integrations both smear over it (doppler#1256, #1254):
* **Inside a coherent block** of D epochs every tile's epoch correlations are shifted along the code axis by the drift the tile's own frequency implies, `f_tile / carrier` chips per chip, aligned to the block's middle, before the slow-time transform (a linear phase on each epoch's product, exact to a fraction of a sample). Measured at SPEC's 20 ppm with D = 154 (3.1 chips of drift across the block): without it the block's peak is 13 dB down and 3 chips wide and the depth detects nothing at 34 dB-Hz; with it the block reads as a still one.
* **The hand-off** ([**acq\_build\_handoff()**](acq__core_8h.md#function-acq_build_handoff)) advances the hit's code phase by the drift over half the dwell  the non-coherent sum's peak is the phase at the dwell's middle, the seed is wanted at its end: 0.9 chip at the 40 dB-Hz floor, past a refine loop's pull-in.




Config, not running state: it is not in the state blob, so a resumed engine wants it set again by its holder, as at create. Default 0.0 (uncoupled) is the engine exactly as it ran without it.




**Parameters:**


* `state` Must be non-NULL. 
* `carrier_freq_hz` RF carrier, Hz; 0.0 = uncoupled. 



**Returns:**

`DP_OK`, or `DP_ERR_INVALID` for a negative or non-finite value. 
```C++
>>> import numpy as np
>>> from doppler.dsss import Acquisition
>>> from doppler.wfm import Gold
>>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
>>> a = Acquisition(code, spc=2, chip_rate=5e6, symbol_rate=2700.0,
...                 cn0_dbhz=45.0, doppler_uncertainty=50e3)
>>> a.carrier_freq_hz
0.0
>>> a.set_carrier_freq_hz(2.5e9)
>>> a.carrier_freq_hz
2500000000.0
```
 





        

<hr>



### function acq\_set\_max\_peaks 

_How many peaks a dwell may report: the peak list's capacity._ 
```C++
int acq_set_max_peaks (
    acq_state_t * state,
    size_t n
) 
```



One (the default) is the classic detector  the maximum of the surface, gated. More is the list of docs/design/async-dsss-receiver.md §7.1: every peak above the same gate, strongest first, each with an exclusion zone of one Doppler bin by one chip around it (one emitter's main lobe, so its own shoulders are not the next peak), and the two-epoch rule for a peak at an already-listed code phase  a data transition inside the epoch splits one emitter into twins at its own code phase on other tiles, so such a peak is held for one dwell and listed only if it was there, at the same tile, on the previous one. Each listed peak is one [**acq\_result\_t**](structacq__result__t.md) from [**acq\_push()**](acq__core_8h.md#function-acq_push), all of a dwell's sharing its `samples_consumed` and `noise_est`. A held twin takes a slot of the `n` for that dwell but is not reported. The threshold does not change: a second peak is another draw from the same cells against the same union bound. Clears the held candidates.




**Parameters:**


* `state` Must be non-NULL. 
* `n` 1 … ACQ\_MAX\_PEAKS. 



**Returns:**

0, or -1 (state untouched) when `n` is out of range. 
```C++
>>> import numpy as np
>>> from doppler.dsss import Acquisition
>>> code = (np.arange(31) * 5 % 2).astype(np.uint8)
>>> a = Acquisition(code, spc=2, chip_rate=1e6, symbol_rate=1e3,
...                 cn0_dbhz=50.0, doppler_uncertainty=50e3)
>>> a.max_peaks
1
>>> a.set_max_peaks(8)
>>> a.max_peaks
8
```
 





        

<hr>



### function acq\_set\_state 

_Restore cross-call state from_ `blob` _into_`state` _(replacing it)._
```C++
int acq_set_state (
    acq_state_t * state,
    const void * blob
) 
```





**Returns:**

0 on success, -1 if the blob's magic/version/n/n\_noncoh disagree with `state` (rebuild the engine from the matching descriptor first). 





        

<hr>



### function acq\_set\_surface\_sink 

_Attach (or detach) a C surface sink: every_ `decim-th` _decided dwell's surface, in test-statistic units, handed to_`fn` _on the pushing thread (design §2.4)._
```C++
void acq_set_surface_sink (
    acq_state_t * state,
    acq_surface_sink_fn fn,
    void * ctx,
    uint32_t decim
) 
```



Sets `keep_surface`, so [**acq\_surface()**](acq__core_8h.md#function-acq_surface) reads the same dwell afterwards. The pointer handed to `fn` is the engine's and is valid only for the call: copy or write it out there. This is how a long run records the surface decimated in time without a copy per dwell it does not keep.




**Parameters:**


* `state` Must be non-NULL. 
* `fn` The sink, or NULL to detach. 
* `ctx` Passed through to `fn`. 
* `decim` Hand over every decim-th dwell; 0 reads as 1. 




        

<hr>



### function acq\_set\_telemetry 

_Attach (or detach) a telemetry context and register the engine's probes on it (design §2.4)._ 
```C++
int acq_set_telemetry (
    acq_state_t * state,
    dp_tlm_t * tlm,
    const char * prefix,
    uint32_t decim
) 
```



Registers ten probes, emitted once per DECIDED dwell (a coherent dump, or the dwell that completes `n_noncoh` looks) and further thinned by `decim:` "&lt;prefix&gt;.stat" (the dwell's test statistic — the strongest cell against the CFAR reference, in the units the gate is set in), "&lt;prefix&gt;.gate" (that gate: `threshold` on the coherent path, `eta_nc` on the non-coherent one — plotted together they show exactly where a hit fired), "&lt;prefix&gt;.noise" (the CFAR reference `noise_est`), "&lt;prefix&gt;.peak" (the strongest cell's raw value), "&lt;prefix&gt;.row" and "&lt;prefix&gt;.col" (its native Doppler row and code-phase column — a surface coordinate, not a physical unit; [**acq\_surface\_doppler\_hz()**](acq__core_8h.md#function-acq_surface_doppler_hz) and [**acq\_surface\_chip\_phase()**](acq__core_8h.md#function-acq_surface_chip_phase) convert), "&lt;prefix&gt;.n\_peaks" (picks in the dwell, held twins included), "&lt;prefix&gt;.n\_held" (picks held as same-code-phase twins rather than listed, §7.1), "&lt;prefix&gt;.conc" (the strongest pick's concentration — see `peak_conc`: its main lobe's power over its whole column's, near 1 for one clean emitter even when it straddles two tiles, about 0.5 when a data transition splits it into twins two or more tiles away, lower still when a coherent block straddles data — the discriminator between one emitter's splatter and a second emitter) and "&lt;prefix&gt;.hit" (1 when the gate fired). Passing NULL detaches. Setup path, never hot; the context is borrowed and must outlive the attachment (SPSC rules in [**dp\_tlm/dp\_tlm\_core.h**](dp__tlm__core_8h.md)).




**Parameters:**


* `state` Must be non-NULL. 
* `tlm` Telemetry context to attach, or NULL to detach. 
* `prefix` Probe-name prefix, e.g. "acq" or "ch0.acq". 
* `decim` Emit every decim-th decided dwell; &gt;= 1. 



**Returns:**

DP\_OK, or DP\_ERR\_INVALID when the probe table cannot take all ten probes (the attach fails whole; the engine stays detached). 
```C++
>>> import numpy as np
>>> from doppler.dsss import Acquisition
>>> from doppler.telemetry import Telemetry
>>> from doppler.wfm import PN, mls_poly
>>> code = np.asarray(
...     PN(poly=mls_poly(9), seed=1, length=9).generate(511), np.uint8)
>>> a = Acquisition(code, spc=2, chip_rate=1e6, cn0_dbhz=50.0)
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



### function acq\_set\_threads 

_Set how many threads the searcher fans its tiles across (design §2.3: a roll per thread on persistent workers)._ 
```C++
int acq_set_threads (
    acq_state_t * state,
    int n
) 
```



A continuous engine is created with a pool of the machine's online cores when it has more than one tile; a burst engine, and a single-tile one, run serially. This sets the count: 0 auto-selects the online core count, 1 runs everything on the calling thread, n runs on n workers (the caller included). The workers are created here, once, and parked between pushes; nothing is created per push. The surface is bit-identical at every count  the tiles are independent after the one forward transform and each writes its own rows  so this changes the cost of a push and nothing about its result. Setup path, never hot; not while another thread is inside push().




**Parameters:**


* `state` Must be non-NULL. 
* `n` Thread count; 0 = online cores, 1 = serial. 



**Returns:**

DP\_OK. The count actually running is `threads`. 
```C++
>>> import numpy as np
>>> from doppler.dsss import Acquisition
>>> from doppler.wfm import PN, mls_poly
>>> code = np.asarray(
...     PN(poly=mls_poly(9), seed=1, length=9).generate(511), np.uint8)
>>> a = Acquisition(code, spc=2, chip_rate=1e6, cn0_dbhz=50.0,
...                 doppler_uncertainty=4000.0)
>>> a.threads >= 1               # a pool, sized to the machine
True
>>> a.set_threads(1)
>>> a.threads
1
```
 





        

<hr>



### function acq\_state\_bytes 

_Byte size of_ `state's` _blob (header + unconsumed + nc)._
```C++
size_t acq_state_bytes (
    const acq_state_t * state
) 
```




<hr>



### function acq\_surface 

_The last decided dwell's surface, in the gate's own units._ 
```C++
size_t acq_surface (
    acq_state_t * state,
    float * out,
    size_t n_out
) 
```



Copies the surface the last dwell was decided on into `out`, row-major `surface_rows` (Doppler: tiles, or interpolated slow-time rows) by `code_bins` (code phase), every cell divided by the same CFAR reference the gate used — so a cell reads as its own test statistic, to a float rounding (the SIMD build's fast-math may take a reciprocal in this loop and a divide in the gate's), and the gate (`threshold`, or `eta_nc` on the non-coherent path) is a flat plane on a plot. The engine keeps this only while `keep_surface` is set (a caller sets it, or [**acq\_set\_surface\_sink()**](acq__core_8h.md#function-acq_set_surface_sink) does): set it, push, then read. `surface_at` says which dwell it is; a time-decimated record is the caller reading every k-th dwell, or a sink with `decim`.




**Parameters:**


* `state` Must be non-NULL. 
* `out` At least `surface_rows * code_bins` floats. 
* `n_out` Capacity of `out`. 



**Returns:**

Cells written (`surface_rows * code_bins`), or 0 when no dwell has been decided with `keep_surface` set, or `out` is too small. 
```C++
>>> import numpy as np
>>> from doppler.dsss import Acquisition
>>> from doppler.wfm import PN, mls_poly
>>> code = np.asarray(
...     PN(poly=mls_poly(9), seed=1, length=9).generate(511), np.uint8)
>>> a = Acquisition(code, spc=2, chip_rate=1e6, cn0_dbhz=50.0)
>>> a.keep_surface = 1
>>> x = np.zeros(a.n_noncoh * 511 * 2, dtype=np.complex64)
>>> _ = a.push(x)
>>> s = np.empty(a.surface_rows * a.code_bins, dtype=np.float32)
>>> a.surface(s) == s.size
True
>>> s.reshape(a.surface_rows, a.code_bins).shape == (a.surface_rows, 1022)
True
```
 





        

<hr>



### function acq\_surface\_chip\_phase 

_The surface's code-phase axis: the chip phase of each column._ 
```C++
size_t acq_surface_chip_phase (
    acq_state_t * state,
    double * out,
    size_t n_out
) 
```



One value per surface column, in chips, the same mapping [**acq\_build\_handoff()**](acq__core_8h.md#function-acq_build_handoff) applies to a hit's `code_phase` — so a plotted peak sits at the chip phase the DetectionEvent would carry.




**Parameters:**


* `state` Must be non-NULL. 
* `out` At least `code_bins` doubles. 
* `n_out` Capacity of `out`. 



**Returns:**

Values written (`code_bins`), or 0 if `out` is too small. 
```C++
>>> import numpy as np
>>> from doppler.dsss import Acquisition
>>> from doppler.wfm import PN, mls_poly
>>> code = np.asarray(
...     PN(poly=mls_poly(9), seed=1, length=9).generate(511), np.uint8)
>>> a = Acquisition(code, spc=2, chip_rate=1e6, cn0_dbhz=50.0)
>>> c = np.empty(a.code_bins, dtype=np.float64)
>>> a.surface_chip_phase(c) == a.code_bins
True
>>> bool(c[0] == 0.0 and c[1] == 510.5)
True
```
 





        

<hr>



### function acq\_surface\_complex 

_The last decided dwell's surface, complex: amplitude and carrier phase per cell, before the magnitude the gate reads._ 
```C++
size_t acq_surface_complex (
    acq_state_t * state,
    float _Complex * out,
    size_t n_out
) 
```



Copies the coherent sum the last dwell was decided on into `out`, row-major `surface_rows` x `code_bins` like [**acq\_surface()**](acq__core_8h.md#function-acq_surface), in the correlation's own units rather than the gate's. A cell's phase is the carrier at the block's middle, relative to its tile's centre; its neighbours along the code axis are complex early and late arms, so a tracker can form the coherent discriminator `Re(conj(P) (L - E)) / |P|^2`, which the magnitude surface cannot (docs/design/async-dsss-receiver-measurements.md §12.21). Coherent path only: a non-coherent dwell (`n_noncoh > 1`) is a power sum with no phase, and reads 0.




**Parameters:**


* `state` Must be non-NULL. 
* `out` At least `surface_rows * code_bins` complex floats. 
* `n_out` Capacity of `out`. 



**Returns:**

Cells written (`surface_rows * code_bins`), or 0 when no dwell has been decided, the path is non-coherent, or `out` is too small. 
```C++
>>> import numpy as np
>>> from doppler.dsss import Acquisition
>>> from doppler.wfm import PN, mls_poly
>>> code = np.asarray(PN(poly=mls_poly(5), seed=1,
...                      length=5).generate(31)).astype(np.uint8)
>>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), 4).astype(
...     np.complex64)
>>> a = Acquisition(code, spc=4, chip_rate=1e6, cn0_dbhz=70.0)
>>> a.n_noncoh                 # one look: the dwell is the coherent dump
1
>>> _ = a.push(np.roll(s0, 17).astype(np.complex64))
>>> sc = np.empty(a.surface_rows * a.code_bins, dtype=np.complex64)
>>> a.surface_complex(sc) == sc.size
True
>>> int(np.argmax(np.abs(sc)) % a.code_bins)   # the peak's code phase
17
```
 





        

<hr>



### function acq\_surface\_doppler\_hz 

_The surface's Doppler axis: the frequency of each row, in Hz._ 
```C++
size_t acq_surface_doppler_hz (
    acq_state_t * state,
    double * out,
    size_t n_out
) 
```



One value per surface row, the fold and scale a hit's `doppler_hz_est` uses (dp\_fftfreq\_index() times `doppler_res_hz`, on the interpolated grid where the slow-time axis is interpolated), so a plot of [**acq\_surface()**](acq__core_8h.md#function-acq_surface) carries the same axis a DetectionEvent reports on.




**Parameters:**


* `state` Must be non-NULL. 
* `out` At least `surface_rows` doubles. 
* `n_out` Capacity of `out`. 



**Returns:**

Values written (`surface_rows`), or 0 if `out` is too small. 
```C++
>>> import numpy as np
>>> from doppler.dsss import Acquisition
>>> from doppler.wfm import PN, mls_poly
>>> code = np.asarray(
...     PN(poly=mls_poly(9), seed=1, length=9).generate(511), np.uint8)
>>> a = Acquisition(code, spc=2, chip_rate=1e6, cn0_dbhz=50.0,
...                 doppler_uncertainty=4000.0)
>>> f = np.empty(a.surface_rows, dtype=np.float64)
>>> a.surface_doppler_hz(f) == a.surface_rows
True
>>> bool(f[0] == 0.0 and f.min() < 0.0 < f.max())
True
```
 





        

<hr>
## Macro Definition Documentation





### define ACQ\_COL\_CHUNK 

```C++
#define ACQ_COL_CHUNK `32u`
```



Columns the block-end transform gathers per pass (design §2.3, #1243): a cache line holds 8 cf32 cells, so a chunk of 32 columns reads four lines per slow-time row of the block and writes four per surface row, where a column at a time read and wrote one line per CELL. 


        

<hr>



### define ACQ\_MAX\_PEAKS 

```C++
#define ACQ_MAX_PEAKS `64u`
```



The largest `max_peaks` [**acq\_set\_max\_peaks()**](acq__core_8h.md#function-acq_set_max_peaks) accepts: one push's result array is sized to this many in the binding, so one dwell can always be reported whole. 


        

<hr>



### define ACQ\_N\_NONCOH\_SAFETY\_CEILING 

_Internal safety-valve ceiling on auto-selected non-coherent looks_  _not a public knob (no caller-facing equivalent of the retired_`max_noncoh` _parameter)._
```C++
#define ACQ_N_NONCOH_SAFETY_CEILING `256u`
```



The semi-analytical Pd model both auto-sizers ascend against turns non-monotonic and unreliable past a few hundred looks (this project's own geometry found ~256 empirically  see docs/design/async-dsss-receiver.md). Hitting this ceiling without meeting `pd` leaves [**acq\_state\_t::underpowered**](structacq__state__t.md#variable-underpowered) set, same as any other infeasible operating point  no separate bookkeeping needed. 


        

<hr>



### define ACQ\_STATE\_MAGIC 

```C++
#define ACQ_STATE_MAGIC `DP_FOURCC ('A', 'C', 'Q', 'R')`
```




<hr>



### define ACQ\_STATE\_VERSION 

```C++
#define ACQ_STATE_VERSION `4u /* v4: the block's raw epochs ride beside it */`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/acq/acq_core.h`

