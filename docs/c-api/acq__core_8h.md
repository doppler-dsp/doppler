

# File acq\_core.h



[**FileList**](files.md) **>** [**acq**](dir_25a1e6db36731e5901b5cfb158eaa462.md) **>** [**acq\_core.h**](acq__core_8h.md)

[Go to the source code of this file](acq__core_8h_source.md)

_Streaming DSSS burst-acquisition engine._ [More...](#detailed-description)

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
| struct | [**acq\_result\_t**](structacq__result__t.md) <br>_One acquisition detection event._  |
| struct | [**acq\_state\_t**](structacq__state__t.md) <br>_Streaming acquisition-engine state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  int | [**acq\_configure\_search\_raw**](#function-acq_configure_search_raw) ([**acq\_state\_t**](structacq__state__t.md) \* state, size\_t doppler\_bins, size\_t n\_noncoh) <br>_Pin the search grid directly, bypassing both auto-sizing searches — the advanced escape hatch (mirrors Dll's/Costas's configure\_lock\_raw())._  |
|  [**acq\_state\_t**](structacq__state__t.md) \* | [**acq\_create**](#function-acq_create) (const uint8\_t \* code, size\_t code\_len, size\_t reps, size\_t spc, double chip\_rate, double cn0\_dbhz, double doppler\_uncertainty, double pfa, double pd, int noise\_mode, size\_t max\_noncoh, double symbol\_rate, double doppler\_resolution, double doppler\_rate) <br>_Create a streaming DSSS acquisition engine._  |
|  void | [**acq\_destroy**](#function-acq_destroy) ([**acq\_state\_t**](structacq__state__t.md) \* state) <br>_Destroy and free an engine._  |
|  void | [**acq\_get\_state**](#function-acq_get_state) (const [**acq\_state\_t**](structacq__state__t.md) \* state, void \* blob) <br>_Serialize_ `state's` _cross-call state into_`blob` _(caller-owned,_[_**acq\_state\_bytes()**_](acq__core_8h.md#function-acq_state_bytes) _long). Call between pushes (no partial dump pending)._ |
|  size\_t | [**acq\_push**](#function-acq_push) ([**acq\_state\_t**](structacq__state__t.md) \* state, const float complex \* in, size\_t n\_in, [**acq\_result\_t**](structacq__result__t.md) \* result, size\_t max\_results) <br>_Stream raw samples; emit one event per CFAR dump above threshold._  |
|  void | [**acq\_reset**](#function-acq_reset) ([**acq\_state\_t**](structacq__state__t.md) \* state) <br>_Drain the input ring and reset the coherent accumulator._  |
|  size\_t | [**acq\_run**](#function-acq_run) ([**acq\_state\_t**](structacq__state__t.md) \* state, const void \* state\_in, void \* state\_out, const float complex \* in, size\_t n\_in, [**acq\_result\_t**](structacq__result__t.md) \* result, size\_t max\_results) <br>_Pure run: inject_ `state_in` _, stream_`in` _, emit hits, export_`state_out` _—_`(state_in, input) -> (state_out, output)` _over an engine treated as immutable config + scratch._`state_in` _/_`state_out` _may alias. Either may be NULL (NULL in = fresh; NULL out = discard)._ |
|  int | [**acq\_set\_state**](#function-acq_set_state) ([**acq\_state\_t**](structacq__state__t.md) \* state, const void \* blob) <br>_Restore cross-call state from_ `blob` _into_`state` _(replacing it)._ |
|  size\_t | [**acq\_state\_bytes**](#function-acq_state_bytes) (const [**acq\_state\_t**](structacq__state__t.md) \* state) <br>_Byte size of_ `state's` _blob (header + unconsumed + nc)._ |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**ACQ\_STATE\_MAGIC**](acq__core_8h.md#define-acq_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('A', 'C', 'Q', 'R')`<br> |
| define  | [**ACQ\_STATE\_VERSION**](acq__core_8h.md#define-acq_state_version)  `1u`<br> |

## Detailed Description


Acquires a direct-sequence spread-spectrum burst — a run of repeated, BPSK-modulated PN-code segments — arriving with an unknown integer code phase and an unknown carrier-frequency (Doppler) offset, buried in AWGN. It jointly estimates the (Doppler bin, code phase) of the burst and declares a detection whenever the CFAR test statistic crosses an automatically configured threshold.


Pipeline (owned end to end, one object): push(raw cf32) -&gt; ring buffer -&gt; reframe to (doppler\_bins, code\_bins) -&gt; slow-time Doppler FFT (FFT along the segment axis) -&gt; 2-D code correlation against a single-row PN reference (corr2d) -&gt; argmax + CFAR noise estimate -&gt; threshold gate -&gt; [**acq\_result\_t**](structacq__result__t.md).


The fast-time axis (code\_bins = sf\*spc columns) is the circular code matched filter; the slow-time axis (doppler\_bins rows, one row per code repetition) is the Doppler search. A carrier offset f (cycles/sample) lands the peak at row = round(f\*code\_bins\*doppler\_bins) mod doppler\_bins, column = code phase.


Physics-only construction: the user gives the PN `code`, the front-end geometry (`reps`, `spc`, `chip_rate`), the sensitivity (`cn0_dbhz`), and the detection targets (`pfa`, `pd`, optional `doppler_uncertainty`). The engine converts C/N0 to a per-sample amplitude SNR (snr = sqrt(10^(cn0\_dbhz/10) / (chip\_rate\*spc))) and picks the _smallest_ coherent depth doppler\_bins in `[1, reps]` whose doppler\_bins\*code\_bins coherent samples meet `pd` (det\_threshold / det\_pd) — minimum latency for a strong signal. A tighter `doppler_uncertainty` shrinks the searched cell count, lowering the Bonferroni threshold (more sensitive). Every reported detection inverts this same relationship to report an estimated C/N0 ([**acq\_result\_t::cn0\_dbhz\_est**](structacq__result__t.md#variable-cn0_dbhz_est)) — a bandwidth/integration-time-independent figure of merit directly comparable to `cn0_dbhz`, unlike a raw per-sample or coherently-integrated ratio (both scale with `spc/ reps and` so aren't portable across configurations).



```C++
// 31-chip PN, 4x oversample, up to 16 coherent reps; 1 MHz chips, 45 dB-Hz
uint8_t code[31] = { 0 };   // ... fill with PN chips (0/1) ...
acq_state_t *a = acq_create(code, 31, 16, 4, 1.0e6, 45.0,
                            0.0, 1e-3, 0.9, 0, 1, 0.0, 0.0);
acq_result_t hits[64];
size_t nh = acq_push(a, samples, n_samples, hits, 64);
for (size_t i = 0; i < nh; i++)
  printf("Doppler %zu, code phase %zu, C/N0 %.1f dB-Hz\n",
         hits[i].doppler_bin, hits[i].code_phase,
         hits[i].cn0_dbhz_est);
acq_destroy(a);
```
 


    
## Public Functions Documentation




### function acq\_configure\_search\_raw 

_Pin the search grid directly, bypassing both auto-sizing searches — the advanced escape hatch (mirrors Dll's/Costas's configure\_lock\_raw())._ 
```C++
int acq_configure_search_raw (
    acq_state_t * state,
    size_t doppler_bins,
    size_t n_noncoh
) 
```



Resizes every buffer/plan that depends on the grid (the slow-time FFT, the code correlator, the reference, and every per-frame scratch buffer), re-derives the threshold ladder for the pinned grid from the same physics [**acq\_create()**](acq__core_8h.md#function-acq_create) used, and clears in-flight accumulation (ring contents, the non-coherent power accumulator, dwell bookkeeping) — call between push() calls, never a substitute for one.




**Parameters:**


* `state` Allocated engine (non-NULL). 
* `doppler_bins` Coherent depth to pin, in `[1, reps]`. 
* `n_noncoh` Non-coherent look count to pin, in `[1, max_noncoh]`. 



**Returns:**

0 on success, -1 if either argument is out of range or an allocation fails (the engine is left usable at its prior grid on failure). 





        

<hr>



### function acq\_create 

_Create a streaming DSSS acquisition engine._ 
```C++
acq_state_t * acq_create (
    const uint8_t * code,
    size_t code_len,
    size_t reps,
    size_t spc,
    double chip_rate,
    double cn0_dbhz,
    double doppler_uncertainty,
    double pfa,
    double pd,
    int noise_mode,
    size_t max_noncoh,
    double symbol_rate,
    double doppler_resolution,
    double doppler_rate
) 
```



Builds the single-row oversampled BPSK reference from `code`, infers sf = `code_len`, converts `cn0_dbhz` to a per-sample amplitude SNR (snr = sqrt(10^(cn0\_dbhz/10) / (chip\_rate\*spc))), and auto-configures the search grid.


With `symbol_rate` &lt;= 0 (default; no known continuous data-modulation clock): picks the _smallest_ coherent depth doppler\_bins in `[1, reps]` whose doppler\_bins\*code\_bins coherent samples meet `pd` at the Bonferroni threshold, plus non-coherent looks (up to `max_noncoh`) if the coherent depth alone falls short.


With `symbol_rate` &gt; 0: a continuous data-modulated signal makes a data-bit transition landing mid-coherent-epoch split the coherent sum into two oppositely-signed partial segments, a self-cancellation loss the Doppler/code-phase-only model above doesn't know about and can silently under-size for (see docs/gallery/dsss-acq-async-data.md). The engine instead jointly searches doppler\_bins in `[1, reps]` x non-coherent looks in `[1, max_noncoh]`, pricing that loss honestly (semi-analytical: quadrature over the window's phase relative to the symbol clock, crossed with exact enumeration over the data signs the window touches), and picks the grid meeting `pd` with the fewest total epochs, breaking ties toward a smaller coherent depth (which also lowers mislock risk)  unless `doppler_resolution` floors the search (below).


A tighter `doppler_uncertainty` narrows the scanned Doppler band, lowering the per-cell threshold (more sensitive), on both paths. Use [**acq\_configure\_search\_raw()**](acq__core_8h.md#function-acq_configure_search_raw) to pin the grid directly instead of relying on either search.


`doppler_resolution` &gt; 0 (only meaningful with `symbol_rate` &gt; 0) floors the coherent depth at `ceil(chip_rate / (sf * doppler_resolution))` (clipped to `[1, reps]`) before the joint search runs, and the search then takes the _first_ `(doppler_bins, n_noncoh)` starting from that floor that meets `pd`  trading the fewest-total-epochs guarantee for a guaranteed minimum resolution, and, critically, for search cost: the unfloored joint search is a full `reps x max_noncoh` sweep of the `O(doppler_bins^2)` data-modulation model (`_data_mod_pd`), which the function's own inner comment already flags as assuming a coherent depth "physically small (tens at most)"  fine at the default `reps`, but cubic in `reps` once a caller raises it to reach a fine `doppler_resolution`. Anchoring the sweep at the resolution floor instead of 1 turns that into a handful of evaluations near the floor (first success wins), independent of how large `reps` is.


`doppler_rate` &gt; 0 (only meaningful with `symbol_rate` &gt; 0) caps the coherent depth from the other direction: over a `doppler_bins`-epoch coherent window, a nonzero Doppler rate of change shifts the true frequency across the window, smearing the FFT peak once that drift approaches a resolution bin. The largest depth that keeps in-window drift under one bin is `doppler_bins < chip_rate / (sf * sqrt (doppler_rate))`; the joint search (both its floored and unfloored modes) clips its coherent-depth ceiling to this in addition to `reps`, so a caller-raised `doppler_resolution` can never push `doppler_bins` past the point where the signal's own dynamics would invalidate the coherent sum.




**Parameters:**


* `code` PN chips (0/1), length `code_len`. 
* `code_len` Number of chips supplied (= sf, the spreading factor). 
* `reps` Max coherent code repetitions, the coherence ceiling (&gt;=1). 
* `spc` Samples per chip (&gt;= 1). 
* `chip_rate` Chip rate in Hz (&gt; 0). 
* `cn0_dbhz` Carrier-to-noise density in dB-Hz (&gt; 0). 
* `doppler_uncertainty` One-sided Doppler search half-range in Hz; 0 uses the full native span +/- chip\_rate/(2\*sf). Must be &lt;= span. 
* `pfa` Target system (max-of-N) false-alarm probability (0,1). 
* `pd` Target detection probability (0,1). 
* `noise_mode` CFAR mode index: 0=mean, 1=median, 2=min, 3=max. 
* `max_noncoh` Cap on the auto-split non-coherent look count (&gt;= 1; default 1 keeps the engine purely coherent). 
* `symbol_rate` Continuous data-symbol rate in Hz; &lt;= 0 (default) disables the data-modulation-aware search above. 
* `doppler_resolution` Desired Doppler-bin resolution in Hz; 0 (default) places no floor on the coherent depth  see above. 
* `doppler_rate` Expected Doppler rate of change in Hz/s; 0 (default) assumes a static Doppler  see above. 



**Returns:**

Heap-allocated state, or NULL on bad arguments / allocation failure. 





        

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
    const float complex * in,
    size_t n_in,
    acq_result_t * result,
    size_t max_results
) 
```



Buffers `in`, then for every complete frame applies the slow-time Doppler FFT, correlates against the PN reference, dumps the coherent surface (or, when n\_noncoh &gt; 1, accumulates \|·\|² over n\_noncoh looks first), gates the peak on the auto-configured threshold, and appends an [**acq\_result\_t**](structacq__result__t.md).




**Parameters:**


* `state` Allocated engine (non-NULL). 
* `in` Raw input, interleaved CF32, `n_in` complex samples. 
* `n_in` Number of complex input samples. 
* `result` Output array for detection events. 
* `max_results` Capacity of `result`. 



**Returns:**

Number of events written (0 … max\_results). 





        

<hr>



### function acq\_reset 

_Drain the input ring and reset the coherent accumulator._ 
```C++
void acq_reset (
    acq_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 




        

<hr>



### function acq\_run 

_Pure run: inject_ `state_in` _, stream_`in` _, emit hits, export_`state_out` _—_`(state_in, input) -> (state_out, output)` _over an engine treated as immutable config + scratch._`state_in` _/_`state_out` _may alias. Either may be NULL (NULL in = fresh; NULL out = discard)._
```C++
size_t acq_run (
    acq_state_t * state,
    const void * state_in,
    void * state_out,
    const float complex * in,
    size_t n_in,
    acq_result_t * result,
    size_t max_results
) 
```





**Returns:**

Number of events written (0 … max\_results). 





        

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





### define ACQ\_STATE\_MAGIC 

```C++
#define ACQ_STATE_MAGIC `DP_FOURCC ('A', 'C', 'Q', 'R')`
```




<hr>



### define ACQ\_STATE\_VERSION 

```C++
#define ACQ_STATE_VERSION `1u`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/acq/acq_core.h`

