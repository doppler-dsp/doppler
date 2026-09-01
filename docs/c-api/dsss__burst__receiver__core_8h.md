

# File dsss\_burst\_receiver\_core.h



[**FileList**](files.md) **>** [**dsss\_burst\_receiver**](dir_32a143d35207eb7d99f4a541895f77eb.md) **>** [**dsss\_burst\_receiver\_core.h**](dsss__burst__receiver__core_8h.md)

[Go to the source code of this file](dsss__burst__receiver__core_8h_source.md)

_DsssBurstReceiver — the burst chain composed in C._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "jm_perf.h"`
* `#include "buffer/buffer.h"`
* `#include "dp_state.h"`
* `#include "burst_capture/burst_capture_core.h"`
* `#include "burst_acq/burst_acq_core.h"`
* `#include "acq/acq_core.h"`
* `#include "burst_demod/burst_demod_core.h"`
* `#include "burst_despreader/burst_despreader_core.h"`
* `#include "ppe/ppe_core.h"`
* `#include "corr/corr_core.h"`
* `#include "corr2d/corr2d_core.h"`
* `#include "fft2d/fft2d_core.h"`
* `#include "spectral/spectral_core.h"`
* `#include "loop_filter/loop_filter_core.h"`
* `#include "detection/detection_core.h"`
* `#include "fft/fft_core.h"`
* `#include "pn/pn_core.h"`
* `#include "conv/conv_core.h"`
* `#include "rs/rs_core.h"`
* `#include "gold/gold_core.h"`
* `#include "mpsk/mpsk_core.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**dsss\_br\_event\_t**](structdsss__br__event__t.md) <br>_One completed burst's event, as_ `events()` _hands it back._ |
| struct | [**dsss\_burst\_receiver\_state\_t**](structdsss__burst__receiver__state__t.md) <br>_DsssBurstReceiver state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  int | [**dsss\_burst\_receiver\_configure\_search\_raw**](#function-dsss_burst_receiver_configure_search_raw) ([**dsss\_burst\_receiver\_state\_t**](structdsss__burst__receiver__state__t.md) \* state, size\_t doppler\_bins, size\_t n\_noncoh) <br>_Pin the acquisition search grid, bypassing the auto-sizing._  |
|  [**dsss\_burst\_receiver\_state\_t**](structdsss__burst__receiver__state__t.md) \* | [**dsss\_burst\_receiver\_create**](#function-dsss_burst_receiver_create) (const uint8\_t \* acq\_code, size\_t acq\_code\_len, const uint8\_t \* data\_code, size\_t data\_code\_len, const uint8\_t \* sync, size\_t sync\_len, size\_t reps, size\_t spc, double chip\_rate, size\_t frame\_syms, double cn0\_dbhz, double doppler\_uncertainty, double pfa, double pd, double carrier\_hz, double max\_rate, size\_t est\_segments) <br>_Create a burst receiver: acquisition, refine and demodulation composed behind one push()._  |
|  void | [**dsss\_burst\_receiver\_destroy**](#function-dsss_burst_receiver_destroy) ([**dsss\_burst\_receiver\_state\_t**](structdsss__burst__receiver__state__t.md) \* state) <br>_Destroy a dsss\_burst\_receiver instance and release all memory._  |
|  size\_t | [**dsss\_burst\_receiver\_events**](#function-dsss_burst_receiver_events) ([**dsss\_burst\_receiver\_state\_t**](structdsss__burst__receiver__state__t.md) \* state, size\_t n, [**dsss\_br\_event\_t**](structdsss__br__event__t.md) \* out, size\_t max\_out) <br>_The event record for each burst the last push() returned._  |
|  size\_t | [**dsss\_burst\_receiver\_events\_max\_out**](#function-dsss_burst_receiver_events_max_out) ([**dsss\_burst\_receiver\_state\_t**](structdsss__burst__receiver__state__t.md) \* state) <br>_Max records events() writes: one per burst the last push() returned._  |
|  double | [**dsss\_burst\_receiver\_get\_cn0\_dbhz\_est**](#function-dsss_burst_receiver_get_cn0_dbhz_est) (const [**dsss\_burst\_receiver\_state\_t**](structdsss__burst__receiver__state__t.md) \* state) <br> |
|  double | [**dsss\_burst\_receiver\_get\_doppler\_hz\_est**](#function-dsss_burst_receiver_get_doppler_hz_est) (const [**dsss\_burst\_receiver\_state\_t**](structdsss__burst__receiver__state__t.md) \* state) <br> |
|  double | [**dsss\_burst\_receiver\_get\_doppler\_res\_hz**](#function-dsss_burst_receiver_get_doppler_res_hz) (const [**dsss\_burst\_receiver\_state\_t**](structdsss__burst__receiver__state__t.md) \* state) <br> |
|  uint64\_t | [**dsss\_burst\_receiver\_get\_dropped**](#function-dsss_burst_receiver_get_dropped) (const [**dsss\_burst\_receiver\_state\_t**](structdsss__burst__receiver__state__t.md) \* state) <br> |
|  double | [**dsss\_burst\_receiver\_get\_est\_freq\_hz**](#function-dsss_burst_receiver_get_est_freq_hz) (const [**dsss\_burst\_receiver\_state\_t**](structdsss__burst__receiver__state__t.md) \* state) <br> |
|  double | [**dsss\_burst\_receiver\_get\_est\_rate\_hz**](#function-dsss_burst_receiver_get_est_rate_hz) (const [**dsss\_burst\_receiver\_state\_t**](structdsss__burst__receiver__state__t.md) \* state) <br> |
|  double | [**dsss\_burst\_receiver\_get\_est\_snr\_db**](#function-dsss_burst_receiver_get_est_snr_db) (const [**dsss\_burst\_receiver\_state\_t**](structdsss__burst__receiver__state__t.md) \* state) <br> |
|  size\_t | [**dsss\_burst\_receiver\_get\_min\_gap**](#function-dsss_burst_receiver_get_min_gap) (const [**dsss\_burst\_receiver\_state\_t**](structdsss__burst__receiver__state__t.md) \* state) <br> |
|  uint64\_t | [**dsss\_burst\_receiver\_get\_n\_bursts**](#function-dsss_burst_receiver_get_n_bursts) (const [**dsss\_burst\_receiver\_state\_t**](structdsss__burst__receiver__state__t.md) \* state) <br> |
|  size\_t | [**dsss\_burst\_receiver\_get\_pending**](#function-dsss_burst_receiver_get_pending) (const [**dsss\_burst\_receiver\_state\_t**](structdsss__burst__receiver__state__t.md) \* state) <br> |
|  uint64\_t | [**dsss\_burst\_receiver\_get\_preamble\_start**](#function-dsss_burst_receiver_get_preamble_start) (const [**dsss\_burst\_receiver\_state\_t**](structdsss__burst__receiver__state__t.md) \* state) <br> |
|  double | [**dsss\_burst\_receiver\_get\_refine\_margin**](#function-dsss_burst_receiver_get_refine_margin) (const [**dsss\_burst\_receiver\_state\_t**](structdsss__burst__receiver__state__t.md) \* state) <br> |
|  size\_t | [**dsss\_burst\_receiver\_get\_refine\_span**](#function-dsss_burst_receiver_get_refine_span) (const [**dsss\_burst\_receiver\_state\_t**](structdsss__burst__receiver__state__t.md) \* state) <br> |
|  size\_t | [**dsss\_burst\_receiver\_get\_retain\_span**](#function-dsss_burst_receiver_get_retain_span) (const [**dsss\_burst\_receiver\_state\_t**](structdsss__burst__receiver__state__t.md) \* state) <br> |
|  void | [**dsss\_burst\_receiver\_get\_state**](#function-dsss_burst_receiver_get_state) (const [**dsss\_burst\_receiver\_state\_t**](structdsss__burst__receiver__state__t.md) \* state, void \* blob) <br>_Serialize_ `state's` _cross-call state into_`blob` _(caller-owned,_[_**dsss\_burst\_receiver\_state\_bytes()**_](dsss__burst__receiver__core_8h.md#function-dsss_burst_receiver_state_bytes) _long)._ |
|  size\_t | [**dsss\_burst\_receiver\_llrs**](#function-dsss_burst_receiver_llrs) ([**dsss\_burst\_receiver\_state\_t**](structdsss__burst__receiver__state__t.md) \* state, size\_t n, float \* out, size\_t max\_out) <br>_The SOFT bits of every burst the last push() returned._  |
|  size\_t | [**dsss\_burst\_receiver\_llrs\_max\_out**](#function-dsss_burst_receiver_llrs_max_out) ([**dsss\_burst\_receiver\_state\_t**](structdsss__burst__receiver__state__t.md) \* state, size\_t n) <br>_Max LLRs llrs() writes: frame bits x the bursts the last push returned._  |
|  size\_t | [**dsss\_burst\_receiver\_push**](#function-dsss_burst_receiver_push) ([**dsss\_burst\_receiver\_state\_t**](structdsss__burst__receiver__state__t.md) \* state, const float complex \* x, size\_t x\_len, uint8\_t \* out, size\_t max\_out) <br>_Stream samples; return the payload of EVERY burst that completed._  |
|  size\_t | [**dsss\_burst\_receiver\_push\_max\_out**](#function-dsss_burst_receiver_push_max_out) ([**dsss\_burst\_receiver\_state\_t**](structdsss__burst__receiver__state__t.md) \* state, size\_t x\_len) <br>_Max bits push() can write for an input of_ `x_len` _samples._ |
|  void | [**dsss\_burst\_receiver\_reset**](#function-dsss_burst_receiver_reset) ([**dsss\_burst\_receiver\_state\_t**](structdsss__burst__receiver__state__t.md) \* state) <br>_Return to searching: drop the history and clear every read-back._  |
|  int | [**dsss\_burst\_receiver\_set\_state**](#function-dsss_burst_receiver_set_state) ([**dsss\_burst\_receiver\_state\_t**](structdsss__burst__receiver__state__t.md) \* state, const void \* blob) <br>_Restore cross-call state from_ `blob` _(replacing it)._ |
|  size\_t | [**dsss\_burst\_receiver\_state\_bytes**](#function-dsss_burst_receiver_state_bytes) (const [**dsss\_burst\_receiver\_state\_t**](structdsss__burst__receiver__state__t.md) \* state) <br>_Byte size of_ `state's` _blob (envelope + payload + child)._ |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**DSSS\_BURST\_RECEIVER\_STATE\_MAGIC**](dsss__burst__receiver__core_8h.md#define-dsss_burst_receiver_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc)('D', 'B', 'R', 'X')`<br>_Per-object envelope tag: "DBRX" (DsssBurstReceiver)._  |
| define  | [**DSSS\_BURST\_RECEIVER\_STATE\_VERSION**](dsss__burst__receiver__core_8h.md#define-dsss_burst_receiver_state_version)  `5u`<br> |

## Detailed Description


Composes the three certified burst objects behind one push(): acquisition SEARCHES the stream, a refine stage recovers the exact preamble start, and the demodulator produces the payload. It owns the hand-off between them  the epoch, the fold, and the look-back reaching back to a burst start that has already gone past  which is the part every caller previously redid by hand. See docs/design/dsss-burst-receiver.md.


Lifecycle: create, then push() repeatedly, then destroy. There is no step()/steps(): a burst is a frame, not a sample.



```C++
uint8_t acq[31], data[8], sync[13];
dsss_burst_receiver_state_t *rx = dsss_burst_receiver_create (
    acq, 31, data, 8, sync, 13, 4, 4, 1.0e6, 61,
    55.0, 0.0, 1e-3, 0.9, 0.0, 0.0, 10);
uint8_t bits[61];   // frame_syms per burst: sync | payload | CRC
size_t n = dsss_burst_receiver_push (rx, samples, n_samples, bits, 61);
// the bits are the FRAME as received; undoing it is a Frame's job
dsss_burst_receiver_destroy (rx);
```
 


    
## Public Functions Documentation




### function dsss\_burst\_receiver\_configure\_search\_raw 

_Pin the acquisition search grid, bypassing the auto-sizing._ 
```C++
int dsss_burst_receiver_configure_search_raw (
    dsss_burst_receiver_state_t * state,
    size_t doppler_bins,
    size_t n_noncoh
) 
```



The escape hatch for a caller who wants a specific (doppler\_bins, n\_noncoh) rather than the grid the cn0\_dbhz/pfa/pd sizing chooses. Forwards to the embedded engine unchanged.




**Parameters:**


* `state` Must be non-NULL. 
* `doppler_bins` Coherent depth to pin, in `[1, reps]`. 
* `n_noncoh` Non-coherent looks to combine. 



**Returns:**

0 on success, non-zero if the grid is out of range. 
```C++
>>> import numpy as np
>>> from doppler.dsss import DsssBurstReceiver
>>> rng = np.random.default_rng(0)
>>> rx = DsssBurstReceiver(
...     rng.integers(0, 2, 31).astype(np.uint8),
...     rng.integers(0, 2, 8).astype(np.uint8),
...     np.zeros(13, dtype=np.uint8), reps=4, spc=4, frame_syms=32)
>>> rx.configure_search_raw(doppler_bins=1, n_noncoh=1)
```
 





        

<hr>



### function dsss\_burst\_receiver\_create 

_Create a burst receiver: acquisition, refine and demodulation composed behind one push()._ 
```C++
dsss_burst_receiver_state_t * dsss_burst_receiver_create (
    const uint8_t * acq_code,
    size_t acq_code_len,
    const uint8_t * data_code,
    size_t data_code_len,
    const uint8_t * sync,
    size_t sync_len,
    size_t reps,
    size_t spc,
    double chip_rate,
    size_t frame_syms,
    double cn0_dbhz,
    double doppler_uncertainty,
    double pfa,
    double pd,
    double carrier_hz,
    double max_rate,
    size_t est_segments
) 
```



Give it the waveform  the two codes and the frame sync word  plus the geometry, and stream samples in. It searches blindly for a burst, recovers the exact preamble start, and demodulates, publishing one detection event per burst through the read-back fields.


The look-back buffer is NOT a parameter. Its span is derived from the geometry here (detection lag + refine search + the burst itself), because every term is already known and a caller asked to size a history buffer is a caller handed a way to lose bursts silently.




**Parameters:**


* `acq_code` Preamble PN chips (0/1), length `acq_code_len`. 
* `acq_code_len` Preamble code length, chips. 
* `data_code` Payload spreading chips (0/1), `data_code_len` long. 
* `data_code_len` Data code length, chips. 
* `sync` Frame sync word (0/1 symbols), `sync_len` long. 
* `sync_len` Sync word length, symbols. 
* `reps` Preamble code repetitions (&gt;= 1). 
* `spc` Samples per chip (&gt;= 1). 
* `chip_rate` Chip rate in Hz (&gt; 0). 
* `frame_syms` Frame symbols per burst (&gt;= 1) — what push() returns, bit for bit. 
* `cn0_dbhz` Carrier-to-noise density in dB-Hz (&gt; 0), sizing the acquisition search. 
* `doppler_uncertainty` One-sided Doppler half-range, Hz. 
* `pfa` Target false-alarm probability, in (0, 1). 
* `pd` Target detection probability, in (0, 1). 
* `carrier_hz` RF carrier (Hz) for code-Doppler; 0 = ignore. 
* `max_rate` Chirp-rate search half-span (cycles/sample^2). 
* `est_segments` Segments the feedforward estimator fits over. 



**Returns:**

Heap-allocated state, or NULL if any argument is invalid. 




**Note:**

Caller must call [**dsss\_burst\_receiver\_destroy()**](dsss__burst__receiver__core_8h.md#function-dsss_burst_receiver_destroy) when done. 
```C++
>>> import numpy as np
>>> from doppler.dsss import DsssBurstReceiver
>>> rng = np.random.default_rng(0)
>>> acq = rng.integers(0, 2, 31).astype(np.uint8)
>>> dat = rng.integers(0, 2, 8).astype(np.uint8)
>>> syn = np.zeros(13, dtype=np.uint8)
>>> rx = DsssBurstReceiver(acq, dat, syn, reps=4, spc=4,
...                        frame_syms=32)
>>> rx.n_bursts
0
```
 





        

<hr>



### function dsss\_burst\_receiver\_destroy 

_Destroy a dsss\_burst\_receiver instance and release all memory._ 
```C++
void dsss_burst_receiver_destroy (
    dsss_burst_receiver_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function dsss\_burst\_receiver\_events 

_The event record for each burst the last push() returned._ 
```C++
size_t dsss_burst_receiver_events (
    dsss_burst_receiver_state_t * state,
    size_t n,
    dsss_br_event_t * out,
    size_t max_out
) 
```



Row `i` describes the payload at `out[i*frame_syms ...]` of that push. A single push can complete many bursts and each needs its own event, so these are a list rather than the scalar read-backs  those still exist and still describe the LAST burst, but they cannot speak for the others.


Valid until the next push(), reset() or set\_state(). Deliberately not serialized: it describes one call, and keeping it out of the blob is what holds state\_bytes() to a pure function of configuration.




**Parameters:**


* `state` Must be non-NULL. 
* `n` Ignored. The record count is whatever the last push() produced, not something a caller chooses; this parameter exists because every variable-output method carries one, and the binding uses it only as a floor on the buffer it allocates. 
* `out` Records, caller-owned, `max_out` long. 
* `max_out` Capacity of `out`; see events\_max\_out(). 



**Returns:**

Records written to `out`  `min(events_max_out(), max_out)`. 
```C++
>>> import numpy as np
>>> from doppler.dsss import DsssBurstReceiver
>>> rng = np.random.default_rng(0)
>>> rx = DsssBurstReceiver(
...     rng.integers(0, 2, 31).astype(np.uint8),
...     rng.integers(0, 2, 8).astype(np.uint8),
...     np.zeros(13, dtype=np.uint8), reps=4, spc=4, frame_syms=32)
>>> bits = rx.push(np.zeros(4096, dtype=np.complex64))
>>> len(rx.events()) == bits.size // 32   # one record per payload
True
```
 





        

<hr>



### function dsss\_burst\_receiver\_events\_max\_out 

_Max records events() writes: one per burst the last push() returned._ 
```C++
size_t dsss_burst_receiver_events_max_out (
    dsss_burst_receiver_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

The number of bursts the most recent push() completed. 





        

<hr>



### function dsss\_burst\_receiver\_get\_cn0\_dbhz\_est 

```C++
double dsss_burst_receiver_get_cn0_dbhz_est (
    const dsss_burst_receiver_state_t * state
) 
```




<hr>



### function dsss\_burst\_receiver\_get\_doppler\_hz\_est 

```C++
double dsss_burst_receiver_get_doppler_hz_est (
    const dsss_burst_receiver_state_t * state
) 
```




<hr>



### function dsss\_burst\_receiver\_get\_doppler\_res\_hz 

```C++
double dsss_burst_receiver_get_doppler_res_hz (
    const dsss_burst_receiver_state_t * state
) 
```




<hr>



### function dsss\_burst\_receiver\_get\_dropped 

```C++
uint64_t dsss_burst_receiver_get_dropped (
    const dsss_burst_receiver_state_t * state
) 
```




<hr>



### function dsss\_burst\_receiver\_get\_est\_freq\_hz 

```C++
double dsss_burst_receiver_get_est_freq_hz (
    const dsss_burst_receiver_state_t * state
) 
```




<hr>



### function dsss\_burst\_receiver\_get\_est\_rate\_hz 

```C++
double dsss_burst_receiver_get_est_rate_hz (
    const dsss_burst_receiver_state_t * state
) 
```




<hr>



### function dsss\_burst\_receiver\_get\_est\_snr\_db 

```C++
double dsss_burst_receiver_get_est_snr_db (
    const dsss_burst_receiver_state_t * state
) 
```




<hr>



### function dsss\_burst\_receiver\_get\_min\_gap 

```C++
size_t dsss_burst_receiver_get_min_gap (
    const dsss_burst_receiver_state_t * state
) 
```




<hr>



### function dsss\_burst\_receiver\_get\_n\_bursts 

```C++
uint64_t dsss_burst_receiver_get_n_bursts (
    const dsss_burst_receiver_state_t * state
) 
```




<hr>



### function dsss\_burst\_receiver\_get\_pending 

```C++
size_t dsss_burst_receiver_get_pending (
    const dsss_burst_receiver_state_t * state
) 
```




<hr>



### function dsss\_burst\_receiver\_get\_preamble\_start 

```C++
uint64_t dsss_burst_receiver_get_preamble_start (
    const dsss_burst_receiver_state_t * state
) 
```




<hr>



### function dsss\_burst\_receiver\_get\_refine\_margin 

```C++
double dsss_burst_receiver_get_refine_margin (
    const dsss_burst_receiver_state_t * state
) 
```




<hr>



### function dsss\_burst\_receiver\_get\_refine\_span 

```C++
size_t dsss_burst_receiver_get_refine_span (
    const dsss_burst_receiver_state_t * state
) 
```




<hr>



### function dsss\_burst\_receiver\_get\_retain\_span 

```C++
size_t dsss_burst_receiver_get_retain_span (
    const dsss_burst_receiver_state_t * state
) 
```




<hr>



### function dsss\_burst\_receiver\_get\_state 

_Serialize_ `state's` _cross-call state into_`blob` _(caller-owned,_[_**dsss\_burst\_receiver\_state\_bytes()**_](dsss__burst__receiver__core_8h.md#function-dsss_burst_receiver_state_bytes) _long)._
```C++
void dsss_burst_receiver_get_state (
    const dsss_burst_receiver_state_t * state,
    void * blob
) 
```




<hr>



### function dsss\_burst\_receiver\_llrs 

_The SOFT bits of every burst the last push() returned._ 
```C++
size_t dsss_burst_receiver_llrs (
    dsss_burst_receiver_state_t * state,
    size_t n,
    float * out,
    size_t max_out
) 
```



`crealf(sym * derot)` IS the log-likelihood ratio up to a scale, and the demodulator used to compute it, slice it to one bit and free it. A hard decision throws away roughly 2 dB of the coding gain a soft-input decoder exists to deliver (`mpsk_soft_demap`'s own docstring), which is what makes a coded burst worth coding.


Concatenated the same way push()'s payloads are, one row of `frame_bits` per burst: burst `i` starts at `i * frame_bits`, in the order events() reports. The convention is `mpsk_soft_demap`'s — positive means bit 0, so `L < 0` reproduces exactly the bits push() returned. Spans the WHOLE frame rather than the payload alone, because a code covers what its description says it covers.


Valid until the next push(), reset() or set\_state(); deliberately not serialized, for the same reason events() is not: it describes one call.




**Parameters:**


* `state` Receiver handle. 
* `n` Ignored — the count is the last push's, not a request. 
* `out` Receives the LLRs. 
* `max_out` Capacity of `out`; see llrs\_max\_out(). 



**Returns:**

LLRs written. 
```C++
>>> import numpy as np
>>> from doppler.dsss import DsssBurstReceiver
>>> rng = np.random.default_rng(0)
>>> rx = DsssBurstReceiver(
...     rng.integers(0, 2, 31).astype(np.uint8),
...     rng.integers(0, 2, 8).astype(np.uint8),
...     np.zeros(13, dtype=np.uint8), reps=4, spc=4, frame_syms=32)
>>> bits = rx.push(np.zeros(4096, dtype=np.complex64))
>>> len(bits), len(rx.llrs(rx.llrs_max_out(1)))   # nothing decoded
(0, 0)
```
 





        

<hr>



### function dsss\_burst\_receiver\_llrs\_max\_out 

_Max LLRs llrs() writes: frame bits x the bursts the last push returned._ 
```C++
size_t dsss_burst_receiver_llrs_max_out (
    dsss_burst_receiver_state_t * state,
    size_t n
) 
```





**Parameters:**


* `state` Receiver handle. 
* `n` Ignored, as in llrs(). 




        

<hr>



### function dsss\_burst\_receiver\_push 

_Stream samples; return the payload of EVERY burst that completed._ 
```C++
size_t dsss_burst_receiver_push (
    dsss_burst_receiver_state_t * state,
    const float complex * x,
    size_t x_len,
    uint8_t * out,
    size_t max_out
) 
```



Retains `x` in the history ring and feeds the embedded acquisition. When a detection fires, the refine stage correlates the whole preamble to recover the exact preamble start  the quantity acquisition structurally cannot report, its code phase being a lag modulo one code period  and the burst is demodulated once its last sample has arrived.


EVERY SAMPLE OF `x` IS CONSUMED, and every burst that completes is returned by the call that completed it. Payloads are concatenated, so burst `i` occupies `out` from `i*frame_syms`, and `events()` returns the matching record for each. Returning 0 is normal, not an error: it means no burst completed in this call.


This is the contract doppler#1008 broke. push() used to return at most one burst per call AND abandon the rest of its input to do it, so a block carrying several bursts lost all but the first  measured at 6/6 decoded with 333-sample blocks against 1/6 with one large one. The history ring is a contiguous window over the stream and is never reset between bursts, so a payload whose tail falls outside one call is completed by a later one.




**Parameters:**


* `state` Must be non-NULL. 
* `x` Input samples (cf32), `x_len` long. 
* `x_len` Number of input samples. 
* `out` Payload bits, caller-owned, `max_out` long. 
* `max_out` Capacity of `out`; see push\_max\_out(). 



**Returns:**

Bits written to `out`  `n_bursts_returned * frame_syms`. 
```C++
>>> import numpy as np
>>> from doppler.dsss import DsssBurstReceiver
>>> rng = np.random.default_rng(0)
>>> rx = DsssBurstReceiver(
...     rng.integers(0, 2, 31).astype(np.uint8),
...     rng.integers(0, 2, 8).astype(np.uint8),
...     np.zeros(13, dtype=np.uint8), reps=4, spc=4, frame_syms=32)
>>> bits = rx.push(np.zeros(4096, dtype=np.complex64))
>>> bits.size            # silence carries no burst
0
```
 





        

<hr>



### function dsss\_burst\_receiver\_push\_max\_out 

_Max bits push() can write for an input of_ `x_len` _samples._
```C++
size_t dsss_burst_receiver_push_max_out (
    dsss_burst_receiver_state_t * state,
    size_t x_len
) 
```



push() returns EVERY burst it completed, so the bound scales with the input: distinct bursts cannot overlap, so they are at least `burst_len` apart, and a push of `x_len` samples can complete at most `x_len/burst_len + 1` of them  plus every detection already queued from an earlier call, which is `q_cap`.




**Parameters:**


* `state` Must be non-NULL. 
* `x_len` Number of input samples the caller is about to push. 



**Returns:**

`(x_len/burst_len + 1 + q_cap) * frame_syms`. 





        

<hr>



### function dsss\_burst\_receiver\_reset 

_Return to searching: drop the history and clear every read-back._ 
```C++
void dsss_burst_receiver_reset (
    dsss_burst_receiver_state_t * state
) 
```



Resets the embedded acquisition, discards the retained look-back, and clears all the event fields, so a fresh stream cannot inherit the previous burst's verdict. The lifetime counters (`n_bursts`, `dropped`) deliberately survive  a reset that zeroed them could hide that this receiver had already lost samples. Construction parameters are untouched.




**Parameters:**


* `state` Must be non-NULL. 
```C++
>>> import numpy as np
>>> from doppler.dsss import DsssBurstReceiver
>>> rng = np.random.default_rng(0)
>>> rx = DsssBurstReceiver(
...     rng.integers(0, 2, 31).astype(np.uint8),
...     rng.integers(0, 2, 8).astype(np.uint8),
...     np.zeros(13, dtype=np.uint8), reps=4, spc=4, frame_syms=32)
>>> _ = rx.push(np.zeros(1024, dtype=np.complex64))
>>> rx.reset()
```
 




        

<hr>



### function dsss\_burst\_receiver\_set\_state 

_Restore cross-call state from_ `blob` _(replacing it)._
```C++
int dsss_burst_receiver_set_state (
    dsss_burst_receiver_state_t * state,
    const void * blob
) 
```





**Returns:**

DP\_OK, or DP\_ERR\_INVALID if the envelope or any child rejects. 





        

<hr>



### function dsss\_burst\_receiver\_state\_bytes 

_Byte size of_ `state's` _blob (envelope + payload + child)._
```C++
size_t dsss_burst_receiver_state_bytes (
    const dsss_burst_receiver_state_t * state
) 
```




<hr>
## Macro Definition Documentation





### define DSSS\_BURST\_RECEIVER\_STATE\_MAGIC 

_Per-object envelope tag: "DBRX" (DsssBurstReceiver)._ 
```C++
#define DSSS_BURST_RECEIVER_STATE_MAGIC `DP_FOURCC ('D', 'B', 'R', 'X')`
```




<hr>



### define DSSS\_BURST\_RECEIVER\_STATE\_VERSION 

```C++
#define DSSS_BURST_RECEIVER_STATE_VERSION `5u`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/dsss_burst_receiver/dsss_burst_receiver_core.h`

