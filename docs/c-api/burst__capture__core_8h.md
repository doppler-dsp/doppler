

# File burst\_capture\_core.h



[**FileList**](files.md) **>** [**burst\_capture**](dir_8eab18aa96a66319f16718502165a0b6.md) **>** [**burst\_capture\_core.h**](burst__capture__core_8h.md)

[Go to the source code of this file](burst__capture__core_8h_source.md)

_BurstCapture — acquisition's output turned into aligned bursts._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "jm_perf.h"`
* `#include "buffer/buffer.h"`
* `#include "dp_state.h"`
* `#include "burst_acq/burst_acq_core.h"`
* `#include "acq/acq_core.h"`
* `#include "corr2d/corr2d_core.h"`
* `#include "fft2d/fft2d_core.h"`
* `#include "fft/fft_core.h"`
* `#include "detection/detection_core.h"`
* `#include "pn/pn_core.h"`
* `#include "cvt/cvt_core.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**burst\_capture\_detection\_t**](structburst__capture__detection__t.md) <br>_One raw detection, as the search reported it._  |
| struct | [**burst\_capture\_event\_t**](structburst__capture__event__t.md) <br>_One captured burst's event, as_ `events()` _hands it back._ |
| struct | [**burst\_capture\_pending\_t**](structburst__capture__pending__t.md) <br>_One detection between acquisition and emission._  |
| struct | [**burst\_capture\_state\_t**](structburst__capture__state__t.md) <br>_BurstCapture state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  int | [**burst\_capture\_configure\_search\_raw**](#function-burst_capture_configure_search_raw) ([**burst\_capture\_state\_t**](structburst__capture__state__t.md) \* state, size\_t doppler\_bins, size\_t n\_noncoh) <br>_Pin the embedded acquisition's search grid directly._  |
|  [**burst\_capture\_state\_t**](structburst__capture__state__t.md) \* | [**burst\_capture\_create**](#function-burst_capture_create) (const float \_Complex \* preamble, size\_t preamble\_len, size\_t burst\_len, size\_t reps, double fs, double cn0\_dbhz, double doppler\_uncertainty, double pfa, double pd, int noise\_mode, double doppler\_rate) <br>_Create a burst capture: acquisition, refine and retention behind one push()._  |
|  [**burst\_capture\_state\_t**](structburst__capture__state__t.md) \* | [**burst\_capture\_create\_backed**](#function-burst_capture_create_backed) (const char \* path, const float \_Complex \* preamble, size\_t preamble\_len, size\_t burst\_len, size\_t reps, double fs, double cn0\_dbhz, double doppler\_uncertainty, double pfa, double pd, int noise\_mode, double doppler\_rate) <br>_Create a capture whose look-back lives in a FILE._  |
|  void | [**burst\_capture\_destroy**](#function-burst_capture_destroy) ([**burst\_capture\_state\_t**](structburst__capture__state__t.md) \* state) <br>_Release a capture and everything it owns. NULL-safe._  |
|  size\_t | [**burst\_capture\_detections**](#function-burst_capture_detections) ([**burst\_capture\_state\_t**](structburst__capture__state__t.md) \* state, size\_t n, [**burst\_capture\_detection\_t**](structburst__capture__detection__t.md) \* out, size\_t max\_out) <br>_Every hit the search made in the last push(), unfiltered._  |
|  size\_t | [**burst\_capture\_detections\_max\_out**](#function-burst_capture_detections_max_out) ([**burst\_capture\_state\_t**](structburst__capture__state__t.md) \* state, size\_t n) <br>_Raw detections available from the last push()._ `n` _is ignored._ |
|  const [**burst\_capture\_event\_t**](structburst__capture__event__t.md) \* | [**burst\_capture\_event\_at**](#function-burst_capture_event_at) (const [**burst\_capture\_state\_t**](structburst__capture__state__t.md) \* state, size\_t i) <br>_Borrow event_ `i` _of the last push(), or NULL if out of range._ |
|  size\_t | [**burst\_capture\_events**](#function-burst_capture_events) ([**burst\_capture\_state\_t**](structburst__capture__state__t.md) \* state, size\_t n, [**burst\_capture\_event\_t**](structburst__capture__event__t.md) \* out, size\_t max\_out) <br>_The event record for each burst the last push() returned._  |
|  size\_t | [**burst\_capture\_events\_max\_out**](#function-burst_capture_events_max_out) ([**burst\_capture\_state\_t**](structburst__capture__state__t.md) \* state, size\_t n) <br>_Records available from the last push()._ `n` _is ignored._ |
|  double | [**burst\_capture\_get\_cn0\_dbhz\_est**](#function-burst_capture_get_cn0_dbhz_est) (const [**burst\_capture\_state\_t**](structburst__capture__state__t.md) \* state) <br> |
|  size\_t | [**burst\_capture\_get\_code\_bins**](#function-burst_capture_get_code_bins) (const [**burst\_capture\_state\_t**](structburst__capture__state__t.md) \* state) <br>_Code-phase hypotheses per Doppler row._  |
|  size\_t | [**burst\_capture\_get\_doppler\_bins**](#function-burst_capture_get_doppler_bins) (const [**burst\_capture\_state\_t**](structburst__capture__state__t.md) \* state) <br>_Doppler hypotheses searched: the coherent depth, or the window-tile count past the native span_  _acq\_grid\_bins() (doppler#1512)._ |
|  double | [**burst\_capture\_get\_doppler\_hz\_est**](#function-burst_capture_get_doppler_hz_est) (const [**burst\_capture\_state\_t**](structburst__capture__state__t.md) \* state) <br> |
|  double | [**burst\_capture\_get\_doppler\_rate**](#function-burst_capture_get_doppler_rate) (const [**burst\_capture\_state\_t**](structburst__capture__state__t.md) \* state) <br>_Doppler rate (Hz/s) the coherent depth is bounded against; 0 is no bound._  |
|  double | [**burst\_capture\_get\_doppler\_res\_hz**](#function-burst_capture_get_doppler_res_hz) (const [**burst\_capture\_state\_t**](structburst__capture__state__t.md) \* state) <br> |
|  double | [**burst\_capture\_get\_doppler\_span\_hz**](#function-burst_capture_get_doppler_span_hz) (const [**burst\_capture\_state\_t**](structburst__capture__state__t.md) \* state) <br>_Unambiguous Doppler half-range, Hz (+/- this)._  |
|  uint64\_t | [**burst\_capture\_get\_dropped**](#function-burst_capture_get_dropped) (const [**burst\_capture\_state\_t**](structburst__capture__state__t.md) \* state) <br> |
|  double | [**burst\_capture\_get\_eta**](#function-burst_capture_get_eta) (const [**burst\_capture\_state\_t**](structburst__capture__state__t.md) \* state) <br>_Coherent detection gate; in force when_ `n_noncoh == 1` _._ |
|  double | [**burst\_capture\_get\_eta\_nc**](#function-burst_capture_get_eta_nc) (const [**burst\_capture\_state\_t**](structburst__capture__state__t.md) \* state) <br>_Non-coherent gate; in force only when_ `n_noncoh > 1` _, which a burst search never chooses (it reads 0 unless a pin sets looks)._ |
|  size\_t | [**burst\_capture\_get\_min\_gap**](#function-burst_capture_get_min_gap) (const [**burst\_capture\_state\_t**](structburst__capture__state__t.md) \* state) <br>_Dead air a caller must leave between bursts, edge to edge._  |
|  uint64\_t | [**burst\_capture\_get\_n\_bursts**](#function-burst_capture_get_n_bursts) (const [**burst\_capture\_state\_t**](structburst__capture__state__t.md) \* state) <br> |
|  size\_t | [**burst\_capture\_get\_n\_noncoh**](#function-burst_capture_get_n_noncoh) (const [**burst\_capture\_state\_t**](structburst__capture__state__t.md) \* state) <br>_Non-coherent looks combined per decision._  |
|  double | [**burst\_capture\_get\_pd\_burst**](#function-burst_capture_get_pd_burst) (const [**burst\_capture\_state\_t**](structburst__capture__state__t.md) \* state) <br>_Detection probability of one burst: any dwell its preamble spans, at a uniform alignment (doppler#1498). What_ `underpowered` _reads._ |
|  double | [**burst\_capture\_get\_pd\_predicted**](#function-burst_capture_get_pd_predicted) (const [**burst\_capture\_state\_t**](structburst__capture__state__t.md) \* state) <br>_Detection probability of one dwell wholly inside the preamble._  |
|  size\_t | [**burst\_capture\_get\_pending**](#function-burst_capture_get_pending) (const [**burst\_capture\_state\_t**](structburst__capture__state__t.md) \* state) <br> |
|  uint64\_t | [**burst\_capture\_get\_preamble\_start**](#function-burst_capture_get_preamble_start) (const [**burst\_capture\_state\_t**](structburst__capture__state__t.md) \* state) <br> |
|  double | [**burst\_capture\_get\_psl\_db**](#function-burst_capture_get_psl_db) (const [**burst\_capture\_state\_t**](structburst__capture__state__t.md) \* state) <br>_The preamble's peak sidelobe level, dB:_ [_**acq\_psl\_db()**_](acq__core_8h.md#function-acq_psl_db) _of the engine. A burst clearing the threshold by more than_`-psl_db` _also detects its own sidelobe, outside the peak zone; -INFINITY for a perfect sequence._ |
|  void | [**burst\_capture\_get\_state**](#function-burst_capture_get_state) (const [**burst\_capture\_state\_t**](structburst__capture__state__t.md) \* state, void \* blob) <br>_Serialize into_ `blob` _, which must be state\_bytes() long._ |
|  double | [**burst\_capture\_get\_straddle\_loss**](#function-burst_capture_get_straddle_loss) (const [**burst\_capture\_state\_t**](structburst__capture__state__t.md) \* state) <br>_Correlation kept, worst case, by a burst landing between bins._  |
|  size\_t | [**burst\_capture\_push**](#function-burst_capture_push) ([**burst\_capture\_state\_t**](structburst__capture__state__t.md) \* state, const float \_Complex \* x, size\_t x\_len, float \_Complex \* out, size\_t max\_out) <br>_Stream samples; get back every burst whose window has arrived._  |
|  size\_t | [**burst\_capture\_push\_max\_out**](#function-burst_capture_push_max_out) ([**burst\_capture\_state\_t**](structburst__capture__state__t.md) \* state, size\_t x\_len) <br>_Upper bound on samples push() can return for_ `x_len` _input._ |
|  size\_t | [**burst\_capture\_ready**](#function-burst_capture_ready) (const [**burst\_capture\_state\_t**](structburst__capture__state__t.md) \* state) <br>_Windows the last push() completed._  |
|  int | [**burst\_capture\_release**](#function-burst_capture_release) ([**burst\_capture\_state\_t**](structburst__capture__state__t.md) \* state, size\_t i) <br>_Give back the span that window_ `i` _of the last push() claimed._ |
|  void | [**burst\_capture\_reset**](#function-burst_capture_reset) ([**burst\_capture\_state\_t**](structburst__capture__state__t.md) \* state) <br>_Return to the searching state._  |
|  int | [**burst\_capture\_set\_state**](#function-burst_capture_set_state) ([**burst\_capture\_state\_t**](structburst__capture__state__t.md) \* state, const void \* blob) <br>_Restore from_ `blob` _._ |
|  size\_t | [**burst\_capture\_state\_bytes**](#function-burst_capture_state_bytes) (const [**burst\_capture\_state\_t**](structburst__capture__state__t.md) \* state) <br>_Bytes one blob occupies: a pure function of CONFIGURATION._  |
|  const float \_Complex \* | [**burst\_capture\_window**](#function-burst_capture_window) (const [**burst\_capture\_state\_t**](structburst__capture__state__t.md) \* state, size\_t i) <br>_Borrow window_ `i` _of the last push(), or NULL if out of range._ |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**BURST\_CAPTURE\_EDGE\_TWINS**](burst__capture__core_8h.md#define-burst_capture_edge_twins)  `4u`<br> |
| define  | [**BURST\_CAPTURE\_HITS**](burst__capture__core_8h.md#define-burst_capture_hits)  `16u`<br>_Detections collected from acquisition per batch._  |
| define  | [**BURST\_CAPTURE\_MAX\_PHASES**](burst__capture__core_8h.md#define-burst_capture_max_phases)  `4u`<br> |
| define  | [**BURST\_CAPTURE\_REFINE\_INTERP**](burst__capture__core_8h.md#define-burst_capture_refine_interp)  `4u`<br> |
| define  | [**BURST\_CAPTURE\_STATE\_MAGIC**](burst__capture__core_8h.md#define-burst_capture_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('B', 'C', 'A', 'P')`<br>_State blob magic — a wrong blob is rejected, not reinterpreted._  |
| define  | [**BURST\_CAPTURE\_STATE\_VERSION**](burst__capture__core_8h.md#define-burst_capture_state_version)  `4u`<br>_State blob layout version._  |

## Detailed Description


Between a detector and whatever consumes a burst there is a stage nobody owned: acquisition reports an END anchor and a code phase that is a lag MODULO one code period, so it fixes the alignment WITHIN a repetition and never says WHICH one. A burst has a frame that begins in one specific repetition, so somebody has to resolve the period and reach BACK to a start that has already gone past. This object is that somebody.


It searches, refines, retains, and emits the burst's SAMPLES. It stops there — demodulating, recording, or shipping a window elsewhere is the caller's business. `DsssBurstReceiver` is this plus `BurstDemod`.


It OWNS its acquisition engine rather than accepting someone else's results, and that is a correctness choice rather than a convenience one: `acq_result_t::samples_consumed` is stream-absolute only for an engine fed continuously and never reset, in the caller's own sample coordinates. An object taking foreign results would have to require that and could not check it — and a violated assumption is not a slightly wrong window, it is refine searching the wrong repetition, which returns noise rather than a degraded frame. `push()` defining the coordinate system makes the invariant internal. See docs/design/dsss-burst-receiver.md §11.


Lifecycle: create, then push() repeatedly, then destroy. There is no step()/steps(): a burst is a frame, not a sample.



```C++
// a 31-chip code held 4 samples a chip (bin_to_nrz, then the hold)
uint8_t code[31];
for (size_t i = 0; i < 31; i++) code[i] = (uint8_t)(i & 1u);
float nrz[31];
bin_to_nrz (code, 31, nrz, 31);
float _Complex pre[124];
for (size_t i = 0; i < 124; i++) pre[i] = nrz[i / 4];
burst_capture_state_t *cap = burst_capture_create (
    pre, 124, 4096, 4, 4.0e6, 55.0, 0.0, 1e-3, 0.9, 0, 0.0);
float _Complex x[2048] = { 0 };
float _Complex win[4096];
size_t n = burst_capture_push (cap, x, 2048, win, 4096);
// n is a multiple of burst_len: burst i starts at i*burst_len
burst_capture_destroy (cap);
```
 


    
## Public Functions Documentation




### function burst\_capture\_configure\_search\_raw 

_Pin the embedded acquisition's search grid directly._ 
```C++
int burst_capture_configure_search_raw (
    burst_capture_state_t * state,
    size_t doppler_bins,
    size_t n_noncoh
) 
```



The escape hatch for a caller who wants a specific (doppler\_bins, n\_noncoh). Forwards to the engine, with one refusal of this object's own: a grid whose anchor can lag the preamble by more than refine reaches  `n_noncoh * doppler_bins` code periods against `k_lo`  is rejected rather than accepted and silently mis-refined. Acquisition stamps a hit at the end of the LAST accumulated look, so every look past the one holding the preamble moves the anchor a whole frame later; a burst has one frame of preamble, so `n_noncoh = 1` is the grid a capture wants and the sizer now always picks (doppler#1181).




**Returns:**

DP\_OK, or DP\_ERR\_INVALID if this object or the engine refused the grid.



```C++
>>> import numpy as np
>>> from doppler.acquire import BurstCapture
>>> code = np.array([1, 1, 1, 0, 1, 0, 0], dtype=np.uint8)
>>> pre = np.repeat(np.where(code, -1.0, 1.0), 2).astype(np.complex64)
>>> cap = BurstCapture(pre, burst_len=512, reps=4, fs=2e6)
>>> cap.configure_search_raw(4, 1)   # 4 Doppler bins, coherent only
```
 


        

<hr>



### function burst\_capture\_create 

_Create a burst capture: acquisition, refine and retention behind one push()._ 
```C++
burst_capture_state_t * burst_capture_create (
    const float _Complex * preamble,
    size_t preamble_len,
    size_t burst_len,
    size_t reps,
    double fs,
    double cn0_dbhz,
    double doppler_uncertainty,
    double pfa,
    double pd,
    int noise_mode,
    double doppler_rate
) 
```



Give it the preamble  one period of its SAMPLES  and the geometry, say how long a burst is, and stream samples in. It searches blindly, recovers the exact preamble start, and hands back the burst's samples once they have all arrived. A `doppler_uncertainty` wider than the native span `+/- fs/(2n)` is searched in window tiles, and the event carries the absolute Doppler, tile included (doppler#1512)  for a preamble with a single-peak ambiguity, such as a PN code. A Zadoff-Chu preamble cannot be resolved past the native span: its delay-Doppler ridge correlates at full magnitude at (k tiles, k\*u^-1 samples) for every k, so keep its Doppler inside the span.


The look-back buffer is NOT a parameter. Its span is derived from the geometry here (detection lag + refine search + the burst itself), because every term is already known and a caller asked to size a history buffer is a caller handed a way to lose bursts silently.


Any repeated preamble: a chirp, a Zadoff-Chu sequence, shaped PSK, or a PN code mapped by [**bin\_to\_nrz()**](cvt__core_8h.md#function-bin_to_nrz) and held `spc` samples a chip (at `fs = chip_rate*spc`). One chip is one sample: a period is `preamble_len` samples. Refine correlates each candidate position against the acquisition engine's own reference row  the preamble at unit RMS  so there is one replica of it (doppler#1470).




**Parameters:**


* `preamble` One period of the preamble, `preamble_len` samples; not all zero, every sample finite. Read, not kept. 
* `preamble_len` Samples per repetition (&gt;= 1). 
* `burst_len` Samples in one burst  what gets captured. 
* `reps` Preamble repetitions (&gt;= 1). 
* `fs` Sample rate, Hz (&gt; 0); 1 for normalized units. 
* `cn0_dbhz` C/N0 the search is sized for, dB-Hz, of the preamble's mean power: any finite value, or NaN (ACQ\_CN0\_NONE) for no design point. 
* `doppler_uncertainty` Doppler search half-range, Hz (0 = native). 
* `pfa` Target false-alarm probability, in (0, 1). 
* `pd` Target detection probability, in (0, 1). 
* `noise_mode` CFAR reference: 0=mean, 1=median, 2=min, 3=max. 
* `doppler_rate` Doppler rate, Hz/s (&gt;= 0), that caps the acquisition's coherent depth at `f_epoch/sqrt(2*doppler_rate)` repetitions (doppler#1482); 0 is no bound. 



**Returns:**

Heap state, or NULL if any parameter is out of range.



```C++
>>> import numpy as np
>>> from doppler.acquire import BurstCapture
>>> code = np.array([1, 1, 1, 0, 1, 0, 0], dtype=np.uint8)
>>> pre = np.repeat(np.where(code, -1.0, 1.0), 2).astype(np.complex64)
>>> cap = BurstCapture(pre, burst_len=512, reps=4, fs=2e6)
>>> cap.burst_len
512
>>> cap.retain_span == cap.refine_span + cap.burst_len
True
```
 


        

<hr>



### function burst\_capture\_create\_backed 

_Create a capture whose look-back lives in a FILE._ 
```C++
burst_capture_state_t * burst_capture_create_backed (
    const char * path,
    const float _Complex * preamble,
    size_t preamble_len,
    size_t burst_len,
    size_t reps,
    double fs,
    double cn0_dbhz,
    double doppler_uncertainty,
    double pfa,
    double pd,
    int noise_mode,
    double doppler_rate
) 
```



Same object, same behaviour, one difference in where the history ring's pages come from: they are a `MAP_SHARED` mapping of `path`, so the ring's samples ARE the file's contents. There is no copy and no separate flush path — the kernel writes the pages back, and `get_state()` forces the point so a checkpoint and its history agree.


Two things follow, and they are the reason to reach for this constructor:



* **The blob stops carrying the look-back.** For an in-RAM capture the retained history IS the blob (measured 2026-09-24: 2.73 MB at a 1029-symbol frame, 16.84 MB at 8029). Backed, `state_bytes()` is a few hundred bytes plus the acquisition child, because the samples are already durable and the blob only has to name where in the ring they sit.
* **The history outlives the process.** Point a new capture at the same path and the samples are there; restore the blob and it reaches back across the restart into a burst that began before it.




The file is created if absent and truncated to the ring's byte size, which zeroes it. An existing file of exactly that size is adopted as it stands. Because the capacity rounds up to a page, that size is `capacity * sizeof(float _Complex)` — do not compute it from `burst_len`.


A blob from a backed capture does NOT restore into an in-RAM one, or the reverse: `state_bytes()` differs, so jm's length check rejects it. That is the intent — they are different configurations, and silently accepting one for the other would resume a capture whose history was somewhere else.




**Parameters:**


* `path` File to back the ring with; not NULL and not empty. 
* `preamble` One period of the preamble, `preamble_len` samples. 
* `preamble_len` Samples per repetition (&gt;= 1). 
* `burst_len` Samples in one burst  what gets captured. 
* `reps` Preamble repetitions (&gt;= 1). 
* `fs` Sample rate, Hz (&gt; 0). 
* `cn0_dbhz` C/N0 the search is sized for, dB-Hz: any finite value, or NaN (ACQ\_CN0\_NONE) for no design point. 
* `doppler_uncertainty` Doppler search half-range, Hz (0 = native). 
* `pfa` Target false-alarm probability, in (0, 1). 
* `pd` Target detection probability, in (0, 1). 
* `noise_mode` CFAR reference: 0=mean, 1=median, 2=min, 3=max. 
* `doppler_rate` Doppler rate, Hz/s (&gt;= 0), that caps the acquisition's coherent depth at `f_epoch/sqrt(2*doppler_rate)` repetitions (doppler#1482); 0 is no bound. 



**Returns:**

Heap state, or NULL if a parameter is out of range or the file could not be opened, sized or mapped.



```C++
>>> import numpy as np, tempfile, os
>>> from doppler.acquire import BurstCapture, PersistentBurstCapture
>>> code = np.array([1, 1, 1, 0, 1, 0, 0], dtype=np.uint8)
>>> pre = np.repeat(np.where(code, -1.0, 1.0), 2).astype(np.complex64)
>>> path = os.path.join(tempfile.mkdtemp(), "ring.cf32")
>>> cap = PersistentBurstCapture(path, pre, burst_len=512,
...                             reps=4, fs=2e6)
>>> ram = BurstCapture(pre, burst_len=512, reps=4, fs=2e6)
>>> _ = cap.push(np.zeros(4096, dtype=np.complex64))
>>> # the look-back is in the file, so the blob stops carrying it
>>> ram.state_bytes() - cap.state_bytes() == ram.retain_span * 8
True
>>> os.path.getsize(path) > 0
True
```
 


        

<hr>



### function burst\_capture\_destroy 

_Release a capture and everything it owns. NULL-safe._ 
```C++
void burst_capture_destroy (
    burst_capture_state_t * state
) 
```




<hr>



### function burst\_capture\_detections 

_Every hit the search made in the last push(), unfiltered._ 
```C++
size_t burst_capture_detections (
    burst_capture_state_t * state,
    size_t n,
    burst_capture_detection_t * out,
    size_t max_out
) 
```



BEFORE the claim rule and the suppression window: several rows can name one preamble, and a row can be a false alarm. That is the point  this is what acquisition FOUND, and `events()` is what survived. Valid until the next push(), reset() or set\_state().



```C++
>>> import numpy as np
>>> from doppler.acquire import BurstCapture
>>> code = np.array([1, 1, 1, 0, 1, 0, 0], dtype=np.uint8)
>>> pre = np.repeat(np.where(code, -1.0, 1.0), 2).astype(np.complex64)
>>> cap = BurstCapture(pre, burst_len=512, reps=4, fs=2e6)
>>> _ = cap.push(np.zeros(4096, dtype=np.complex64))
>>> # what the search found, against what became a burst
>>> len(cap.detections()) >= len(cap.events())
True
```
 


        

<hr>



### function burst\_capture\_detections\_max\_out 

_Raw detections available from the last push()._ `n` _is ignored._
```C++
size_t burst_capture_detections_max_out (
    burst_capture_state_t * state,
    size_t n
) 
```




<hr>



### function burst\_capture\_event\_at 

_Borrow event_ `i` _of the last push(), or NULL if out of range._
```C++
const burst_capture_event_t * burst_capture_event_at (
    const burst_capture_state_t * state,
    size_t i
) 
```




<hr>



### function burst\_capture\_events 

_The event record for each burst the last push() returned._ 
```C++
size_t burst_capture_events (
    burst_capture_state_t * state,
    size_t n,
    burst_capture_event_t * out,
    size_t max_out
) 
```



Row `i` describes the window at `i*burst_len`. Valid until the next push(), reset() or set\_state().



```C++
>>> import numpy as np
>>> from doppler.acquire import BurstCapture
>>> code = np.array([1, 1, 1, 0, 1, 0, 0], dtype=np.uint8)
>>> pre = np.repeat(np.where(code, -1.0, 1.0), 2).astype(np.complex64)
>>> cap = BurstCapture(pre, burst_len=512, reps=4, fs=2e6)
>>> win = cap.push(np.zeros(4096, dtype=np.complex64))
>>> len(cap.events()) == win.size // cap.burst_len
True
```
 


        

<hr>



### function burst\_capture\_events\_max\_out 

_Records available from the last push()._ `n` _is ignored._
```C++
size_t burst_capture_events_max_out (
    burst_capture_state_t * state,
    size_t n
) 
```




<hr>



### function burst\_capture\_get\_cn0\_dbhz\_est 

```C++
double burst_capture_get_cn0_dbhz_est (
    const burst_capture_state_t * state
) 
```




<hr>



### function burst\_capture\_get\_code\_bins 

_Code-phase hypotheses per Doppler row._ 
```C++
size_t burst_capture_get_code_bins (
    const burst_capture_state_t * state
) 
```




<hr>



### function burst\_capture\_get\_doppler\_bins 

_Doppler hypotheses searched: the coherent depth, or the window-tile count past the native span_  _acq\_grid\_bins() (doppler#1512)._
```C++
size_t burst_capture_get_doppler_bins (
    const burst_capture_state_t * state
) 
```




<hr>



### function burst\_capture\_get\_doppler\_hz\_est 

```C++
double burst_capture_get_doppler_hz_est (
    const burst_capture_state_t * state
) 
```




<hr>



### function burst\_capture\_get\_doppler\_rate 

_Doppler rate (Hz/s) the coherent depth is bounded against; 0 is no bound._ 
```C++
double burst_capture_get_doppler_rate (
    const burst_capture_state_t * state
) 
```




<hr>



### function burst\_capture\_get\_doppler\_res\_hz 

```C++
double burst_capture_get_doppler_res_hz (
    const burst_capture_state_t * state
) 
```




<hr>



### function burst\_capture\_get\_doppler\_span\_hz 

_Unambiguous Doppler half-range, Hz (+/- this)._ 
```C++
double burst_capture_get_doppler_span_hz (
    const burst_capture_state_t * state
) 
```




<hr>



### function burst\_capture\_get\_dropped 

```C++
uint64_t burst_capture_get_dropped (
    const burst_capture_state_t * state
) 
```




<hr>



### function burst\_capture\_get\_eta 

_Coherent detection gate; in force when_ `n_noncoh == 1` _._
```C++
double burst_capture_get_eta (
    const burst_capture_state_t * state
) 
```




<hr>



### function burst\_capture\_get\_eta\_nc 

_Non-coherent gate; in force only when_ `n_noncoh > 1` _, which a burst search never chooses (it reads 0 unless a pin sets looks)._
```C++
double burst_capture_get_eta_nc (
    const burst_capture_state_t * state
) 
```




<hr>



### function burst\_capture\_get\_min\_gap 

_Dead air a caller must leave between bursts, edge to edge._ 
```C++
size_t burst_capture_get_min_gap (
    const burst_capture_state_t * state
) 
```




<hr>



### function burst\_capture\_get\_n\_bursts 

```C++
uint64_t burst_capture_get_n_bursts (
    const burst_capture_state_t * state
) 
```




<hr>



### function burst\_capture\_get\_n\_noncoh 

_Non-coherent looks combined per decision._ 
```C++
size_t burst_capture_get_n_noncoh (
    const burst_capture_state_t * state
) 
```




<hr>



### function burst\_capture\_get\_pd\_burst 

_Detection probability of one burst: any dwell its preamble spans, at a uniform alignment (doppler#1498). What_ `underpowered` _reads._
```C++
double burst_capture_get_pd_burst (
    const burst_capture_state_t * state
) 
```



It models the ENGINE; what refine loses afterwards is not in the model. Measured on a Zadoff-Chu 127 x 8 preamble at every depth, the capture delivers at least this at a 0.6 design point and at 0.9 (the default `pd`): its loss against the engine is refine naming the wrong repetition, about 2% of trials (native/validation/capture\_dwell\_pd.c; validation report §2.8). Until doppler#1519 it also lost a code phase along the ZC delay-Doppler ridge and fell 0.026 short at 0.9. 


        

<hr>



### function burst\_capture\_get\_pd\_predicted 

_Detection probability of one dwell wholly inside the preamble._ 
```C++
double burst_capture_get_pd_predicted (
    const burst_capture_state_t * state
) 
```




<hr>



### function burst\_capture\_get\_pending 

```C++
size_t burst_capture_get_pending (
    const burst_capture_state_t * state
) 
```




<hr>



### function burst\_capture\_get\_preamble\_start 

```C++
uint64_t burst_capture_get_preamble_start (
    const burst_capture_state_t * state
) 
```




<hr>



### function burst\_capture\_get\_psl\_db 

_The preamble's peak sidelobe level, dB:_ [_**acq\_psl\_db()**_](acq__core_8h.md#function-acq_psl_db) _of the engine. A burst clearing the threshold by more than_`-psl_db` _also detects its own sidelobe, outside the peak zone; -INFINITY for a perfect sequence._
```C++
double burst_capture_get_psl_db (
    const burst_capture_state_t * state
) 
```




<hr>



### function burst\_capture\_get\_state 

_Serialize into_ `blob` _, which must be state\_bytes() long._
```C++
void burst_capture_get_state (
    const burst_capture_state_t * state,
    void * blob
) 
```




<hr>



### function burst\_capture\_get\_straddle\_loss 

_Correlation kept, worst case, by a burst landing between bins._ 
```C++
double burst_capture_get_straddle_loss (
    const burst_capture_state_t * state
) 
```




<hr>



### function burst\_capture\_push 

_Stream samples; get back every burst whose window has arrived._ 
```C++
size_t burst_capture_push (
    burst_capture_state_t * state,
    const float _Complex * x,
    size_t x_len,
    float _Complex * out,
    size_t max_out
) 
```



Windows are concatenated: burst `i` occupies `burst_len` samples starting at `i*burst_len`, and events() returns the matching record for each. Every sample of `x` is consumed. An empty return is normal  it means no burst completed in this call.




**Parameters:**


* `state` Capture. 
* `x` Input samples, `x_len` long. 
* `x_len` Samples in `x`. 
* `out` Written with the completed windows; may be NULL to drop. 
* `max_out` Capacity of `out`, in samples. 



**Returns:**

Samples written  always a multiple of `burst_len`.



```C++
>>> import numpy as np
>>> from doppler.acquire import BurstCapture
>>> code = np.array([1, 1, 1, 0, 1, 0, 0], dtype=np.uint8)
>>> pre = np.repeat(np.where(code, -1.0, 1.0), 2).astype(np.complex64)
>>> cap = BurstCapture(pre, burst_len=512, reps=4, fs=2e6)
>>> win = cap.push(np.zeros(4096, dtype=np.complex64))
>>> win.size % cap.burst_len        # whole windows, never a partial
0
>>> win.size                        # silence, so no burst completed
0
```
 


        

<hr>



### function burst\_capture\_push\_max\_out 

_Upper bound on samples push() can return for_ `x_len` _input._
```C++
size_t burst_capture_push_max_out (
    burst_capture_state_t * state,
    size_t x_len
) 
```



Distinct bursts cannot overlap, so `x_len` samples complete at most `x_len/burst_len + 1` of them, plus whatever is already queued. 


        

<hr>



### function burst\_capture\_ready 

_Windows the last push() completed._ 
```C++
size_t burst_capture_ready (
    const burst_capture_state_t * state
) 
```



The C consumer's face, and the reason a composing object pays no second copy: [**burst\_capture\_window()**](burst__capture__core_8h.md#function-burst_capture_window) borrows straight out of the scratch that push() filled. 


        

<hr>



### function burst\_capture\_release 

_Give back the span that window_ `i` _of the last push() claimed._
```C++
int burst_capture_release (
    burst_capture_state_t * state,
    size_t i
) 
```



An emitted window owns its whole span: a detection inside it is the payload firing against the acquisition code, not a new burst, so it is HELD rather than reported. Whether the window WAS a burst is a verdict this object cannot reach  it stops at samples; error detection, whatever form the frame gives it, is the consumer's  so a consumer that knows better calls this for that window, and the held detections are searched again on the next push(). Unreleased, they are dropped when the next push() begins, which is exactly the behaviour a consumer with no verdict always had.


What it prevents (doppler#1181): a spurious window ending just after a real burst begins used to swallow that burst's first detections  the receiver's own design says only a DECODED burst may own a span (§10.3, doppler#1004), and the capture underneath had been owning it on emission.


Must be called BEFORE the next push(): `i` indexes THIS push's windows.




**Returns:**

DP\_OK, or DP\_ERR\_INVALID if `i` is not a window of the last push().



```C++
>>> import numpy as np
>>> from doppler.acquire import BurstCapture
>>> code = np.array([1, 1, 1, 0, 1, 0, 0], dtype=np.uint8)
>>> pre = np.repeat(np.where(code, -1.0, 1.0), 2).astype(np.complex64)
>>> cap = BurstCapture(pre, burst_len=512, reps=4, fs=2e6)
>>> _ = cap.push(np.zeros(4096, dtype=np.complex64))
>>> cap.release(0)   # no window 0 in a quiet push
Traceback (most recent call last):
  ...
ValueError: release failed (rc=-4)
```
 


        

<hr>



### function burst\_capture\_reset 

_Return to the searching state._ 
```C++
void burst_capture_reset (
    burst_capture_state_t * state
) 
```



Resets the embedded acquisition, rewinds the history ring, clears every queued detection and every read-back. Construction parameters are untouched; `dropped` deliberately survives, because a lost burst stays lost.



```C++
>>> import numpy as np
>>> from doppler.acquire import BurstCapture
>>> code = np.array([1, 1, 1, 0, 1, 0, 0], dtype=np.uint8)
>>> pre = np.repeat(np.where(code, -1.0, 1.0), 2).astype(np.complex64)
>>> cap = BurstCapture(pre, burst_len=512, reps=4, fs=2e6)
>>> cap.push(np.zeros(4096, dtype=np.complex64)).size
0
>>> cap.reset()
>>> cap.pending
0
```
 


        

<hr>



### function burst\_capture\_set\_state 

_Restore from_ `blob` _._
```C++
int burst_capture_set_state (
    burst_capture_state_t * state,
    const void * blob
) 
```





**Returns:**

DP\_OK or DP\_ERR\_INVALID.


A wrong-object, wrong-version, wrong-size or foreign-endian blob is refused, never reinterpreted; so is a blob from the other flavour (a backed and an in-RAM capture have different `state_bytes()`). A backed capture restores POSITIONS only  the samples are the file's  so it also refuses a blob whose retained span the file cannot hold: a file create() made fresh that this capture has not written that far into, or a span the ring has since wrapped past (more than the ring's capacity pushed since the checkpoint). A capture restoring a checkpoint it took itself is the normal case and is accepted (doppler#1190): `set_state(blob) -> push(chunk) -> get_state()` per call is a service shape this object supports, on both flavours. 


        

<hr>



### function burst\_capture\_state\_bytes 

_Bytes one blob occupies: a pure function of CONFIGURATION._ 
```C++
size_t burst_capture_state_bytes (
    const burst_capture_state_t * state
) 
```




<hr>



### function burst\_capture\_window 

_Borrow window_ `i` _of the last push(), or NULL if out of range._
```C++
const float _Complex * burst_capture_window (
    const burst_capture_state_t * state,
    size_t i
) 
```



Contiguous, `burst_len` samples, valid until the next push(), reset() or set\_state(). The caller must not free it. 


        

<hr>
## Macro Definition Documentation





### define BURST\_CAPTURE\_EDGE\_TWINS 

```C++
#define BURST_CAPTURE_EDGE_TWINS `4u`
```



Extra Doppler cells refine scores at the other alias when a cell sits at the edge of the native span (doppler#1519). 


        

<hr>



### define BURST\_CAPTURE\_HITS 

_Detections collected from acquisition per batch._ 
```C++
#define BURST_CAPTURE_HITS `16u`
```



A BATCHING parameter, never a correctness one: push() loops until acq has absorbed the whole chunk, so a smaller array means more iterations and nothing else. Growing it to "be safe" would hide the fact that [**acq\_push()**](acq__core_8h.md#function-acq_push) stops once its result array is full and abandons the rest of its input. 


        

<hr>



### define BURST\_CAPTURE\_MAX\_PHASES 

```C++
#define BURST_CAPTURE_MAX_PHASES `4u`
```



Distinct code phases a pending burst remembers for refine to score. Two is what a Zadoff-Chu preamble at the edge of the native span produces (doppler#1519); the rest is room for a false alarm merged into the same burst. A phase past the cap is not recorded, and refine then behaves as it did before phases were kept. 


        

<hr>



### define BURST\_CAPTURE\_REFINE\_INTERP 

```C++
#define BURST_CAPTURE_REFINE_INTERP `4u`
```



Refine's Doppler cells per native bin at depth `reps`: twice acquisition's own zero-padding. Choosing a repetition compares two close scores, and the straddle that costs a detection little tips that comparison: measured on Zadoff-Chu 127 x 8, 2 cells a bin chose the wrong period for 45 of 656 engine hits at D = 5, 4 for 26, and 8 for 25 (doppler#1502). 


        

<hr>



### define BURST\_CAPTURE\_STATE\_MAGIC 

_State blob magic — a wrong blob is rejected, not reinterpreted._ 
```C++
#define BURST_CAPTURE_STATE_MAGIC `DP_FOURCC ('B', 'C', 'A', 'P')`
```




<hr>



### define BURST\_CAPTURE\_STATE\_VERSION 

_State blob layout version._ 
```C++
#define BURST_CAPTURE_STATE_VERSION `4u`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/burst_capture/burst_capture_core.h`

