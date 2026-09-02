

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















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**acq\_extra\_t**](structacq__extra__t.md) <br>_Per-object extra header for an engine's cross-call state._  |
| struct | [**acq\_handoff\_t**](structacq__handoff__t.md) <br>_Wire-ready hand-off record built from one_ [_**acq\_result\_t**_](structacq__result__t.md) _hit._ |
| struct | [**acq\_result\_t**](structacq__result__t.md) <br>_One acquisition detection event._  |
| struct | [**acq\_state\_t**](structacq__state__t.md) <br>_Streaming acquisition-engine state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  void | [**acq\_build\_handoff**](#function-acq_build_handoff) (const [**acq\_state\_t**](structacq__state__t.md) \* state, const [**acq\_result\_t**](structacq__result__t.md) \* hit, size\_t code\_len, size\_t spc, [**acq\_handoff\_t**](structacq__handoff__t.md) \* out) <br>_Convert one_ [_**acq\_push()**_](acq__core_8h.md#function-acq_push) _hit into a wire-ready hand-off record._ |
|  int | [**acq\_configure\_search\_raw**](#function-acq_configure_search_raw) ([**acq\_state\_t**](structacq__state__t.md) \* state, size\_t doppler\_bins, size\_t n\_noncoh) <br>_Pin the search grid directly, bypassing both auto-sizing searches — the advanced escape hatch (mirrors Dll's/Costas's configure\_lock\_raw())._  |
|  [**acq\_state\_t**](structacq__state__t.md) \* | [**acq\_create\_burst**](#function-acq_create_burst) (const uint8\_t \* code, size\_t code\_len, size\_t reps, size\_t spc, double chip\_rate, double cn0\_dbhz, double doppler\_uncertainty, double pfa, double pd, int noise\_mode) <br>_Create a burst-mode acquisition engine: coherent multi-epoch combining, up to_ `reps` _deep (today's classic behavior)._ |
|  [**acq\_state\_t**](structacq__state__t.md) \* | [**acq\_create\_continuous**](#function-acq_create_continuous) (const uint8\_t \* code, size\_t code\_len, size\_t spc, double chip\_rate, double symbol\_rate, double cn0\_dbhz, double doppler\_uncertainty, double pfa, double pd, int noise\_mode) <br>_Create a continuous-mode acquisition engine: always wideband window-tiling, never coherent multi-epoch combining._  |
|  void | [**acq\_destroy**](#function-acq_destroy) ([**acq\_state\_t**](structacq__state__t.md) \* state) <br>_Destroy and free an engine._  |
|  void | [**acq\_get\_state**](#function-acq_get_state) (const [**acq\_state\_t**](structacq__state__t.md) \* state, void \* blob) <br>_Serialize_ `state's` _cross-call state into_`blob` _(caller-owned,_[_**acq\_state\_bytes()**_](acq__core_8h.md#function-acq_state_bytes) _long). Call between pushes (no partial dump pending)._ |
|  size\_t | [**acq\_push**](#function-acq_push) ([**acq\_state\_t**](structacq__state__t.md) \* state, const float \_Complex \* x, size\_t n\_in, [**acq\_result\_t**](structacq__result__t.md) \* result, size\_t max\_results) <br>_Stream raw samples; emit one event per CFAR dump above threshold._  |
|  void | [**acq\_reset**](#function-acq_reset) ([**acq\_state\_t**](structacq__state__t.md) \* state) <br>_Drain the input ring and reset the coherent accumulator._  |
|  size\_t | [**acq\_run**](#function-acq_run) ([**acq\_state\_t**](structacq__state__t.md) \* state, const void \* state\_in, void \* state\_out, const float complex \* in, size\_t n\_in, [**acq\_result\_t**](structacq__result__t.md) \* result, size\_t max\_results) <br>_Pure run: inject_ `state_in` _, stream_`in` _, emit hits, export_`state_out` _—_`(state_in, input) -> (state_out, output)` _over an engine treated as immutable config + scratch._`state_in` _/_`state_out` _may alias. Either may be NULL (NULL in = fresh; NULL out = discard)._ |
|  int | [**acq\_set\_max\_peaks**](#function-acq_set_max_peaks) ([**acq\_state\_t**](structacq__state__t.md) \* state, size\_t n) <br>_How many peaks a dwell may report: the peak list's capacity._  |
|  int | [**acq\_set\_state**](#function-acq_set_state) ([**acq\_state\_t**](structacq__state__t.md) \* state, const void \* blob) <br>_Restore cross-call state from_ `blob` _into_`state` _(replacing it)._ |
|  size\_t | [**acq\_state\_bytes**](#function-acq_state_bytes) (const [**acq\_state\_t**](structacq__state__t.md) \* state) <br>_Byte size of_ `state's` _blob (header + unconsumed + nc)._ |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**ACQ\_MAX\_PEAKS**](acq__core_8h.md#define-acq_max_peaks)  `64u`<br> |
| define  | [**ACQ\_N\_NONCOH\_SAFETY\_CEILING**](acq__core_8h.md#define-acq_n_noncoh_safety_ceiling)  `256u`<br>_Internal safety-valve ceiling on auto-selected non-coherent looks_  _not a public knob (no caller-facing equivalent of the retired_`max_noncoh` _parameter)._ |
| define  | [**ACQ\_STATE\_MAGIC**](acq__core_8h.md#define-acq_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('A', 'C', 'Q', 'R')`<br> |
| define  | [**ACQ\_STATE\_VERSION**](acq__core_8h.md#define-acq_state_version)  `2u /\* v2: the peak list's held twins ride along \*/`<br> |

## Detailed Description


Acquires a direct-sequence spread-spectrum signal — repeated, BPSK -modulated PN-code segments — arriving with an unknown integer code phase and an unknown carrier-frequency (Doppler) offset, buried in AWGN. It jointly estimates the (Doppler bin, code phase) and declares a detection whenever the CFAR test statistic crosses an automatically configured threshold.


Pipeline (owned end to end, one object): push(raw cf32) -&gt; ring buffer -&gt; reframe to (coherent\_bins, code\_bins) -&gt; slow-time Doppler FFT (FFT along the segment axis) -&gt; 2-D code correlation against a single-row PN reference (corr2d) -&gt; argmax + CFAR noise estimate -&gt; threshold gate -&gt; [**acq\_result\_t**](structacq__result__t.md).


The fast-time axis (code\_bins = sf\*spc columns) is the circular code matched filter; the slow-time axis (coherent\_bins rows, one row per code repetition) is the coherent Doppler search. A carrier offset f (cycles/sample) lands the peak at row = round(f\*code\_bins\*coherent\_bins) mod coherent\_bins, column = code phase.


**Two mode-fixed public constructors, one shared engine.** A coherent slow-time Doppler FFT can only ever resolve frequency _within_ one native span `chip_rate/(2*sf)` — more coherent depth subdivides that SAME fixed range more finely, it never widens it — and for a continuous (async, data-modulated) signal, a multi-epoch coherent axis wide enough to matter aliases the data's own bit transitions across the whole Doppler-bin axis (a structural mislock, not a graceful SNR loss — see docs/design/dsss-acquisition.md). So the two constructors fix a mode each, never a per-call knob:



* [**acq\_create\_burst()**](acq__core_8h.md#function-acq_create_burst) — today's classic behavior: the smallest coherent depth `coherent_bins` in `[1, reps]` whose coherent\_bins\*code\_bins coherent samples meet `pd` (det\_threshold / det\_pd) — minimum latency for a strong signal, unmodulated bursts/preambles only. A tighter `doppler_uncertainty` shrinks the searched cell count, lowering the Bonferroni threshold (more sensitive). When `doppler_uncertainty` exceeds the native span, falls back to the wideband window-tiling mechanism below instead (coherent depth structurally can't cover more than one span, regardless of mode).
* [**acq\_create\_continuous()**](acq__core_8h.md#function-acq_create_continuous) — for a continuous, data-modulated signal: ALWAYS uses the wideband window-tiling mechanism below, unconditionally (never attempts coherent multi-epoch combining, even when `doppler_uncertainty` is narrower than one native span) — closes the aliasing footgun structurally rather than pricing it as a tunable loss. Sensitivity margin comes entirely from auto-selected non-coherent looks.




Both convert C/N0 to a per-sample amplitude SNR (snr = sqrt(10^(cn0\_dbhz/10) / (chip\_rate\*spc))). Every reported detection inverts this same relationship to report an estimated C/N0 ([**acq\_result\_t::cn0\_dbhz\_est**](structacq__result__t.md#variable-cn0_dbhz_est)) — a bandwidth/integration-time-independent figure of merit directly comparable to `cn0_dbhz`, unlike a raw per-sample or coherently-integrated ratio (both scale with `spc/ reps and` so aren't portable across configurations).


**Wideband window-tiling mode**: instead of coherent combining, tiles the requested uncertainty with `window_bins = ceil(doppler_uncertainty / (chip_rate/(2*sf)))` parallel frequency-window hypotheses, each one native span wide, searched every epoch from a SINGLE shared forward FFT of that epoch: hypothesis r's spectrum is the shared FFT circularly rolled by r bins (exact — the window spacing IS this code\_bins-point FFT's own bin spacing) against one fixed precomputed replica spectrum, then inverse-FFT'd — `window_bins` inverse FFTs plus the one shared forward FFT per epoch, not `window_bins` independent down-conversions. Empirically the cheaper of the two realizations benchmarked for this (a frequency-bank benchmark): ~1.2x-1.55x faster than an equivalent tuned-mixer bank, measured with real doppler.spectral.FFT. SNR margin in this mode comes entirely from auto-selected non-coherent looks (magnitude-squared accumulation, immune to data-modulation sign flips) rather than coherent depth, sized against an internal safety-valve ceiling rather than a caller-supplied cap (the semi-analytical Pd model this engine sizes against grows unreliable past a few hundred looks — not a public knob to tune around that). `doppler_bin` in [**acq\_result\_t**](structacq__result__t.md) reports the frequency-window index (0 … window\_bins-1, native FFT-bin ordering) instead of a slow-time-FFT row when this mode is active; `doppler_res_hz` still reports the per-window spacing (chip\_rate/sf, unchanged formula at coherent\_bins=1); combining wideband search WITH a coherent depth &gt; 1 per window is not supported (a possible future extension, not needed by any current use case).



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
 


    
## Public Functions Documentation




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

_Create a continuous-mode acquisition engine: always wideband window-tiling, never coherent multi-epoch combining._ 
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
    int noise_mode
) 
```



Builds the single-row oversampled BPSK reference from `code`, infers sf = `code_len`, converts `cn0_dbhz` to a per-sample amplitude SNR, and ALWAYS tiles `window_bins = max(1, ceil(doppler_uncertainty / (chip_rate/(2*sf))))` parallel frequency-window hypotheses (see the file doc comment's "Wideband window-tiling mode")  unconditionally, even when `doppler_uncertainty` is narrower than one native span. `coherent_bins` is pinned to 1 always: a continuous, data-modulated signal's own bit transitions make coherent multi-epoch combining a structural aliasing mislock, not a graceful SNR loss (see docs/design/dsss-acquisition.md), so this engine never attempts it. Sensitivity margin comes entirely from auto-selected non-coherent looks (up to the internal [**ACQ\_N\_NONCOH\_SAFETY\_CEILING**](acq__core_8h.md#define-acq_n_noncoh_safety_ceiling)).




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



### function acq\_state\_bytes 

_Byte size of_ `state's` _blob (header + unconsumed + nc)._
```C++
size_t acq_state_bytes (
    const acq_state_t * state
) 
```




<hr>
## Macro Definition Documentation





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
#define ACQ_STATE_VERSION `2u /* v2: the peak list's held twins ride along */`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/acq/acq_core.h`

