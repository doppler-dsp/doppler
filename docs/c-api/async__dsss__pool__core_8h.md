

# File async\_dsss\_pool\_core.h



[**FileList**](files.md) **>** [**async\_dsss\_pool**](dir_24b3a88e64d64c92fdc3841f769b80af.md) **>** [**async\_dsss\_pool\_core.h**](async__dsss__pool__core_8h.md)

[Go to the source code of this file](async__dsss__pool__core_8h_source.md)

_AsyncDsssPool_  _one object holds the population: a searcher, a pool of hand-off receivers, the assigned table and the event log (docs/design/async-dsss-receiver.md section 8.2)._[More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "jm_perf.h"`
* `#include "async_dsss_receiver/async_dsss_receiver_core.h"`
* `#include "acq/acq_core.h"`
* `#include "dp_event_log/dp_event_log_core.h"`
* `#include "dll/dll_core.h"`
* `#include "costas/costas_core.h"`
* `#include "RateConverter/RateConverter_core.h"`
* `#include "mpsk_receiver/mpsk_receiver_core.h"`
* `#include "cic/cic_core.h"`
* `#include "resample/resample_core.h"`
* `#include "psd/psd_core.h"`
* `#include "detector/detector_core.h"`
* `#include "detection/detection_core.h"`
* `#include "corr2d/corr2d_core.h"`
* `#include "fft2d/fft2d_core.h"`
* `#include "fft/fft_core.h"`
* `#include "dp_tlm/dp_tlm_core.h"`
* `#include "carrier_acq/carrier_acq_core.h"`
* `#include "resamp/resamp_core.h"`
* `#include "hbdecim/hbdecim_core.h"`
* `#include "dp_parallel.h"`
* `#include "dp_state.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**async\_dsss\_pool\_row\_t**](structasync__dsss__pool__row__t.md) <br> |
| struct | [**async\_dsss\_pool\_slot\_t**](structasync__dsss__pool__slot__t.md) <br>_One slot's picture, by value_  _what_`status()` _returns._ |
| struct | [**async\_dsss\_pool\_state\_t**](structasync__dsss__pool__state__t.md) <br>_AsyncDsssPool state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**async\_dsss\_pool\_state\_t**](structasync__dsss__pool__state__t.md) \* | [**async\_dsss\_pool\_create**](#function-async_dsss_pool_create) (const uint8\_t \* code, size\_t code\_len, double chip\_rate, double symbol\_rate, size\_t spc, int m, double cn0\_dbhz, double pfa, double pd, double doppler\_uncertainty, size\_t code\_only\_epochs, double doppler\_rate, size\_t max\_peaks, size\_t n\_slots, int threads, double carrier\_freq\_hz, double lost\_confirm\_s, double max\_emitter\_on\_time\_secs, size\_t segments, size\_t sps, int differential, double refine\_max\_error\_db, size\_t refine\_samples\_per\_symbol, double refine\_design\_margin\_db, size\_t refine\_n\_fft, size\_t refine\_zero\_pad, bool refine\_sequential, size\_t refine\_max\_n\_blocks) <br>_Create a async\_dsss\_pool instance._  |
|  void | [**async\_dsss\_pool\_destroy**](#function-async_dsss_pool_destroy) ([**async\_dsss\_pool\_state\_t**](structasync__dsss__pool__state__t.md) \* state) <br>_Destroy a async\_dsss\_pool instance and release all memory._  |
|  void | [**async\_dsss\_pool\_get\_state**](#function-async_dsss_pool_get_state) (const [**async\_dsss\_pool\_state\_t**](structasync__dsss__pool__state__t.md) \* state, void \* blob) <br> |
|  size\_t | [**async\_dsss\_pool\_push**](#function-async_dsss_pool_push) ([**async\_dsss\_pool\_state\_t**](structasync__dsss__pool__state__t.md) \* state, const float \_Complex \* x, size\_t x\_len) <br>_One block of raw cf32 samples through the population._  |
|  void | [**async\_dsss\_pool\_reset**](#function-async_dsss_pool_reset) ([**async\_dsss\_pool\_state\_t**](structasync__dsss__pool__state__t.md) \* state) <br>_Release every slot and start over: the searcher reset, every receiver back to idle, the table cleared, the counters zeroed._  |
|  int | [**async\_dsss\_pool\_set\_event\_log**](#function-async_dsss_pool_set_event_log) ([**async\_dsss\_pool\_state\_t**](structasync__dsss__pool__state__t.md) \* state, [**dp\_event\_log\_t**](dp__event__log__core_8h.md#typedef-dp_event_log_t) \* log) <br>_Attach the run's event log (design section 8.1); NULL detaches._  |
|  int | [**async\_dsss\_pool\_set\_state**](#function-async_dsss_pool_set_state) ([**async\_dsss\_pool\_state\_t**](structasync__dsss__pool__state__t.md) \* state, const void \* blob) <br> |
|  size\_t | [**async\_dsss\_pool\_state\_bytes**](#function-async_dsss_pool_state_bytes) (const [**async\_dsss\_pool\_state\_t**](structasync__dsss__pool__state__t.md) \* state) <br> |
|  [**async\_dsss\_pool\_slot\_t**](structasync__dsss__pool__slot__t.md) | [**async\_dsss\_pool\_status**](#function-async_dsss_pool_status) ([**async\_dsss\_pool\_state\_t**](structasync__dsss__pool__state__t.md) \* state, size\_t slot) <br>_One slot's picture, by value (_ [_**async\_dsss\_pool\_slot\_t**_](structasync__dsss__pool__slot__t.md) _)._ |
|  size\_t | [**async\_dsss\_pool\_symbols**](#function-async_dsss_pool_symbols) ([**async\_dsss\_pool\_state\_t**](structasync__dsss__pool__state__t.md) \* state, size\_t slot, float \_Complex \* out, size\_t max\_out) <br>_The symbols slot_ `slot's` _receiver decided on the last push()._ |
|  size\_t | [**async\_dsss\_pool\_symbols\_max\_out**](#function-async_dsss_pool_symbols_max_out) ([**async\_dsss\_pool\_state\_t**](structasync__dsss__pool__state__t.md) \* state) <br>_The per-slot symbol capacity_ `symbols()` _can return_ _grown with the largest block pushed so far (0 before the first push)._ |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**ASYNC\_DSSS\_POOL\_MAX\_EMITTER\_ON\_TIME\_SECS**](async__dsss__pool__core_8h.md#define-async_dsss_pool_max_emitter_on_time_secs)  `(15.0 \* 60.0)`<br> |
| define  | [**ASYNC\_DSSS\_POOL\_STATE\_MAGIC**](async__dsss__pool__core_8h.md#define-async_dsss_pool_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('A', 'D', 'P', 'L')`<br> |
| define  | [**ASYNC\_DSSS\_POOL\_STATE\_VERSION**](async__dsss__pool__core_8h.md#define-async_dsss_pool_state_version)  `1u`<br> |

## Detailed Description


The multi-emitter use case (design section 6) has one channel searching without pause and one AsyncDsssReceiver per emitter, assigned once from a detection and tracking until its own loss decision. This object is the holder of that lifecycle, in C, with the Python face as glue:



* **one searcher**, `Acquisition` in continuous mode with the block coherence its code-only window buys (section 2.3) and a peak list (section 7.1), its tiles fanned across the threads the pool is given;
* \*\*`n_slots` hand-off receivers\*\*, created idle. An idle or lost receiver consumes and discards what it is fed, so every receiver is fed every block and the feed has no per-state branch; they run across the same threads (`dp_parallel.h`);
* **the assigned table**, one row per slot: the seed's coordinates, and the row's CURRENT Doppler and chip phase  the live loop's once the receiver tracks, the seed's advanced by the clock dilation before  refreshed before every dwell is read, because an emitter drifts between windows and the seed is the wrong key (section 9);
* **the event log**, borrowed by attachment (the telemetry shape): the pool is the one component that stamps.




One `push()` per block does, in order: feed the searcher; refresh the table; drop every peak inside one exclusion zone  one Doppler row by one chip, section 7.1  of a live row, as that emitter's own; for each survivor, `acq_build_handoff()` and `seed()` into a free slot, or count it dropped when there is none; feed every receiver; then, for each slot whose receiver reports lost, or has held its slot past the maximum on-air time, clear the row, `reset()` the receiver to idle and log `released`. `seed()`'s own refusal on a receiver that is not idle is the second guard behind the table, so a bookkeeping error cannot become a double assignment. The transitions  `seeded`, `tracking`, `degrade`, `lost`, `released`, `dropped`  are the log's annotations, at the sample they happened, with the slot, the receiver's state, the Doppler, the chip phase and the C/N0 staged as `doppler:<name>` fields beside the label (`core:label`).


Nothing about the waveform or the population is baked in: every number is a create parameter whose default is the operating point of section 6.1 (twelve slots, a peak list of sixteen, a 2 s release interval, a 15 min maximum on-air time), and the searcher's and the receivers' own parameters pass through untouched. A physically-coupled carrier (`carrier_freq_hz` &gt; 0) is told to the searcher as well as the receivers: its hand-off advances a hit's code phase by the drift over half its dwell and its coherent blocks align their epochs (#1254, #1256), and the table's rows are advanced by the same dilation.


What comes out, per slot and by index: the status record by value (`status()`), and the symbols the receiver decided on this push, borrowed from the pool's own buffer (`symbols()`). Nothing allocates per push once a block size has been seen, the pool never exceeds `n_slots`, and a released emitter still on the air is a new detection at its next window into whichever slot is free  the one re-assignment the lifecycle permits. Replay and live runs produce the same records, because nothing here sees a time: every stamp is a stream position.


Not built: section 11.4's replica. The operating spread of 10 dB picks the list branch (section 9), so the pool is off the searcher's push path and subtracts nothing.


Lifecycle: create, then push / status / symbols / reset as often as wanted, then destroy.



```C++
>>> import numpy as np
>>> from doppler.dsss import AsyncDsssPool
>>> from doppler.wfm import Gold
>>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
>>> pool = AsyncDsssPool(code, chip_rate=5e6, symbol_rate=2700.0,
...                      spc=2, cn0_dbhz=45.0, doppler_uncertainty=5e3,
...                      n_slots=4, lost_confirm_s=0.5)
>>> (pool.n_slots, pool.n_assigned, pool.status(0).assigned)
(4, 0, 0)
>>> int(pool.push(np.zeros(2046, np.complex64)))   # noise-free silence
0
```
 


    
## Public Functions Documentation




### function async\_dsss\_pool\_create 

_Create a async\_dsss\_pool instance._ 
```C++
async_dsss_pool_state_t * async_dsss_pool_create (
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
    size_t code_only_epochs,
    double doppler_rate,
    size_t max_peaks,
    size_t n_slots,
    int threads,
    double carrier_freq_hz,
    double lost_confirm_s,
    double max_emitter_on_time_secs,
    size_t segments,
    size_t sps,
    int differential,
    double refine_max_error_db,
    size_t refine_samples_per_symbol,
    double refine_design_margin_db,
    size_t refine_n_fft,
    size_t refine_zero_pad,
    bool refine_sequential,
    size_t refine_max_n_blocks
) 
```



Everything is sized once, here: the searcher with its list and its threads, `n_slots` idle receivers, the table. NULL for a NULL or empty code, a non-positive rate, `spc` or `n_slots` of 0, `max_peaks` outside the searcher's own range, a negative `lost_confirm_s` or `max_emitter_on_time_secs`, or a child that fails to open.




**Parameters:**


* `code` Spreading code, one 0/1 chip per element. 
* `code_len` Chips in `code`. 
* `chip_rate` Chip rate, Hz (default: 1000000.0). 
* `symbol_rate` Data-symbol rate, Hz (default: 1000.0). 
* `spc` Samples per chip (default: 2). 
* `m` PSK order of the receivers (default: 2). 
* `cn0_dbhz` Design C/N0 for the searcher's sizing and the receivers' (default: 55.0). 
* `pfa` False-alarm target, the searcher's and the refine's (default: 1e-3). 
* `pd` Detection-probability target (default: 0.9). 
* `doppler_uncertainty` The searcher's one-sided span, Hz (default: 100.0). 
* `code_only_epochs` Whole code-only epochs the waveform's window holds at any chip phase  the block depth of section 2.3; 1 = no window (default: 1). 
* `doppler_rate` Doppler rate the depth is bounded against, Hz/s; 0 leaves the window as the only bound (default: 0.0). 
* `max_peaks` The searcher's list capacity per dwell (default: 16). 
* `n_slots` Receivers held (default: 12). 
* `threads` Threads the receivers and the searcher's fan run across; &lt;= 0 picks the online core count, 1 is serial (default: 1). 
* `carrier_freq_hz` RF carrier the Doppler is physically coupled to, Hz, told to the searcher and every receiver; 0.0 = uncoupled (default: 0.0). 
* `lost_confirm_s` The release rule's interval, seconds (section 10) (default: 2.0). 
* `max_emitter_on_time_secs` Maximum on-air time of one emitter, seconds: a slot held longer is released (`reason` on\_time); 0 = never (default: 900.0, ASYNC\_DSSS\_POOL\_MAX\_EMITTER\_ON\_ TIME\_SECS). 
* `segments` The receivers' live Dll segments (default: 4). 
* `sps` The receivers' samples per symbol (default: 8). 
* `differential` The receivers' differential demap (default: 0). 
* `refine_max_error_db` As [**async\_dsss\_receiver\_create()**](async__dsss__receiver__core_8h.md#function-async_dsss_receiver_create) (default: 0.5). 
* `refine_samples_per_symbol` As [**async\_dsss\_receiver\_create()**](async__dsss__receiver__core_8h.md#function-async_dsss_receiver_create) (default: 4). 
* `refine_design_margin_db` As [**async\_dsss\_receiver\_create()**](async__dsss__receiver__core_8h.md#function-async_dsss_receiver_create) (default: 14.0). 
* `refine_n_fft` As [**async\_dsss\_receiver\_create()**](async__dsss__receiver__core_8h.md#function-async_dsss_receiver_create) (default: 64). 
* `refine_zero_pad` As [**async\_dsss\_receiver\_create()**](async__dsss__receiver__core_8h.md#function-async_dsss_receiver_create) (default: 8). 
* `refine_sequential` As [**async\_dsss\_receiver\_create()**](async__dsss__receiver__core_8h.md#function-async_dsss_receiver_create) (default: false). 
* `refine_max_n_blocks` As [**async\_dsss\_receiver\_create()**](async__dsss__receiver__core_8h.md#function-async_dsss_receiver_create) (default: 100000). 



**Returns:**

Heap-allocated state, or NULL on allocation failure. 




**Note:**

Caller must call [**async\_dsss\_pool\_destroy()**](async__dsss__pool__core_8h.md#function-async_dsss_pool_destroy) when done. 
```C++
>>> import numpy as np
>>> from doppler.dsss import AsyncDsssPool
>>> from doppler.wfm import Gold
>>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
>>> pool = AsyncDsssPool(code, chip_rate=5e6, symbol_rate=2700.0,
...                      spc=2, cn0_dbhz=45.0, doppler_uncertainty=5e3,
...                      n_slots=4, lost_confirm_s=0.5)
>>> (pool.n_slots, pool.n_assigned, pool.coherent_bins)
(4, 0, 1)
>>> round(pool.doppler_res_hz)          # one Doppler row of the searcher
4888
```
 





        

<hr>



### function async\_dsss\_pool\_destroy 

_Destroy a async\_dsss\_pool instance and release all memory._ 
```C++
void async_dsss_pool_destroy (
    async_dsss_pool_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function async\_dsss\_pool\_get\_state 

```C++
void async_dsss_pool_get_state (
    const async_dsss_pool_state_t * state,
    void * blob
) 
```




<hr>



### function async\_dsss\_pool\_push 

_One block of raw cf32 samples through the population._ 
```C++
size_t async_dsss_pool_push (
    async_dsss_pool_state_t * state,
    const float _Complex * x,
    size_t x_len
) 
```



In order: the searcher; the table refreshed; every peak inside one exclusion zone of a live row dropped as that emitter's own; each survivor seeded into a free slot or counted dropped; every receiver fed, across the pool's threads; every receiver that reports lost, or has held its slot past the maximum on-air time, released. Every transition goes to the attached log at the sample it happened. Accepts any block size: a hit decided inside the block is referred to the block's start before it seeds (the receiver is fed the whole block), on the dilated clock when the carrier is known.




**Parameters:**


* `state` Must be non-NULL. 
* `x` Input samples. 
* `x_len` Samples in `x`. 



**Returns:**

Receivers assigned after this push. 
```C++
>>> import numpy as np
>>> from doppler.dsss import AsyncDsssPool
>>> from doppler.wfm import Gold
>>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
>>> pool = AsyncDsssPool(code, chip_rate=5e6, symbol_rate=2700.0,
...                      spc=2, cn0_dbhz=45.0, n_slots=2)
>>> int(pool.push(np.zeros(4 * 2046, np.complex64)))  # silence: no one
0
>>> pool.samples_consumed                  # the stream position
8184
```
 





        

<hr>



### function async\_dsss\_pool\_reset 

_Release every slot and start over: the searcher reset, every receiver back to idle, the table cleared, the counters zeroed._ 
```C++
void async_dsss_pool_reset (
    async_dsss_pool_state_t * state
) 
```



The attached log stays attached and nothing is logged  a reset is the holder's decision, not an emitter's transition.




**Parameters:**


* `state` Must be non-NULL. 
```C++
>>> import numpy as np
>>> from doppler.dsss import AsyncDsssPool
>>> from doppler.wfm import Gold
>>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
>>> pool = AsyncDsssPool(code, chip_rate=5e6, symbol_rate=2700.0,
...                      spc=2, cn0_dbhz=45.0, n_slots=2)
>>> _ = pool.push(np.zeros(2046, np.complex64))
>>> pool.samples_consumed
2046
>>> pool.reset()
>>> (pool.samples_consumed, pool.n_assigned, pool.events)
(0, 0, 0)
```
 




        

<hr>



### function async\_dsss\_pool\_set\_event\_log 

_Attach the run's event log (design section 8.1); NULL detaches._ 
```C++
int async_dsss_pool_set_event_log (
    async_dsss_pool_state_t * state,
    dp_event_log_t * log
) 
```



Borrowed, never owned: the holder opens, finalizes and closes it. From now on every transition is appended at the sample it happened, with `slot`, `state`, `doppler_hz`, `chip_phase` and `cn0_dbhz` staged as `doppler:<name>` fields beside the label (`core:label`) and, on `released`, `reason` (`lost` or `on_time`). A log that has already failed keeps failing (its error is sticky); the pool counts the transition either way.




**Parameters:**


* `state` Must be non-NULL. 
* `log` The log, or NULL. 



**Returns:**

`DP_OK`. 
```C++
>>> import os, tempfile
>>> import numpy as np
>>> from doppler.dsss import AsyncDsssPool
>>> from doppler.telemetry import EventLog
>>> from doppler.wfm import Gold
>>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
>>> pool = AsyncDsssPool(code, chip_rate=5e6, symbol_rate=2700.0,
...                      spc=2, cn0_dbhz=45.0, n_slots=2)
>>> log = EventLog(os.path.join(tempfile.mkdtemp(), "run.events"))
>>> pool.set_event_log(log)               # attached: transitions go here
>>> _ = pool.push(np.zeros(2046, np.complex64))
>>> pool.set_event_log(None)              # detached
>>> log.close()
```
 





        

<hr>



### function async\_dsss\_pool\_set\_state 

```C++
int async_dsss_pool_set_state (
    async_dsss_pool_state_t * state,
    const void * blob
) 
```




<hr>



### function async\_dsss\_pool\_state\_bytes 

```C++
size_t async_dsss_pool_state_bytes (
    const async_dsss_pool_state_t * state
) 
```




<hr>



### function async\_dsss\_pool\_status 

_One slot's picture, by value (_ [_**async\_dsss\_pool\_slot\_t**_](structasync__dsss__pool__slot__t.md) _)._
```C++
async_dsss_pool_slot_t async_dsss_pool_status (
    async_dsss_pool_state_t * state,
    size_t slot
) 
```



Allocation-free: the row plus the receiver's own status record. A slot outside `[0, n_slots)` returns a zero record with `state` -1.




**Parameters:**


* `state` Must be non-NULL. 
* `slot` The slot. 



**Returns:**

The record. 
```C++
>>> import numpy as np
>>> from doppler.dsss import AsyncDsssPool
>>> from doppler.wfm import Gold
>>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
>>> pool = AsyncDsssPool(code, chip_rate=5e6, symbol_rate=2700.0,
...                      spc=2, cn0_dbhz=45.0, n_slots=2)
>>> r = pool.status(1)
>>> (r.slot, r.assigned, r.state)         # idle: 3, nothing assigned
(1, 0, 3)
>>> pool.status(2).state                  # no such slot
-1
```
 





        

<hr>



### function async\_dsss\_pool\_symbols 

_The symbols slot_ `slot's` _receiver decided on the last push()._
```C++
size_t async_dsss_pool_symbols (
    async_dsss_pool_state_t * state,
    size_t slot,
    float _Complex * out,
    size_t max_out
) 
```



Copied from the pool's own buffer, which the next push() overwrites. Empty while the slot is idle, refining or lost, and for a slot outside `[0, n_slots)`.




**Parameters:**


* `state` Must be non-NULL. 
* `slot` The slot. 
* `out` Caller buffer. 
* `max_out` Its capacity. 



**Returns:**

Symbols written. 
```C++
>>> import numpy as np
>>> from doppler.dsss import AsyncDsssPool
>>> from doppler.wfm import Gold
>>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
>>> pool = AsyncDsssPool(code, chip_rate=5e6, symbol_rate=2700.0,
...                      spc=2, cn0_dbhz=45.0, n_slots=2)
>>> _ = pool.push(np.zeros(2046, np.complex64))
>>> pool.symbols(0).shape                 # idle: nothing decided
(0,)
```
 





        

<hr>



### function async\_dsss\_pool\_symbols\_max\_out 

_The per-slot symbol capacity_ `symbols()` _can return_ _grown with the largest block pushed so far (0 before the first push)._
```C++
size_t async_dsss_pool_symbols_max_out (
    async_dsss_pool_state_t * state
) 
```




<hr>
## Macro Definition Documentation





### define ASYNC\_DSSS\_POOL\_MAX\_EMITTER\_ON\_TIME\_SECS 

```C++
#define ASYNC_DSSS_POOL_MAX_EMITTER_ON_TIME_SECS `(15.0 * 60.0)`
```



The one constant of design section 6.1: the maximum on-air time of a single emitter, seconds  the default of `max_emitter_on_time_secs`, which the false-release budget and the soak are sized against. 


        

<hr>



### define ASYNC\_DSSS\_POOL\_STATE\_MAGIC 

```C++
#define ASYNC_DSSS_POOL_STATE_MAGIC `DP_FOURCC ('A', 'D', 'P', 'L')`
```




<hr>



### define ASYNC\_DSSS\_POOL\_STATE\_VERSION 

```C++
#define ASYNC_DSSS_POOL_STATE_VERSION `1u`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/async_dsss_pool/async_dsss_pool_core.h`

