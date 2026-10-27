

# File async\_dsss\_pool\_core.h



[**FileList**](files.md) **>** [**async\_dsss\_pool**](dir_8a6668f3097fb7847a23b1f65b3df26f.md) **>** [**async\_dsss\_pool\_core.h**](async__dsss__pool__core_8h.md)

[Go to the source code of this file](async__dsss__pool__core_8h_source.md)

_AsyncDsssPool_  _N AsyncDsssReceivers on one stream under a controller (docs/design/async-dsss-parallel.md §4)._[More...](#detailed-description)

* `#include "doppler/clib_common.h"`
* `#include "doppler/jm_perf.h"`
* `#include "doppler/dp_parallel.h"`
* `#include "doppler/dp_state.h"`
* `#include "doppler/dp_tlm/dp_tlm_core.h"`
* `#include "doppler/async_dsss_receiver/async_dsss_receiver_core.h"`
* `#include "doppler/ber/ber_core.h"`
* `#include "doppler/dll/dll_core.h"`
* `#include "doppler/costas/costas_core.h"`
* `#include "doppler/RateConverter/RateConverter_core.h"`
* `#include "doppler/mpsk_receiver/mpsk_receiver_core.h"`
* `#include "doppler/cic/cic_core.h"`
* `#include "doppler/resample/resample_core.h"`
* `#include "doppler/psd/psd_core.h"`
* `#include "doppler/detector/detector_core.h"`
* `#include "doppler/detection/detection_core.h"`
* `#include "doppler/spectral/spectral_core.h"`
* `#include "doppler/doppler_channel/doppler_channel_core.h"`
* `#include "doppler/corr/corr_core.h"`
* `#include "doppler/fft/fft_core.h"`
* `#include "doppler/acc_trace/acc_trace_core.h"`
* `#include "doppler/acq/acq_core.h"`
* `#include "doppler/carrier_acq/carrier_acq_core.h"`
* `#include "doppler/resamp/resamp_core.h"`
* `#include "doppler/hbdecim/hbdecim_core.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**dp\_async\_dsss\_pool\_state\_t**](structdp__async__dsss__pool__state__t.md) <br>_AsyncDsssPool state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**dp\_async\_dsss\_pool\_state\_t**](structdp__async__dsss__pool__state__t.md) \* | [**dp\_async\_dsss\_pool\_create**](#function-dp_async_dsss_pool_create) (const uint8\_t \* code, size\_t code\_len, size\_t n\_receivers, double chip\_rate, double symbol\_rate, size\_t spc, int m, double cn0\_dbhz, double pfa, double pd, double doppler\_uncertainty, size\_t segments, size\_t sps, int differential, double refine\_max\_error\_db, size\_t refine\_samples\_per\_symbol, double refine\_design\_margin\_db, size\_t refine\_n\_fft, size\_t refine\_zero\_pad, bool refine\_sequential, size\_t refine\_max\_n\_blocks, double carrier\_freq\_hz, double lost\_confirm\_s, bool reacquire, double code\_only\_s, double doppler\_rate, int threads) <br>_Create a pool of_ `n_receivers` _receivers built alike._ |
|  void | [**dp\_async\_dsss\_pool\_destroy**](#function-dp_async_dsss_pool_destroy) ([**dp\_async\_dsss\_pool\_state\_t**](structdp__async__dsss__pool__state__t.md) \* state) <br>_Destroy a pool and every receiver._  |
|  size\_t | [**dp\_async\_dsss\_pool\_get\_coherent\_bins**](#function-dp_async_dsss_pool_get_coherent_bins) (const [**dp\_async\_dsss\_pool\_state\_t**](structdp__async__dsss__pool__state__t.md) \* state) <br>_The receivers' slow-time depth D, epochs per coherent block (_ [_**dp\_async\_dsss\_receiver\_get\_coherent\_bins()**_](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_get_coherent_bins) _); alike for all._ |
|  void | [**dp\_async\_dsss\_pool\_get\_state**](#function-dp_async_dsss_pool_get_state) (const [**dp\_async\_dsss\_pool\_state\_t**](structdp__async__dsss__pool__state__t.md) \* state, void \* blob) <br>_Write the state into_ `blob:` _the clock, the release counts, the thread count (a mutator's value, #2022), the samples carried to the next whole period, the ghost rule's history (each receiver's carriers and code-lock flags over its window), and each receiver's state, which carries the exclusions and rank the controller gave it. The last push's symbols do not travel._ |
|  int | [**dp\_async\_dsss\_pool\_get\_threads**](#function-dp_async_dsss_pool_get_threads) (const [**dp\_async\_dsss\_pool\_state\_t**](structdp__async__dsss__pool__state__t.md) \* state) <br>_Workers push() steps the receivers on, the calling thread included: what set\_threads() asked for, at most n\_receivers, and the online cores for 0._  |
|  int | [**dp\_async\_dsss\_pool\_ghost\_of**](#function-dp_async_dsss_pool_ghost_of) (const [**dp\_async\_dsss\_pool\_state\_t**](structdp__async__dsss__pool__state__t.md) \* state, size\_t i, size\_t j) <br>_Whether receiver_ `j` _tracks a Doppler-offset sidelobe of receiver_`i's` _signal, by the controller's ghost rule (header step 2) over the history it holds now._ |
|  size\_t | [**dp\_async\_dsss\_pool\_push**](#function-dp_async_dsss_pool_push) ([**dp\_async\_dsss\_pool\_state\_t**](structdp__async__dsss__pool__state__t.md) \* state, const float \_Complex \* x, size\_t x\_len) <br>_Step every receiver on one block, running the controller at every code period it crosses._  |
|  [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* | [**dp\_async\_dsss\_pool\_receiver**](#function-dp_async_dsss_pool_receiver) (const [**dp\_async\_dsss\_pool\_state\_t**](structdp__async__dsss__pool__state__t.md) \* state, size\_t receiver) <br>_Borrow receiver_ `receiver` _, for C-only configuration: its lock and chain settings (_[_**dp\_async\_dsss\_receiver\_configure\_lock\_raw()**_](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_configure_lock_raw) _,_[_**dp\_async\_dsss\_receiver\_configure\_chain\_raw()**_](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_configure_chain_raw) _)._ |
|  void | [**dp\_async\_dsss\_pool\_reset**](#function-dp_async_dsss_pool_reset) ([**dp\_async\_dsss\_pool\_state\_t**](structdp__async__dsss__pool__state__t.md) \* state) <br>_Every receiver back to searching, with no exclusions and rank 0; the clock, the release counts and the ghost history back to 0._  |
|  int | [**dp\_async\_dsss\_pool\_set\_state**](#function-dp_async_dsss_pool_set_state) ([**dp\_async\_dsss\_pool\_state\_t**](structdp__async__dsss__pool__state__t.md) \* state, const void \* blob) <br>_Restore the state, whole or not at all._ `DP_ERR_INVALID` _on a blob of another object or version, another receiver count, a carry of a period or more, a ghost history control() cannot leave (a length past the window, a flag other than 0 or 1, a nonzero entry outside a ring's span, or a history on a receiver not tracking), or a receiver blob its receiver refuses; a refusal leaves the pool and every receiver as they were._ |
|  int | [**dp\_async\_dsss\_pool\_set\_telemetry**](#function-dp_async_dsss_pool_set_telemetry) ([**dp\_async\_dsss\_pool\_state\_t**](structdp__async__dsss__pool__state__t.md) \* state, [**dp\_tlm\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_t) \* tlm, const char \* prefix, uint32\_t decim) <br>_Attach (or detach) a telemetry context across every receiver._  |
|  int | [**dp\_async\_dsss\_pool\_set\_threads**](#function-dp_async_dsss_pool_set_threads) ([**dp\_async\_dsss\_pool\_state\_t**](structdp__async__dsss__pool__state__t.md) \* state, int n) <br>_Set the workers push() steps the receivers on._  |
|  size\_t | [**dp\_async\_dsss\_pool\_state\_bytes**](#function-dp_async_dsss_pool_state_bytes) (const [**dp\_async\_dsss\_pool\_state\_t**](structdp__async__dsss__pool__state__t.md) \* state) <br>_Bytes in the blob_ [_**dp\_async\_dsss\_pool\_get\_state()**_](async__dsss__pool__core_8h.md#function-dp_async_dsss_pool_get_state) _writes: the envelope, the pool's own fields, and every receiver's own blob._ |
|  [**async\_dsss\_receiver\_status\_t**](structasync__dsss__receiver__status__t.md) | [**dp\_async\_dsss\_pool\_status**](#function-dp_async_dsss_pool_status) (const [**dp\_async\_dsss\_pool\_state\_t**](structdp__async__dsss__pool__state__t.md) \* state, size\_t receiver) <br>_Receiver_ `receiver's` _status, now._ |
|  size\_t | [**dp\_async\_dsss\_pool\_symbols**](#function-dp_async_dsss_pool_symbols) (const [**dp\_async\_dsss\_pool\_state\_t**](structdp__async__dsss__pool__state__t.md) \* state, size\_t receiver, float \_Complex \* out, size\_t max\_out) <br>_The symbols receiver_ `receiver` _emitted during the last push()._ |
|  size\_t | [**dp\_async\_dsss\_pool\_symbols\_max\_out**](#function-dp_async_dsss_pool_symbols_max_out) (const [**dp\_async\_dsss\_pool\_state\_t**](structdp__async__dsss__pool__state__t.md) \* state) <br>_The most symbols symbols() can return: the largest count any receiver emitted during the last push()._  |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**ASYNC\_DSSS\_POOL\_GHOST\_EPS\_HZ**](async__dsss__pool__core_8h.md#define-async_dsss_pool_ghost_eps_hz)  `10.0`<br> |
| define  | [**ASYNC\_DSSS\_POOL\_GHOST\_LOCK\_FRAC**](async__dsss__pool__core_8h.md#define-async_dsss_pool_ghost_lock_frac)  `0.2`<br> |
| define  | [**ASYNC\_DSSS\_POOL\_GHOST\_STILL\_HZ**](async__dsss__pool__core_8h.md#define-async_dsss_pool_ghost_still_hz)  `10.0`<br> |
| define  | [**ASYNC\_DSSS\_POOL\_GHOST\_WINDOW\_S**](async__dsss__pool__core_8h.md#define-async_dsss_pool_ghost_window_s)  `0.2`<br> |
| define  | [**ASYNC\_DSSS\_POOL\_MAX\_RECEIVERS**](async__dsss__pool__core_8h.md#define-async_dsss_pool_max_receivers)  `[**ASYNC\_DSSS\_RX\_MAX\_PEAKS**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_max_peaks)`<br> |
| define  | [**ASYNC\_DSSS\_POOL\_NO\_RECEIVER**](async__dsss__pool__core_8h.md#define-async_dsss_pool_no_receiver)  `(-1)`<br> |
| define  | [**ASYNC\_DSSS\_POOL\_STATE\_MAGIC**](async__dsss__pool__core_8h.md#define-async_dsss_pool_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('A', 'D', 'P', 'L')`<br> |
| define  | [**ASYNC\_DSSS\_POOL\_STATE\_VERSION**](async__dsss__pool__core_8h.md#define-async_dsss_pool_state_version)  `4u`<br> |

## Detailed Description


Every receiver is a whole AsyncDsssReceiver: it searches the full Doppler range for itself, seeds itself, refines, tracks and releases. The pool adds no searcher and no surface. What it adds is the bookkeeping that makes N of them hold N different signals instead of all piling onto the strongest:



* **One block to every receiver, in parallel.** push() hands the same samples to each receiver and steps them on the `dp_parallel` workers, one receiver per task. Each receiver's own search engine runs on one thread ([**dp\_async\_dsss\_receiver\_set\_threads()**](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_set_threads) 1), so N receivers do not each start a pool of every core. A receiver touches only its own state, so the result does not depend on the thread count: one thread and N give the same bytes.
* **Then the controller, serially, at every code period of the stream.** A receiver is fed exactly one code period (`code_len * spc` samples) per call, on the grid of the pool's sample count from create or reset; push() keeps what is left past the last whole period and feeds it with the next push. The controller runs after each period. Every receiver call and every controller step is then a position in the stream, not a call boundary, so any blocking of one stream gives the same receivers, the same releases (a receiver's release clock is read once per call) and the same symbols. The cost is latency: up to one code period of a push waits for the next. It reads every receiver's status() and:
  * **releases the lost.** A receiver in the lost state (its release rule fired: the emitter left, or the seed never locked) is reset to searching. With `reacquire` the receiver does this itself and is never lost.
  * **resolves duplicates.** Two receivers that hold a signal (refining or tracking) within one chip of each other's code phase, circularly, hold the same signal. The one that keeps it is the one tracking over the one refining, then the one in its state longer (status().state\_samples: the first to lock), then the higher C/N0 estimate, then the lower index. The other is reset.
  * **releases ghosts.** A receiver can take a strong emitter's Doppler-offset sidelobe, hundreds of chips from the emitter's own phase, where the duplicate key cannot see it: it despreads the emitter's own data. Two of its traits give it away together (doppler#2529), each over the same trailing window of [**ASYNC\_DSSS\_POOL\_GHOST\_WINDOW\_S**](async__dsss__pool__core_8h.md#define-async_dsss_pool_ghost_window_s) of whole code periods, both receivers tracking throughout:
    * its carrier moves in lockstep with a stronger holder's (a higher status().cn0\_dbhz\_est), a whole nonzero number k of lattice steps away: the carrier delta's mean is within [**ASYNC\_DSSS\_POOL\_GHOST\_EPS\_HZ**](async__dsss__pool__core_8h.md#define-async_dsss_pool_ghost_eps_hz) of k times chip\_rate / (2 code\_len), and its standard deviation below [**ASYNC\_DSSS\_POOL\_GHOST\_STILL\_HZ**](async__dsss__pool__core_8h.md#define-async_dsss_pool_ghost_still_hz). A sidelobe sits on its parent's carrier a tile away, and the carrier loop, updating once a code period from a squaring combine, reads that modulo half the epoch rate. Two emitters' carriers move apart.
    * its Dll holds code lock in fewer than [**ASYNC\_DSSS\_POOL\_GHOST\_LOCK\_FRAC**](async__dsss__pool__core_8h.md#define-async_dsss_pool_ghost_lock_frac) of the window's periods: a sidelobe is not the code's triangle. Neither alone will do: a distinct emitter can co-move with another on the lattice (but holds code lock), and a true track near the floor loses code lock (but its carrier is its own). [**dp\_async\_dsss\_pool\_ghost\_of()**](async__dsss__pool__core_8h.md#function-dp_async_dsss_pool_ghost_of) is the test. The ghost is reset; the search's own sidelobe rule ([**dp\_async\_dsss\_receiver\_set\_rank()**](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_set_rank)) then keeps it off that cell, but for noise lifting it past the gate.


  * **hands out exclusions and ranks.** Every receiver gets the code phases the OTHER holders hold as its exclusions ([**dp\_async\_dsss\_receiver\_set\_exclusions()**](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_set_exclusions)), so a search never takes a signal already held, and a reset loser does not take its signal back. The searching receivers get ranks 0, 1, ... in index order ([**dp\_async\_dsss\_receiver\_set\_rank()**](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_set_rank)), so searchers that decide in the same dwell take different signals. A holder's rank is set to 0, ready for its next search. Both levers take effect at a receiver's next dwell.


* **Reports.** status() is a receiver's status now, symbols() the symbols it emitted during the last push(); both are as of the last whole period fed, since the controller runs right after it, so a released receiver never reads lost.




The receivers share the stream from create on, so their dwells share one block grid (a reset receiver's engine restarts on it), and their ranks line up with no command.


Lifecycle: create, then push() any number of times, reset() at any point, destroy.



```C++
>>> import numpy as np
>>> from doppler.dsss import AsyncDsssPool
>>> from doppler.wfm import Gold
>>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
>>> pool = AsyncDsssPool(code, n_receivers=3, chip_rate=3.069e6,
...                      symbol_rate=2700.0, spc=2,
...                      doppler_uncertainty=500.0, threads=1)
>>> pool.n_receivers
3
>>> pool.push(np.zeros(4096, dtype=np.complex64))   # holders: none
0
>>> [pool.status(i).state for i in range(3)]         # all searching
[0, 0, 0]
```
 


    
## Public Functions Documentation




### function dp\_async\_dsss\_pool\_create 

_Create a pool of_ `n_receivers` _receivers built alike._
```C++
dp_async_dsss_pool_state_t * dp_async_dsss_pool_create (
    const uint8_t * code,
    size_t code_len,
    size_t n_receivers,
    double chip_rate,
    double symbol_rate,
    size_t spc,
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
    int threads
) 
```



Every parameter after `n_receivers` and before `threads` is [**dp\_async\_dsss\_receiver\_create()**](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_create)'s, passed through to each receiver unchanged; see it for each one's meaning, range and default. Each receiver's search engine is then set to one thread.




**Parameters:**


* `code` Spreading code, 0/1 chips. 
* `code_len` Chips in `code`. 
* `n_receivers` Receivers, 1 to [**ASYNC\_DSSS\_POOL\_MAX\_RECEIVERS**](async__dsss__pool__core_8h.md#define-async_dsss_pool_max_receivers); the most signals the pool holds at once (default 3). 
* `chip_rate` Chip rate, Hz. 
* `symbol_rate` Symbol rate, Hz. 
* `spc` Samples per chip. 
* `m` PSK order. 
* `cn0_dbhz` Design C/N0, dB-Hz. 
* `pfa` Search false-alarm probability. 
* `pd` Search detection probability. 
* `doppler_uncertainty` Doppler search half-range, Hz. 
* `segments` Dll segments per symbol. 
* `sps` Demodulator samples per symbol. 
* `differential` Differential detection (nonzero) or coherent. 
* `refine_max_error_db` Refine stage's allowed loss, dB. 
* `refine_samples_per_symbol` Refine stage's samples per symbol. 
* `refine_design_margin_db` Refine stage's design margin, dB. 
* `refine_n_fft` Refine stage's FFT length. 
* `refine_zero_pad` Refine stage's zero-pad factor. 
* `refine_sequential` Refine stage's sequential mode. 
* `refine_max_n_blocks` Refine stage's block cap. 
* `carrier_freq_hz` RF carrier for code aiding, Hz; 0 = none. 
* `lost_confirm_s` The release rule's confirm time, s (default 2.0 here: a pool whose receivers never release never frees one). 0 = never lost. 
* `reacquire` The receiver's own release-to-search. 
* `code_only_s` Code-only window, s: sets the slow-time depth D. 
* `doppler_rate` Doppler rate bound, Hz/s. 
* `threads` Workers push() steps the receivers on, the calling thread included: 1 = serial, 0 = the online cores (default). Never more than `n_receivers` run. 



**Returns:**

A new pool, every receiver searching; NULL on an invalid argument, or one any receiver refuses. 




**Note:**

Caller must call [**dp\_async\_dsss\_pool\_destroy()**](async__dsss__pool__core_8h.md#function-dp_async_dsss_pool_destroy) when done. 
```C++
>>> import numpy as np
>>> from doppler.dsss import AsyncDsssPool
>>> from doppler.wfm import Gold
>>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
>>> pool = AsyncDsssPool(code, n_receivers=3, chip_rate=3.069e6,
...                      symbol_rate=2700.0, spc=2,
...                      doppler_uncertainty=500.0, threads=1)
>>> (pool.n_receivers, pool.threads, pool.coherent_bins)
(3, 1, 1)
>>> AsyncDsssPool(code, n_receivers=17)    # one rank per peak: 16 at most
Traceback (most recent call last):
    ...
ValueError: AsyncDsssPool: invalid parameter ...
```
 





        

<hr>



### function dp\_async\_dsss\_pool\_destroy 

_Destroy a pool and every receiver._ 
```C++
void dp_async_dsss_pool_destroy (
    dp_async_dsss_pool_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function dp\_async\_dsss\_pool\_get\_coherent\_bins 

_The receivers' slow-time depth D, epochs per coherent block (_ [_**dp\_async\_dsss\_receiver\_get\_coherent\_bins()**_](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_get_coherent_bins) _); alike for all._
```C++
size_t dp_async_dsss_pool_get_coherent_bins (
    const dp_async_dsss_pool_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

D, read from receiver 0. 





        

<hr>



### function dp\_async\_dsss\_pool\_get\_state 

_Write the state into_ `blob:` _the clock, the release counts, the thread count (a mutator's value, #2022), the samples carried to the next whole period, the ghost rule's history (each receiver's carriers and code-lock flags over its window), and each receiver's state, which carries the exclusions and rank the controller gave it. The last push's symbols do not travel._
```C++
void dp_async_dsss_pool_get_state (
    const dp_async_dsss_pool_state_t * state,
    void * blob
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `blob` Destination; at least [**dp\_async\_dsss\_pool\_state\_bytes()**](async__dsss__pool__core_8h.md#function-dp_async_dsss_pool_state_bytes) bytes. 




        

<hr>



### function dp\_async\_dsss\_pool\_get\_threads 

_Workers push() steps the receivers on, the calling thread included: what set\_threads() asked for, at most n\_receivers, and the online cores for 0._ 
```C++
int dp_async_dsss_pool_get_threads (
    const dp_async_dsss_pool_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

The effective worker count, &gt;= 1. 





        

<hr>



### function dp\_async\_dsss\_pool\_ghost\_of 

_Whether receiver_ `j` _tracks a Doppler-offset sidelobe of receiver_`i's` _signal, by the controller's ghost rule (header step 2) over the history it holds now._
```C++
int dp_async_dsss_pool_ghost_of (
    const dp_async_dsss_pool_state_t * state,
    size_t i,
    size_t j
) 
```



Both tests over the last [**ASYNC\_DSSS\_POOL\_GHOST\_WINDOW\_S**](async__dsss__pool__core_8h.md#define-async_dsss_pool_ghost_window_s) of controller steps: `j's` carrier minus `i's` has a mean within [**ASYNC\_DSSS\_POOL\_GHOST\_EPS\_HZ**](async__dsss__pool__core_8h.md#define-async_dsss_pool_ghost_eps_hz) of a nonzero multiple of `lattice_hz` and a standard deviation below [**ASYNC\_DSSS\_POOL\_GHOST\_STILL\_HZ**](async__dsss__pool__core_8h.md#define-async_dsss_pool_ghost_still_hz), and `j` held code lock in fewer than [**ASYNC\_DSSS\_POOL\_GHOST\_LOCK\_FRAC**](async__dsss__pool__core_8h.md#define-async_dsss_pool_ghost_lock_frac) of those steps. Both must have tracked the whole window. Which of the two is stronger is the controller's question, not this one's.




**Parameters:**


* `state` Must be non-NULL. 
* `i` The stronger holder. 
* `j` The candidate ghost. 



**Returns:**

1 if `j` is `i's` ghost; 0 otherwise, and for an index past the pool or `i` equal to `j`. 





        

<hr>



### function dp\_async\_dsss\_pool\_push 

_Step every receiver on one block, running the controller at every code period it crosses._ 
```C++
size_t dp_async_dsss_pool_push (
    dp_async_dsss_pool_state_t * state,
    const float _Complex * x,
    size_t x_len
) 
```



Hands every whole code period it completes to every receiver (header step 1), keeps each one's symbols for symbols(), and after each period releases the lost, resolves duplicates, releases ghosts and hands out exclusions and ranks (step 2). The samples past the last whole period are kept for the next push. Accepts any block size, and state carries across calls: the result is a function of the stream, not of its blocking.




**Parameters:**


* `state` Must be non-NULL. 
* `x` Input cf32 samples. 
* `x_len` Number of input samples. 



**Returns:**

The receivers holding a signal (refining or tracking) at the end of the block. 
```C++
>>> import numpy as np
>>> from doppler.dsss import AsyncDsssPool
>>> from doppler.wfm import Gold
>>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
>>> pool = AsyncDsssPool(code, n_receivers=2, chip_rate=3.069e6,
...                      symbol_rate=2700.0, spc=2,
...                      doppler_uncertainty=500.0, threads=1)
>>> pool.push(np.zeros(2046, dtype=np.complex64))
0
```
 





        

<hr>



### function dp\_async\_dsss\_pool\_receiver 

_Borrow receiver_ `receiver` _, for C-only configuration: its lock and chain settings (_[_**dp\_async\_dsss\_receiver\_configure\_lock\_raw()**_](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_configure_lock_raw) _,_[_**dp\_async\_dsss\_receiver\_configure\_chain\_raw()**_](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_configure_chain_raw) _)._
```C++
dp_async_dsss_receiver_state_t * dp_async_dsss_pool_receiver (
    const dp_async_dsss_pool_state_t * state,
    size_t receiver
) 
```



Not for telemetry: [**dp\_async\_dsss\_pool\_set\_telemetry()**](async__dsss__pool__core_8h.md#function-dp_async_dsss_pool_set_telemetry) is the one route that keeps a context to one producer. The controller sets the exclusions and the rank at every push, so a value set through the borrow for either is overwritten.




**Parameters:**


* `state` Must be non-NULL. 
* `receiver` 0 to n\_receivers - 1. 



**Returns:**

The receiver, owned by the pool; NULL past the pool. 





        

<hr>



### function dp\_async\_dsss\_pool\_reset 

_Every receiver back to searching, with no exclusions and rank 0; the clock, the release counts and the ghost history back to 0._ 
```C++
void dp_async_dsss_pool_reset (
    dp_async_dsss_pool_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 
```C++
>>> import numpy as np
>>> from doppler.dsss import AsyncDsssPool
>>> from doppler.wfm import Gold
>>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
>>> pool = AsyncDsssPool(code, chip_rate=3.069e6, symbol_rate=2700.0,
...                      spc=2, doppler_uncertainty=500.0, threads=1)
>>> _ = pool.push(np.zeros(2046, dtype=np.complex64))
>>> pool.samples
2046
>>> pool.reset()
>>> pool.samples
0
```
 




        

<hr>



### function dp\_async\_dsss\_pool\_set\_state 

_Restore the state, whole or not at all._ `DP_ERR_INVALID` _on a blob of another object or version, another receiver count, a carry of a period or more, a ghost history control() cannot leave (a length past the window, a flag other than 0 or 1, a nonzero entry outside a ring's span, or a history on a receiver not tracking), or a receiver blob its receiver refuses; a refusal leaves the pool and every receiver as they were._
```C++
int dp_async_dsss_pool_set_state (
    dp_async_dsss_pool_state_t * state,
    const void * blob
) 
```





**Parameters:**


* `state` Must be non-NULL; built with the same code, receiver count and geometry as the writer. 
* `blob` A blob from [**dp\_async\_dsss\_pool\_get\_state()**](async__dsss__pool__core_8h.md#function-dp_async_dsss_pool_get_state). 



**Returns:**

DP\_OK, or DP\_ERR\_INVALID as above. 





        

<hr>



### function dp\_async\_dsss\_pool\_set\_telemetry 

_Attach (or detach) a telemetry context across every receiver._ 
```C++
int dp_async_dsss_pool_set_telemetry (
    dp_async_dsss_pool_state_t * state,
    dp_tlm_t * tlm,
    const char * prefix,
    uint32_t decim
) 
```



Receiver i reports under "&lt;prefix&gt;.r&lt;i&gt;" exactly as [**dp\_async\_dsss\_receiver\_set\_telemetry()**](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_set_telemetry) describes, on the one context. A context is single-producer, so this needs one producer: it is refused unless push() runs on one worker (get\_threads() 1), and while a context is attached set\_threads() refuses more. Parallel telemetry, one context per receiver, is doppler#2547.


Whole or nothing: every receiver's names must fit before any registers on `tlm` (they are first attached to a scratch context), and a refusal leaves the pool on its previous context, or detached. The attachment is a request kept on the pool, so a receiver the controller resets keeps reporting under the same names.




**Parameters:**


* `state` Must be non-NULL. 
* `tlm` Context to attach, or NULL to detach every receiver. 
* `prefix` Probe-name prefix; NULL means "pool". 
* `decim` Emit every decim-th event on each probe; &gt;= 1. 



**Returns:**

`DP_OK`; `DP_ERR_INVALID` on more than one worker, decim 0, a prefix too long for a receiver's names, or no room for them. 
```C++
>>> import numpy as np
>>> from doppler.dsss import AsyncDsssPool
>>> from doppler.telemetry import Telemetry
>>> from doppler.wfm import Gold
>>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
>>> pool = AsyncDsssPool(code, n_receivers=2, chip_rate=3.069e6,
...                      symbol_rate=2700.0, spc=2,
...                      doppler_uncertainty=500.0, threads=1)
>>> tlm = Telemetry(1 << 16)
>>> pool.set_telemetry(tlm, "pool")
>>> sorted(p for p in tlm.probe_names if p.endswith(".state"))
['pool.r0.state', 'pool.r1.state']
```
 





        

<hr>



### function dp\_async\_dsss\_pool\_set\_threads 

_Set the workers push() steps the receivers on._ 
```C++
int dp_async_dsss_pool_set_threads (
    dp_async_dsss_pool_state_t * state,
    int n
) 
```



The receivers' results do not change with it (header step 1); only the time a push takes does. A mutator's value, so it travels in the state blob (#2022).




**Parameters:**


* `state` Must be non-NULL. 
* `n` 1 = serial, 0 = the online cores; at most n\_receivers run. 



**Returns:**

`DP_OK`; `DP_ERR_INVALID` (unchanged) on a negative `n`, or on one that would run more than one worker while a telemetry context is attached ([**dp\_async\_dsss\_pool\_set\_telemetry()**](async__dsss__pool__core_8h.md#function-dp_async_dsss_pool_set_telemetry)). 
```C++
>>> import numpy as np
>>> from doppler.dsss import AsyncDsssPool
>>> from doppler.wfm import Gold
>>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
>>> pool = AsyncDsssPool(code, n_receivers=2, chip_rate=3.069e6,
...                      symbol_rate=2700.0, spc=2,
...                      doppler_uncertainty=500.0)
>>> pool.set_threads(1)
>>> pool.threads
1
```
 





        

<hr>



### function dp\_async\_dsss\_pool\_state\_bytes 

_Bytes in the blob_ [_**dp\_async\_dsss\_pool\_get\_state()**_](async__dsss__pool__core_8h.md#function-dp_async_dsss_pool_get_state) _writes: the envelope, the pool's own fields, and every receiver's own blob._
```C++
size_t dp_async_dsss_pool_state_bytes (
    const dp_async_dsss_pool_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

The blob size in bytes; it depends on the receiver count, the code period and each receiver's own state size. 





        

<hr>



### function dp\_async\_dsss\_pool\_status 

_Receiver_ `receiver's` _status, now._
```C++
async_dsss_receiver_status_t dp_async_dsss_pool_status (
    const dp_async_dsss_pool_state_t * state,
    size_t receiver
) 
```



[**dp\_async\_dsss\_receiver\_status()**](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_status) of that receiver, as of the last whole code period fed, after the controller's step on it: a receiver the controller released reads searching. A `receiver` past the pool returns a zeroed record with `state` [**ASYNC\_DSSS\_POOL\_NO\_RECEIVER**](async__dsss__pool__core_8h.md#define-async_dsss_pool_no_receiver).




**Parameters:**


* `state` Must be non-NULL. 
* `receiver` 0 to n\_receivers - 1. 



**Returns:**

The status record. 
```C++
>>> import numpy as np
>>> from doppler.dsss import AsyncDsssPool
>>> from doppler.wfm import Gold
>>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
>>> pool = AsyncDsssPool(code, n_receivers=2, chip_rate=3.069e6,
...                      symbol_rate=2700.0, spc=2,
...                      doppler_uncertainty=500.0, threads=1)
>>> (pool.status(0).state, pool.status(2).state)  # 2 is past the pool
(0, -1)
```
 





        

<hr>



### function dp\_async\_dsss\_pool\_symbols 

_The symbols receiver_ `receiver` _emitted during the last push()._
```C++
size_t dp_async_dsss_pool_symbols (
    const dp_async_dsss_pool_state_t * state,
    size_t receiver,
    float _Complex * out,
    size_t max_out
) 
```



Empty for a receiver that was searching, refining or lost throughout the push, and for a `receiver` past the pool. Tracking emits symbols whether or not they are a signal's ([**dp\_async\_dsss\_receiver\_steps()**](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_steps)): read status().locked for that. A push replaces them, and set\_state() and reset() clear them.




**Parameters:**


* `state` Must be non-NULL. 
* `receiver` 0 to n\_receivers - 1. 
* `out` Output symbols. 
* `max_out` Capacity of `out`; the symbols past it are not written. 



**Returns:**

Symbols written. 
```C++
>>> import numpy as np
>>> from doppler.dsss import AsyncDsssPool
>>> from doppler.wfm import Gold
>>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
>>> pool = AsyncDsssPool(code, n_receivers=2, chip_rate=3.069e6,
...                      symbol_rate=2700.0, spc=2,
...                      doppler_uncertainty=500.0, threads=1)
>>> _ = pool.push(np.zeros(2046, dtype=np.complex64))
>>> pool.symbols(0).size                    # searching: nothing yet
0
```
 





        

<hr>



### function dp\_async\_dsss\_pool\_symbols\_max\_out 

_The most symbols symbols() can return: the largest count any receiver emitted during the last push()._ 
```C++
size_t dp_async_dsss_pool_symbols_max_out (
    const dp_async_dsss_pool_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

The largest per-receiver symbol count from the last push(); 0 before the first push and after reset() or set\_state(). 





        

<hr>
## Macro Definition Documentation





### define ASYNC\_DSSS\_POOL\_GHOST\_EPS\_HZ 

```C++
#define ASYNC_DSSS_POOL_GHOST_EPS_HZ `10.0`
```



How near the lattice the mean carrier delta must sit, Hz. 


        

<hr>



### define ASYNC\_DSSS\_POOL\_GHOST\_LOCK\_FRAC 

```C++
#define ASYNC_DSSS_POOL_GHOST_LOCK_FRAC `0.2`
```



The code-lock fraction a ghost holds below: of the window's steps, the share with status().code\_locked set. 


        

<hr>



### define ASYNC\_DSSS\_POOL\_GHOST\_STILL\_HZ 

```C++
#define ASYNC_DSSS_POOL_GHOST_STILL_HZ `10.0`
```



The stillness limit: the carrier delta's standard deviation over the window must be below this, Hz. 


        

<hr>



### define ASYNC\_DSSS\_POOL\_GHOST\_WINDOW\_S 

```C++
#define ASYNC_DSSS_POOL_GHOST_WINDOW_S `0.2`
```



The ghost rule's window, s: the trailing span both its tests read (doppler#2529). 


        

<hr>



### define ASYNC\_DSSS\_POOL\_MAX\_RECEIVERS 

```C++
#define ASYNC_DSSS_POOL_MAX_RECEIVERS `ASYNC_DSSS_RX_MAX_PEAKS`
```



Most receivers a pool runs: one rank per peak a dwell reports ([**ASYNC\_DSSS\_RX\_MAX\_PEAKS**](async__dsss__receiver__core_8h.md#define-async_dsss_rx_max_peaks)), so every searcher has a rank. 


        

<hr>



### define ASYNC\_DSSS\_POOL\_NO\_RECEIVER 

```C++
#define ASYNC_DSSS_POOL_NO_RECEIVER `(-1)`
```



`status().state` for a receiver index past the pool: no such receiver. 


        

<hr>



### define ASYNC\_DSSS\_POOL\_STATE\_MAGIC 

```C++
#define ASYNC_DSSS_POOL_STATE_MAGIC `DP_FOURCC ('A', 'D', 'P', 'L')`
```



FourCC type tag ("ADPL") stamped in a pool state blob's header. 


        

<hr>



### define ASYNC\_DSSS\_POOL\_STATE\_VERSION 

```C++
#define ASYNC_DSSS_POOL_STATE_VERSION `4u`
```



Layout version of the pool state blob; a mismatch is rejected. v4, not 3: 'ADPL' v3 was the cell-receiver pool this one replaced (v0.48.0 - v0.73.0), and v0.74.0 shipped this pool as v2. Each version of a magic names one layout, so this pool steps past every number the magic has carried (#2569; scripts/check\_state\_versions.py). 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/async_dsss_pool/async_dsss_pool_core.h`

