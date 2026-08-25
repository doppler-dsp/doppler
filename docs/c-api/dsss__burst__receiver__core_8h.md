

# File dsss\_burst\_receiver\_core.h



[**FileList**](files.md) **>** [**dsss\_burst\_receiver**](dir_32a143d35207eb7d99f4a541895f77eb.md) **>** [**dsss\_burst\_receiver\_core.h**](dsss__burst__receiver__core_8h.md)

[Go to the source code of this file](dsss__burst__receiver__core_8h_source.md)

_DsssBurstReceiver component API._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "jm_perf.h"`
* `#include "buffer/buffer.h"`
* `#include "dp_state.h"`
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















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**dsss\_br\_pending\_t**](structdsss__br__pending__t.md) <br>_One detection between acquisition and demodulation._  |
| struct | [**dsss\_burst\_receiver\_state\_t**](structdsss__burst__receiver__state__t.md) <br>_DsssBurstReceiver state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  int | [**dsss\_burst\_receiver\_configure\_search\_raw**](#function-dsss_burst_receiver_configure_search_raw) ([**dsss\_burst\_receiver\_state\_t**](structdsss__burst__receiver__state__t.md) \* state, size\_t doppler\_bins, size\_t n\_noncoh) <br>_Pin the acquisition search grid, bypassing the auto-sizing._  |
|  [**dsss\_burst\_receiver\_state\_t**](structdsss__burst__receiver__state__t.md) \* | [**dsss\_burst\_receiver\_create**](#function-dsss_burst_receiver_create) (const uint8\_t \* acq\_code, size\_t acq\_code\_len, const uint8\_t \* data\_code, size\_t data\_code\_len, const uint8\_t \* sync, size\_t sync\_len, size\_t reps, size\_t spc, double chip\_rate, size\_t payload\_len, double cn0\_dbhz, double doppler\_uncertainty, double pfa, double pd, double carrier\_hz, double max\_rate, size\_t est\_segments) <br>_Create a burst receiver: acquisition, refine and demodulation composed behind one push()._  |
|  void | [**dsss\_burst\_receiver\_destroy**](#function-dsss_burst_receiver_destroy) ([**dsss\_burst\_receiver\_state\_t**](structdsss__burst__receiver__state__t.md) \* state) <br>_Destroy a dsss\_burst\_receiver instance and release all memory._  |
|  double | [**dsss\_burst\_receiver\_get\_cn0\_dbhz\_est**](#function-dsss_burst_receiver_get_cn0_dbhz_est) (const [**dsss\_burst\_receiver\_state\_t**](structdsss__burst__receiver__state__t.md) \* state) <br> |
|  double | [**dsss\_burst\_receiver\_get\_doppler\_hz\_est**](#function-dsss_burst_receiver_get_doppler_hz_est) (const [**dsss\_burst\_receiver\_state\_t**](structdsss__burst__receiver__state__t.md) \* state) <br> |
|  double | [**dsss\_burst\_receiver\_get\_doppler\_res\_hz**](#function-dsss_burst_receiver_get_doppler_res_hz) (const [**dsss\_burst\_receiver\_state\_t**](structdsss__burst__receiver__state__t.md) \* state) <br> |
|  uint64\_t | [**dsss\_burst\_receiver\_get\_dropped**](#function-dsss_burst_receiver_get_dropped) (const [**dsss\_burst\_receiver\_state\_t**](structdsss__burst__receiver__state__t.md) \* state) <br> |
|  double | [**dsss\_burst\_receiver\_get\_est\_freq\_hz**](#function-dsss_burst_receiver_get_est_freq_hz) (const [**dsss\_burst\_receiver\_state\_t**](structdsss__burst__receiver__state__t.md) \* state) <br> |
|  double | [**dsss\_burst\_receiver\_get\_est\_rate\_hz**](#function-dsss_burst_receiver_get_est_rate_hz) (const [**dsss\_burst\_receiver\_state\_t**](structdsss__burst__receiver__state__t.md) \* state) <br> |
|  double | [**dsss\_burst\_receiver\_get\_est\_snr\_db**](#function-dsss_burst_receiver_get_est_snr_db) (const [**dsss\_burst\_receiver\_state\_t**](structdsss__burst__receiver__state__t.md) \* state) <br> |
|  int | [**dsss\_burst\_receiver\_get\_frame\_valid**](#function-dsss_burst_receiver_get_frame_valid) (const [**dsss\_burst\_receiver\_state\_t**](structdsss__burst__receiver__state__t.md) \* state) <br> |
|  uint64\_t | [**dsss\_burst\_receiver\_get\_n\_bursts**](#function-dsss_burst_receiver_get_n_bursts) (const [**dsss\_burst\_receiver\_state\_t**](structdsss__burst__receiver__state__t.md) \* state) <br> |
|  size\_t | [**dsss\_burst\_receiver\_get\_pending**](#function-dsss_burst_receiver_get_pending) (const [**dsss\_burst\_receiver\_state\_t**](structdsss__burst__receiver__state__t.md) \* state) <br> |
|  uint64\_t | [**dsss\_burst\_receiver\_get\_preamble\_start**](#function-dsss_burst_receiver_get_preamble_start) (const [**dsss\_burst\_receiver\_state\_t**](structdsss__burst__receiver__state__t.md) \* state) <br> |
|  double | [**dsss\_burst\_receiver\_get\_refine\_margin**](#function-dsss_burst_receiver_get_refine_margin) (const [**dsss\_burst\_receiver\_state\_t**](structdsss__burst__receiver__state__t.md) \* state) <br> |
|  void | [**dsss\_burst\_receiver\_get\_state**](#function-dsss_burst_receiver_get_state) (const [**dsss\_burst\_receiver\_state\_t**](structdsss__burst__receiver__state__t.md) \* state, void \* blob) <br>_Serialize_ `state's` _cross-call state into_`blob` _(caller-owned,_[_**dsss\_burst\_receiver\_state\_bytes()**_](dsss__burst__receiver__core_8h.md#function-dsss_burst_receiver_state_bytes) _long)._ |
|  size\_t | [**dsss\_burst\_receiver\_push**](#function-dsss_burst_receiver_push) ([**dsss\_burst\_receiver\_state\_t**](structdsss__burst__receiver__state__t.md) \* state, const float complex \* x, size\_t x\_len, uint8\_t \* out, size\_t max\_out) <br>_Stream samples; emit one burst's payload when one completes._  |
|  size\_t | [**dsss\_burst\_receiver\_push\_max\_out**](#function-dsss_burst_receiver_push_max_out) ([**dsss\_burst\_receiver\_state\_t**](structdsss__burst__receiver__state__t.md) \* state, size\_t x\_len) <br>_Max bits push() writes: the payload length._  |
|  void | [**dsss\_burst\_receiver\_reset**](#function-dsss_burst_receiver_reset) ([**dsss\_burst\_receiver\_state\_t**](structdsss__burst__receiver__state__t.md) \* state) <br>_Return to searching: drop the history and clear every read-back._  |
|  int | [**dsss\_burst\_receiver\_set\_state**](#function-dsss_burst_receiver_set_state) ([**dsss\_burst\_receiver\_state\_t**](structdsss__burst__receiver__state__t.md) \* state, const void \* blob) <br>_Restore cross-call state from_ `blob` _(replacing it)._ |
|  size\_t | [**dsss\_burst\_receiver\_state\_bytes**](#function-dsss_burst_receiver_state_bytes) (const [**dsss\_burst\_receiver\_state\_t**](structdsss__burst__receiver__state__t.md) \* state) <br>_Byte size of_ `state's` _blob (envelope + payload + child)._ |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**DSSS\_BR\_QCAP**](dsss__burst__receiver__core_8h.md#define-dsss_br_qcap)  `8u`<br>_Detections that may be in flight at once._  |
| define  | [**DSSS\_BURST\_RECEIVER\_STATE\_MAGIC**](dsss__burst__receiver__core_8h.md#define-dsss_burst_receiver_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc)('D', 'B', 'R', 'X')`<br>_Per-object envelope tag: "DBRX" (DsssBurstReceiver)._  |
| define  | [**DSSS\_BURST\_RECEIVER\_STATE\_VERSION**](dsss__burst__receiver__core_8h.md#define-dsss_burst_receiver_state_version)  `1u`<br> |

## Detailed Description


Lifecycle: create -&gt; [step / steps / reset]\* -&gt; destroy


Example: 
```C++
dsss_burst_receiver_state_t *obj = dsss_burst_receiver_create(NULL, 0, NULL, 0, NULL, 0, 5, 4, 1000000.0, 64, 50.0, 0.0, 1e-3, 0.9, 0.0, 0.0, 10);
uint8_t y = dsss_burst_receiver_step(obj, 0.0f + 0.0f * I);
dsss_burst_receiver_destroy(obj);
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
* `doppler_bins` Coherent depth to pin, in [1, reps]. 
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
...     np.zeros(13, dtype=np.uint8), reps=4, spc=4, payload_len=32)
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
    size_t payload_len,
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
* `payload_len` Payload bits per burst (&gt;= 1). 
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
...                        payload_len=32)
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



### function dsss\_burst\_receiver\_get\_frame\_valid 

```C++
int dsss_burst_receiver_get_frame_valid (
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



### function dsss\_burst\_receiver\_get\_state 

_Serialize_ `state's` _cross-call state into_`blob` _(caller-owned,_[_**dsss\_burst\_receiver\_state\_bytes()**_](dsss__burst__receiver__core_8h.md#function-dsss_burst_receiver_state_bytes) _long)._
```C++
void dsss_burst_receiver_get_state (
    const dsss_burst_receiver_state_t * state,
    void * blob
) 
```




<hr>



### function dsss\_burst\_receiver\_push 

_Stream samples; emit one burst's payload when one completes._ 
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


Writes the payload of AT MOST ONE completed burst per call, so the read-back fields always describe the burst whose bits were just returned. If more are ready, `pending` is non-zero and a further push drains the next. Returning 0 is normal, not an error: it means no burst completed.




**Parameters:**


* `state` Must be non-NULL. 
* `x` Input samples (cf32), `x_len` long. 
* `x_len` Number of input samples. 
* `out` Payload bits, caller-owned, `max_out` long. 
* `max_out` Capacity of `out`; see push\_max\_out(). 



**Returns:**

Bits written to `out` (0 or payload\_len). 
```C++
>>> import numpy as np
>>> from doppler.dsss import DsssBurstReceiver
>>> rng = np.random.default_rng(0)
>>> rx = DsssBurstReceiver(
...     rng.integers(0, 2, 31).astype(np.uint8),
...     rng.integers(0, 2, 8).astype(np.uint8),
...     np.zeros(13, dtype=np.uint8), reps=4, spc=4, payload_len=32)
>>> bits = rx.push(np.zeros(4096, dtype=np.complex64))
>>> bits.size            # silence carries no burst
0
```
 





        

<hr>



### function dsss\_burst\_receiver\_push\_max\_out 

_Max bits push() writes: the payload length._ 
```C++
size_t dsss_burst_receiver_push_max_out (
    dsss_burst_receiver_state_t * state,
    size_t x_len
) 
```



Independent of `x_len`, because at most one burst is returned per call.




**Parameters:**


* `state` Must be non-NULL. 
* `x_len` Input length (ignored). 



**Returns:**

payload\_len  the buffer a caller must provide. 





        

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
...     np.zeros(13, dtype=np.uint8), reps=4, spc=4, payload_len=32)
>>> _ = rx.push(np.zeros(1024, dtype=np.complex64))
>>> rx.reset()
>>> rx.frame_valid
0
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





### define DSSS\_BR\_QCAP 

_Detections that may be in flight at once._ 
```C++
#define DSSS_BR_QCAP `8u`
```




<hr>



### define DSSS\_BURST\_RECEIVER\_STATE\_MAGIC 

_Per-object envelope tag: "DBRX" (DsssBurstReceiver)._ 
```C++
#define DSSS_BURST_RECEIVER_STATE_MAGIC `DP_FOURCC ('D', 'B', 'R', 'X')`
```




<hr>



### define DSSS\_BURST\_RECEIVER\_STATE\_VERSION 

```C++
#define DSSS_BURST_RECEIVER_STATE_VERSION `1u`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/dsss_burst_receiver/dsss_burst_receiver_core.h`

