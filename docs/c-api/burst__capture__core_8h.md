

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















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**burst\_capture\_event\_t**](structburst__capture__event__t.md) <br>_One captured burst's event, as_ `events()` _hands it back._ |
| struct | [**burst\_capture\_pending\_t**](structburst__capture__pending__t.md) <br>_One detection between acquisition and emission._  |
| struct | [**burst\_capture\_state\_t**](structburst__capture__state__t.md) <br>_BurstCapture state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  int | [**burst\_capture\_configure\_search\_raw**](#function-burst_capture_configure_search_raw) ([**burst\_capture\_state\_t**](structburst__capture__state__t.md) \* state, size\_t doppler\_bins, size\_t n\_noncoh) <br>_Pin the embedded acquisition's search grid directly._  |
|  [**burst\_capture\_state\_t**](structburst__capture__state__t.md) \* | [**burst\_capture\_create**](#function-burst_capture_create) (const uint8\_t \* acq\_code, size\_t acq\_code\_len, size\_t burst\_len, size\_t reps, size\_t spc, double chip\_rate, double cn0\_dbhz, double doppler\_uncertainty, double pfa, double pd) <br>_Create a burst capture: acquisition, refine and retention behind one push()._  |
|  [**burst\_capture\_state\_t**](structburst__capture__state__t.md) \* | [**burst\_capture\_create\_backed**](#function-burst_capture_create_backed) (const char \* path, const uint8\_t \* acq\_code, size\_t acq\_code\_len, size\_t burst\_len, size\_t reps, size\_t spc, double chip\_rate, double cn0\_dbhz, double doppler\_uncertainty, double pfa, double pd) <br>_Create a capture whose look-back lives in a FILE._  |
|  void | [**burst\_capture\_destroy**](#function-burst_capture_destroy) ([**burst\_capture\_state\_t**](structburst__capture__state__t.md) \* state) <br>_Release a capture and everything it owns. NULL-safe._  |
|  const [**burst\_capture\_event\_t**](structburst__capture__event__t.md) \* | [**burst\_capture\_event\_at**](#function-burst_capture_event_at) (const [**burst\_capture\_state\_t**](structburst__capture__state__t.md) \* state, size\_t i) <br>_Borrow event_ `i` _of the last push(), or NULL if out of range._ |
|  size\_t | [**burst\_capture\_events**](#function-burst_capture_events) ([**burst\_capture\_state\_t**](structburst__capture__state__t.md) \* state, size\_t n, [**burst\_capture\_event\_t**](structburst__capture__event__t.md) \* out, size\_t max\_out) <br>_The event record for each burst the last push() returned._  |
|  size\_t | [**burst\_capture\_events\_max\_out**](#function-burst_capture_events_max_out) ([**burst\_capture\_state\_t**](structburst__capture__state__t.md) \* state, size\_t n) <br>_Records available from the last push()._ `n` _is ignored._ |
|  double | [**burst\_capture\_get\_cn0\_dbhz\_est**](#function-burst_capture_get_cn0_dbhz_est) (const [**burst\_capture\_state\_t**](structburst__capture__state__t.md) \* state) <br> |
|  double | [**burst\_capture\_get\_doppler\_hz\_est**](#function-burst_capture_get_doppler_hz_est) (const [**burst\_capture\_state\_t**](structburst__capture__state__t.md) \* state) <br> |
|  double | [**burst\_capture\_get\_doppler\_res\_hz**](#function-burst_capture_get_doppler_res_hz) (const [**burst\_capture\_state\_t**](structburst__capture__state__t.md) \* state) <br> |
|  uint64\_t | [**burst\_capture\_get\_dropped**](#function-burst_capture_get_dropped) (const [**burst\_capture\_state\_t**](structburst__capture__state__t.md) \* state) <br> |
|  uint64\_t | [**burst\_capture\_get\_n\_bursts**](#function-burst_capture_get_n_bursts) (const [**burst\_capture\_state\_t**](structburst__capture__state__t.md) \* state) <br> |
|  size\_t | [**burst\_capture\_get\_pending**](#function-burst_capture_get_pending) (const [**burst\_capture\_state\_t**](structburst__capture__state__t.md) \* state) <br> |
|  uint64\_t | [**burst\_capture\_get\_preamble\_start**](#function-burst_capture_get_preamble_start) (const [**burst\_capture\_state\_t**](structburst__capture__state__t.md) \* state) <br> |
|  double | [**burst\_capture\_get\_refine\_margin**](#function-burst_capture_get_refine_margin) (const [**burst\_capture\_state\_t**](structburst__capture__state__t.md) \* state) <br> |
|  void | [**burst\_capture\_get\_state**](#function-burst_capture_get_state) (const [**burst\_capture\_state\_t**](structburst__capture__state__t.md) \* state, void \* blob) <br>_Serialize into_ `blob` _, which must be state\_bytes() long._ |
|  size\_t | [**burst\_capture\_push**](#function-burst_capture_push) ([**burst\_capture\_state\_t**](structburst__capture__state__t.md) \* state, const float complex \* x, size\_t x\_len, float complex \* out, size\_t max\_out) <br>_Stream samples; get back every burst whose window has arrived._  |
|  size\_t | [**burst\_capture\_push\_max\_out**](#function-burst_capture_push_max_out) ([**burst\_capture\_state\_t**](structburst__capture__state__t.md) \* state, size\_t x\_len) <br>_Upper bound on samples push() can return for_ `x_len` _input._ |
|  size\_t | [**burst\_capture\_ready**](#function-burst_capture_ready) (const [**burst\_capture\_state\_t**](structburst__capture__state__t.md) \* state) <br>_Windows the last push() completed._  |
|  void | [**burst\_capture\_reset**](#function-burst_capture_reset) ([**burst\_capture\_state\_t**](structburst__capture__state__t.md) \* state) <br>_Return to the searching state._  |
|  int | [**burst\_capture\_set\_state**](#function-burst_capture_set_state) ([**burst\_capture\_state\_t**](structburst__capture__state__t.md) \* state, const void \* blob) <br>_Restore from_ `blob` _._ |
|  size\_t | [**burst\_capture\_state\_bytes**](#function-burst_capture_state_bytes) (const [**burst\_capture\_state\_t**](structburst__capture__state__t.md) \* state) <br>_Bytes one blob occupies: a pure function of CONFIGURATION._  |
|  const float complex \* | [**burst\_capture\_window**](#function-burst_capture_window) (const [**burst\_capture\_state\_t**](structburst__capture__state__t.md) \* state, size\_t i) <br>_Borrow window_ `i` _of the last push(), or NULL if out of range._ |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**BURST\_CAPTURE\_HITS**](burst__capture__core_8h.md#define-burst_capture_hits)  `16u`<br>_Detections collected from acquisition per batch._  |
| define  | [**BURST\_CAPTURE\_STATE\_MAGIC**](burst__capture__core_8h.md#define-burst_capture_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('B', 'C', 'A', 'P')`<br>_State blob magic — a wrong blob is rejected, not reinterpreted._  |
| define  | [**BURST\_CAPTURE\_STATE\_VERSION**](burst__capture__core_8h.md#define-burst_capture_state_version)  `1u`<br>_State blob layout version._  |

## Detailed Description


Between a detector and whatever consumes a burst there is a stage nobody owned: acquisition reports an END anchor and a code phase that is a lag MODULO one code period, so it fixes the alignment WITHIN a repetition and never says WHICH one. A burst has a frame that begins in one specific repetition, so somebody has to resolve the period and reach BACK to a start that has already gone past. This object is that somebody.


It searches, refines, retains, and emits the burst's SAMPLES. It stops there — demodulating, recording, or shipping a window elsewhere is the caller's business. `DsssBurstReceiver` is this plus `BurstDemod`.


It OWNS its acquisition engine rather than accepting someone else's results, and that is a correctness choice rather than a convenience one: `acq_result_t::samples_consumed` is stream-absolute only for an engine fed continuously and never reset, in the caller's own sample coordinates. An object taking foreign results would have to require that and could not check it — and a violated assumption is not a slightly wrong window, it is refine searching the wrong repetition, which returns noise rather than a degraded frame. `push()` defining the coordinate system makes the invariant internal. See docs/design/dsss-burst-receiver.md §11.


Lifecycle: create, then push() repeatedly, then destroy. There is no step()/steps(): a burst is a frame, not a sample.



```C++
uint8_t code[31];
for (size_t i = 0; i < 31; i++) code[i] = (uint8_t)(i & 1u);
burst_capture_state_t *cap = burst_capture_create (
    code, 31, 4096, 4, 4, 1.0e6, 55.0, 0.0, 1e-3, 0.9);
float complex x[2048] = { 0 };
float complex win[4096];
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



The escape hatch for a caller who wants a specific (doppler\_bins, n\_noncoh). Forwards to the engine unchanged.




**Returns:**

DP\_OK, or DP\_ERR\_INVALID if the engine refused the grid.



```C++
>>> import numpy as np
>>> from doppler.dsss import BurstCapture
>>> code = np.array([1, 1, 1, 0, 1, 0, 0], dtype=np.uint8)
>>> cap = BurstCapture(code, burst_len=512, reps=4, spc=2)
>>> cap.configure_search_raw(4, 1)   # 4 Doppler bins, coherent only
```
 


        

<hr>



### function burst\_capture\_create 

_Create a burst capture: acquisition, refine and retention behind one push()._ 
```C++
burst_capture_state_t * burst_capture_create (
    const uint8_t * acq_code,
    size_t acq_code_len,
    size_t burst_len,
    size_t reps,
    size_t spc,
    double chip_rate,
    double cn0_dbhz,
    double doppler_uncertainty,
    double pfa,
    double pd
) 
```



Give it the preamble code and the geometry, say how long a burst is, and stream samples in. It searches blindly, recovers the exact preamble start, and hands back the burst's samples once they have all arrived.


The look-back buffer is NOT a parameter. Its span is derived from the geometry here (detection lag + refine search + the burst itself), because every term is already known and a caller asked to size a history buffer is a caller handed a way to lose bursts silently.




**Parameters:**


* `acq_code` Preamble PN chips (0/1), length `acq_code_len`. 
* `acq_code_len` Preamble code length, chips. 
* `burst_len` Samples in one burst  what gets captured. 
* `reps` Preamble code repetitions. 
* `spc` Samples per chip. 
* `chip_rate` Chip rate, Hz. 
* `cn0_dbhz` C/N0 the search is sized for, dB-Hz. 
* `doppler_uncertainty` Doppler search half-range, Hz (0 = native). 
* `pfa` Target false-alarm probability, in (0, 1). 
* `pd` Target detection probability, in (0, 1). 



**Returns:**

Heap state, or NULL if any parameter is out of range.



```C++
>>> import numpy as np
>>> from doppler.dsss import BurstCapture
>>> code = np.array([1, 1, 1, 0, 1, 0, 0], dtype=np.uint8)
>>> cap = BurstCapture(code, burst_len=512, reps=4, spc=2)
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
    const uint8_t * acq_code,
    size_t acq_code_len,
    size_t burst_len,
    size_t reps,
    size_t spc,
    double chip_rate,
    double cn0_dbhz,
    double doppler_uncertainty,
    double pfa,
    double pd
) 
```



Same object, same behaviour, one difference in where the history ring's pages come from: they are a `MAP_SHARED` mapping of `path`, so the ring's samples ARE the file's contents. There is no copy and no separate flush path — the kernel writes the pages back, and `get_state()` forces the point so a checkpoint and its history agree.


Two things follow, and they are the reason to reach for this constructor:



* **The blob stops carrying the look-back.** For an in-RAM capture the retained history IS the blob (measured: 2.57 MB at a 1029-symbol frame, 16.68 MB at 8029). Backed, `state_bytes()` is a few hundred bytes plus the acquisition child, because the samples are already durable and the blob only has to name where in the ring they sit.
* **The history outlives the process.** Point a new capture at the same path and the samples are there; restore the blob and it reaches back across the restart into a burst that began before it.




The file is created if absent and truncated to the ring's byte size, which zeroes it. An existing file of exactly that size is adopted as it stands. Because the capacity rounds up to a page, that size is `capacity * sizeof(float complex)` — do not compute it from `burst_len`.


A blob from a backed capture does NOT restore into an in-RAM one, or the reverse: `state_bytes()` differs, so jm's length check rejects it. That is the intent — they are different configurations, and silently accepting one for the other would resume a capture whose history was somewhere else.




**Parameters:**


* `path` File to back the ring with; not NULL and not empty. 
* `acq_code` Preamble PN chips (0/1), length `acq_code_len`. 
* `acq_code_len` Preamble code length, chips. 
* `burst_len` Samples in one burst  what gets captured. 
* `reps` Preamble code repetitions. 
* `spc` Samples per chip. 
* `chip_rate` Chip rate, Hz. 
* `cn0_dbhz` C/N0 the search is sized for, dB-Hz. 
* `doppler_uncertainty` Doppler search half-range, Hz (0 = native). 
* `pfa` Target false-alarm probability, in (0, 1). 
* `pd` Target detection probability, in (0, 1). 



**Returns:**

Heap state, or NULL if a parameter is out of range or the file could not be opened, sized or mapped.



```C++
>>> import numpy as np, tempfile, os
>>> from doppler.dsss import BurstCapture, PersistentBurstCapture
>>> code = np.array([1, 1, 1, 0, 1, 0, 0], dtype=np.uint8)
>>> path = os.path.join(tempfile.mkdtemp(), "ring.cf32")
>>> cap = PersistentBurstCapture(path, code, burst_len=512,
...                             reps=4, spc=2)
>>> ram = BurstCapture(code, burst_len=512, reps=4, spc=2)
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
>>> from doppler.dsss import BurstCapture
>>> code = np.array([1, 1, 1, 0, 1, 0, 0], dtype=np.uint8)
>>> cap = BurstCapture(code, burst_len=512, reps=4, spc=2)
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



### function burst\_capture\_get\_doppler\_hz\_est 

```C++
double burst_capture_get_doppler_hz_est (
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



### function burst\_capture\_get\_dropped 

```C++
uint64_t burst_capture_get_dropped (
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



### function burst\_capture\_get\_refine\_margin 

```C++
double burst_capture_get_refine_margin (
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



### function burst\_capture\_push 

_Stream samples; get back every burst whose window has arrived._ 
```C++
size_t burst_capture_push (
    burst_capture_state_t * state,
    const float complex * x,
    size_t x_len,
    float complex * out,
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
>>> from doppler.dsss import BurstCapture
>>> code = np.array([1, 1, 1, 0, 1, 0, 0], dtype=np.uint8)
>>> cap = BurstCapture(code, burst_len=512, reps=4, spc=2)
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
>>> from doppler.dsss import BurstCapture
>>> code = np.array([1, 1, 1, 0, 1, 0, 0], dtype=np.uint8)
>>> cap = BurstCapture(code, burst_len=512, reps=4, spc=2)
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
const float complex * burst_capture_window (
    const burst_capture_state_t * state,
    size_t i
) 
```



Contiguous, `burst_len` samples, valid until the next push(), reset() or set\_state(). The caller must not free it. 


        

<hr>
## Macro Definition Documentation





### define BURST\_CAPTURE\_HITS 

_Detections collected from acquisition per batch._ 
```C++
#define BURST_CAPTURE_HITS `16u`
```



A BATCHING parameter, never a correctness one: push() loops until acq has absorbed the whole chunk, so a smaller array means more iterations and nothing else. Growing it to "be safe" would hide the fact that [**acq\_push()**](acq__core_8h.md#function-acq_push) stops once its result array is full and abandons the rest of its input. 


        

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
#define BURST_CAPTURE_STATE_VERSION `1u`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/burst_capture/burst_capture_core.h`

