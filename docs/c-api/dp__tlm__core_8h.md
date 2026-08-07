

# File dp\_tlm\_core.h



[**FileList**](files.md) **>** [**dp\_tlm**](dir_76b7d6d4427bc094138fa987d2f2ac6b.md) **>** [**dp\_tlm\_core.h**](dp__tlm__core_8h.md)

[Go to the source code of this file](dp__tlm__core_8h_source.md)

_Lightweight scalar telemetry taps for running DSP objects._ [More...](#detailed-description)

* `#include "buffer/buffer.h"`
* `#include "clib_common.h"`
* `#include "jm_perf.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**dp\_tlm**](structdp__tlm.md) <br>_Telemetry context: probe registry + SPSC record ring._  |
| struct | [**dp\_tlm\_probe\_t**](structdp__tlm__probe__t.md) <br>_Per-probe registry entry: name, decimation and accounting._  |
| struct | [**dp\_tlm\_rec\_t**](structdp__tlm__rec__t.md) <br>_One telemetry sample: a probe's scalar value at sample index_ `n` _._ |
| struct | [**dp\_tlm\_stats\_t**](structdp__tlm__stats__t.md) <br>_Context-wide counters, snapshotted together._  |


## Public Types

| Type | Name |
| ---: | :--- |
| typedef struct dp\_tlm\_capture | [**dp\_tlm\_capture\_t**](#typedef-dp_tlm_capture_t)  <br> |
| typedef char | [**dp\_tlm\_rec\_fits\_slot**](#typedef-dp_tlm_rec_fits_slot)  <br> |
| typedef [**dp\_tlm\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_t) | [**dp\_tlm\_state\_t**](#typedef-dp_tlm_state_t)  <br>_jm's spelling of_ [_**dp\_tlm\_t**_](dp__tlm__core_8h.md#typedef-dp_tlm_t) _._ |
| typedef struct [**dp\_tlm**](structdp__tlm.md) | [**dp\_tlm\_t**](#typedef-dp_tlm_t)  <br>_Telemetry context: probe registry + SPSC record ring._  |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  size\_t | [**dp\_tlm\_avail**](#function-dp_tlm_avail) (const [**dp\_tlm\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_t) \* t) <br>_Records currently readable, without consuming them._  |
|  size\_t | [**dp\_tlm\_block\_bound**](#function-dp_tlm_block_bound) (const [**dp\_tlm\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_t) \* t, size\_t block\_samples) <br>_Records this context can emit while processing_ `block_samples` _inputs — the number that makes drops preventable._ |
|  size\_t | [**dp\_tlm\_capacity**](#function-dp_tlm_capacity) (const [**dp\_tlm\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_t) \* t) <br>_Authoritative ring capacity in records (post page rounding)._  |
|  [**dp\_tlm\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_t) \* | [**dp\_tlm\_create**](#function-dp_tlm_create) (size\_t ring\_records) <br>_Creates a telemetry context with a ring of_ `ring_records` _slots._ |
|  void | [**dp\_tlm\_destroy**](#function-dp_tlm_destroy) ([**dp\_tlm\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_t) \* t) <br>_Destroys a context. NULL-safe. Detach all objects first._  |
|  uint64\_t | [**dp\_tlm\_dropped**](#function-dp_tlm_dropped) (const [**dp\_tlm\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_t) \* t) <br>_Total records dropped on ring overrun (monotonic)._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) void | [**dp\_tlm\_emit**](#function-dp_tlm_emit) ([**dp\_tlm\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_t) \* t, int32\_t id, double v) <br>_Records one scalar for probe_ `id` _. The hot-path primitive._ |
|  int | [**dp\_tlm\_emit\_checked**](#function-dp_tlm_emit_checked) ([**dp\_tlm\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_t) \* t, int32\_t id, double v) <br>_Validating_ [_**dp\_tlm\_emit()**_](dp__tlm__core_8h.md#function-dp_tlm_emit) _: refuses an id the registry never issued._ |
|  uint64\_t | [**dp\_tlm\_emitted**](#function-dp_tlm_emitted) (const [**dp\_tlm\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_t) \* t, int id) <br>_Records written for probe_ `id` _(post-decimation, post-drop)._ |
|  int | [**dp\_tlm\_probe**](#function-dp_tlm_probe) ([**dp\_tlm\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_t) \* t, const char \* name, uint32\_t decim) <br>_Registers (or re-registers) a named probe. Setup path, not hot._  |
|  size\_t | [**dp\_tlm\_probe\_count**](#function-dp_tlm_probe_count) (const [**dp\_tlm\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_t) \* t) <br>_Number of registered probes._  |
|  int | [**dp\_tlm\_probe\_id**](#function-dp_tlm_probe_id) (const [**dp\_tlm\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_t) \* t, const char \* name) <br>_Looks up a probe id by name;_ [_**DP\_ERR\_INVALID**_](clib__common_8h.md#define-dp_err_invalid) _if unknown._ |
|  int | [**dp\_tlm\_probe\_id\_at**](#function-dp_tlm_probe_id_at) (const [**dp\_tlm\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_t) \* t, size\_t i) <br>_Probe id at registry slot_ `i` _. Always_`i` _— ids ARE slots._ |
|  const char \* | [**dp\_tlm\_probe\_name**](#function-dp_tlm_probe_name) (const [**dp\_tlm\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_t) \* t, int id) <br>_Probe name for_ `id` _, or NULL if out of range._ |
|  size\_t | [**dp\_tlm\_read**](#function-dp_tlm_read) ([**dp\_tlm\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_t) \* t, size\_t n, [**dp\_tlm\_rec\_t**](structdp__tlm__rec__t.md) \* out, size\_t max\_out) <br>_Drains records into_ `out` _. Non-blocking._ |
|  size\_t | [**dp\_tlm\_read\_max\_out**](#function-dp_tlm_read_max_out) ([**dp\_tlm\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_t) \* t) <br>_Upper bound on what_ [_**dp\_tlm\_read()**_](dp__tlm__core_8h.md#function-dp_tlm_read) _can return right now._ |
|  int | [**dp\_tlm\_resize**](#function-dp_tlm_resize) ([**dp\_tlm\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_t) \* t, size\_t records) <br>_Replaces the ring with one holding at least_ `records` _._ |
|  int | [**dp\_tlm\_set\_decim**](#function-dp_tlm_set_decim) ([**dp\_tlm\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_t) \* t, const char \* name, uint32\_t decim) <br>_Retunes an EXISTING probe's decimation, by name._  |
|  [**dp\_tlm\_stats\_t**](structdp__tlm__stats__t.md) | [**dp\_tlm\_stats**](#function-dp_tlm_stats) (const [**dp\_tlm\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_t) \* t) <br>_Snapshots the context's counters. Zeroed for a NULL context._  |


## Public Static Functions

| Type | Name |
| ---: | :--- |
|  void | [**dp\_tlm\_set\_now**](#function-dp_tlm_set_now) ([**dp\_tlm\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_t) \* t, uint64\_t n) <br>_Stamps the sample index carried by subsequent records, and — when a capture is open — closes out the block just finished._  |

























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**DP\_TLM**](dp__tlm__core_8h.md#define-dp_tlm) (ctx, id, v) `[**dp\_tlm\_emit**](dp__tlm__core_8h.md#function-dp_tlm_emit) ((ctx), (id), (v))`<br>_Probe-site wrapper around_ [_**dp\_tlm\_emit()**_](dp__tlm__core_8h.md#function-dp_tlm_emit) _._ |
| define  | [**DP\_TLM\_MAX\_PROBES**](dp__tlm__core_8h.md#define-dp_tlm_max_probes)  `64`<br> |
| define  | [**DP\_TLM\_NAME\_MAX**](dp__tlm__core_8h.md#define-dp_tlm_name_max)  `32`<br> |

## Detailed Description


A `dp_tlm_t` context lets a hot loop publish named scalar time series (tracking-loop stress, AGC gain, lock metrics, ...) without perturbing the signal path:



* **Detached (the default)**: an instrumented object holds a NULL `dp_tlm_t *`; every probe site is a single pointer load and a predicted-not-taken branch, and only at _event_ rate (per recovered symbol, per gain update) — never per input sample. Consumers who want literal zero can compile with `-DDP_TLM_DISABLE`, which turns the `DP_TLM()` probe macro into `((void) 0)`.
* **Attached**: each emit is a per-probe decimation check plus one 16-byte record written into a lock-free VM-mirrored SPSC ring ([**buffer/buffer.h**](buffer_8h.md)). The write never blocks and never allocates; on overrun the record is dropped and counted, so a slow (or absent) reader can never stall the DSP thread.




## Drops are preventable, not merely countable



Dropping is the ring's _fallback_, not the intended steady state. Because no probe can emit more than once per input sample, a block of `N` inputs emits at most `dp_tlm_probe_count() * N` records — see [**dp\_tlm\_block\_bound()**](dp__tlm__core_8h.md#function-dp_tlm_block_bound). A ring sized to that bound and drained to empty at every block boundary therefore _cannot_ overflow, which is what [**dp\_tlm\_capture\_open()**](dp__tlm__capture__core_8h.md#function-dp_tlm_capture_open) ([**dp\_tlm\_capture/dp\_tlm\_capture\_core.h**](dp__tlm__capture__core_8h.md)) sets up for you. Prefer a capture to a hand-rolled drain loop: guessing a ring size and hoping the reader keeps up is the failure mode this bound exists to retire.



## Threading contract



The ring is single-producer / single-consumer:



* All objects attached to one context must step on ONE producer thread (true of any doppler pipeline). Use one context per pipeline/thread.
* `dp_tlm_read()` may run concurrently on one consumer thread — that hand-off is the ring's whole design.
* Probe registration (`dp_tlm_probe`, i.e. `obj_set_telemetry`) must complete before the producer starts stepping: the probe table is written unlocked at setup time.





## Timestamps



Records carry a caller-maintained sample index `now` (stamp it once per block from the pipeline's `dp_sample_clock_t` via `dp_tlm_set_now`). If never stamped it stays 0 and consumers index by record order — fine for per-symbol series.



```C++
dp_tlm_t *tlm = dp_tlm_create (1 << 14);
int id = dp_tlm_probe (tlm, "agc.gain_db", 1);
...
DP_TLM (tlm, id, gain_db);            // in the hot loop, per event
...
dp_tlm_rec_t recs[512];
size_t n = dp_tlm_read (tlm, 512, recs, 512);   // on the consumer side
dp_tlm_destroy (tlm);
```
 



    
## Public Types Documentation




### typedef dp\_tlm\_capture\_t 

```C++
typedef struct dp_tlm_capture dp_tlm_capture_t;
```



Opaque lossless capture ([**dp\_tlm\_capture\_core.h**](dp__tlm__capture__core_8h.md)); see dp\_tlm\_set\_now. 


        

<hr>



### typedef dp\_tlm\_rec\_fits\_slot 

```C++
typedef char dp_tlm_rec_fits_slot[sizeof(dp_tlm_rec_t)==2 *sizeof(uint64_t) ? 1 :-1];
```




<hr>



### typedef dp\_tlm\_state\_t 

_jm's spelling of_ [_**dp\_tlm\_t**_](dp__tlm__core_8h.md#typedef-dp_tlm_t) _._
```C++
typedef dp_tlm_t dp_tlm_state_t;
```



jm derives an object's state struct as `<component>_state_t` with no override (just-makeit#797), and this type predates jm by years — it is in the signature of every instrumented object's `*_set_telemetry`, so renaming it is not on the table. An alias costs one line and nothing at runtime.


Not a second type: `dp_tlm_t` remains the name to write. This exists so the generated binding compiles, and it goes away when jm#797 lands `state_type`. 


        

<hr>



### typedef dp\_tlm\_t 

_Telemetry context: probe registry + SPSC record ring._ 
```C++
typedef struct dp_tlm dp_tlm_t;
```



Public (not opaque) because the emit path is inline; treat the fields as read-only outside dp\_tlm\_core.c and dp\_tlm\_emit.


`capture` is deliberately LAST: the emit hot path touches `ring`, `now` and `probes`, and appending here leaves their cache layout untouched. 


        

<hr>
## Public Functions Documentation




### function dp\_tlm\_avail 

_Records currently readable, without consuming them._ 
```C++
size_t dp_tlm_avail (
    const dp_tlm_t * t
) 
```



The consumer-side head/tail snapshot. Safe to call from the consumer thread while the producer runs: the true count can only GROW after the snapshot, so the value is a lower bound and never over-reports. 


        

<hr>



### function dp\_tlm\_block\_bound 

_Records this context can emit while processing_ `block_samples` _inputs — the number that makes drops preventable._
```C++
size_t dp_tlm_block_bound (
    const dp_tlm_t * t,
    size_t block_samples
) 
```



`probe_count * block_samples`, and that is a genuine upper bound rather than an estimate: **no probe can emit more than once per input sample.** Verified across every object with a `*_set_telemetry` — the interpolating ones are not counterexamples, because a cascade that produces several outputs from one input collapses them into a single `emitted |=` strobe ([**ratesync\_core.h**](ratesync__core_8h.md), [**mpsk\_receiver\_core.h**](mpsk__receiver__core_8h.md)), so one input yields at most one flush. Each probe belongs to exactly one object, so summing over objects is just the context-wide probe count.


Size a ring to this and drain it to empty every block and the ring cannot overflow — no scheduling assumption, no safety factor. Registering more probes raises the bound, which is why a capture re-checks it at each boundary.




**Returns:**

The bound, or 0 for a NULL context / zero block / no probes. Saturates at SIZE\_MAX rather than wrapping. 





        

<hr>



### function dp\_tlm\_capacity 

_Authoritative ring capacity in records (post page rounding)._ 
```C++
size_t dp_tlm_capacity (
    const dp_tlm_t * t
) 
```




<hr>



### function dp\_tlm\_create 

_Creates a telemetry context with a ring of_ `ring_records` _slots._
```C++
dp_tlm_t * dp_tlm_create (
    size_t ring_records
) 
```





**Parameters:**


* `ring_records` Requested ring capacity in records. MUST be a power of 2. Sub-page requests are rounded up to the page minimum ([**buffer.h**](buffer_8h.md) semantics) — read the authoritative value back with [**dp\_tlm\_capacity()**](dp__tlm__core_8h.md#function-dp_tlm_capacity). 



**Returns:**

New context, or NULL on invalid size / allocation failure. 





        

<hr>



### function dp\_tlm\_destroy 

_Destroys a context. NULL-safe. Detach all objects first._ 
```C++
void dp_tlm_destroy (
    dp_tlm_t * t
) 
```




<hr>



### function dp\_tlm\_dropped 

_Total records dropped on ring overrun (monotonic)._ 
```C++
uint64_t dp_tlm_dropped (
    const dp_tlm_t * t
) 
```




<hr>



### function dp\_tlm\_emit 

_Records one scalar for probe_ `id` _. The hot-path primitive._
```C++
JM_FORCEINLINE void dp_tlm_emit (
    dp_tlm_t * t,
    int32_t id,
    double v
) 
```



Detached (`t` NULL) this is one branch — the entire disabled cost. Attached: bump the probe's decimation phase, and on the decim-th event write one 16-byte record (value narrowed to float, stamped with the context's current `now`). Never blocks, never allocates; on ring overrun the record is dropped and counted.


`id` must come from a successful [**dp\_tlm\_probe()**](dp__tlm__core_8h.md#function-dp_tlm_probe) on this context — an object's set\_telemetry fails the whole attach otherwise.


The bound checked here is the ARRAY's, not the registry's. `probes` is a fixed DP\_TLM\_MAX\_PROBES array, so the unguarded indexing this used to do turned any out-of-range id into an out-of-bounds write — reachable from a language binding, where the id is whatever the caller passed, and `Telemetry.emit(1000000, 1.0)` segfaulted the interpreter. Comparing against the compile-time constant (unsigned, so a negative id fails it too) needs no memory and measures free. Comparing against `n_probes` instead would also reject an in-range-but-unregistered id, but it loads a field on the early-return path and cost ~16% of the decimated case (bench\_telemetry\_core, ABBA-interleaved) — so _that_ check belongs at the binding boundary, where the id is untrusted, not in the hot loop, where the caller holds an id [**dp\_tlm\_probe()**](dp__tlm__core_8h.md#function-dp_tlm_probe) gave it.




**Parameters:**


* `t` Context; NULL is a no-op (the detached case). 
* `id` Probe id from [**dp\_tlm\_probe()**](dp__tlm__core_8h.md#function-dp_tlm_probe) on THIS context. 
* `v` The scalar, narrowed to float by the ring record.

The Python face binds [**dp\_tlm\_emit\_checked()**](dp__tlm__core_8h.md#function-dp_tlm_emit_checked) instead, which additionally refuses an id the registry never issued — see its docs for why the hot path does not.



```C++
>>> from doppler.telemetry import Telemetry
>>> tlm = Telemetry(1 << 12)
>>> pid = tlm.probe("rx.snr_db")
>>> tlm.emit(pid, 12.5)
>>> float(tlm.read()[0]["value"])
12.5

An id the registry never issued is refused, not written:

>>> tlm.emit(pid + 1, 1.0)
Traceback (most recent call last):
ValueError: emit failed (rc=-4)
```
 


        

<hr>



### function dp\_tlm\_emit\_checked 

_Validating_ [_**dp\_tlm\_emit()**_](dp__tlm__core_8h.md#function-dp_tlm_emit) _: refuses an id the registry never issued._
```C++
int dp_tlm_emit_checked (
    dp_tlm_t * t,
    int32_t id,
    double v
) 
```



The out-of-line twin of the inline hot-path emit, for callers whose id did not come from [**dp\_tlm\_probe()**](dp__tlm__core_8h.md#function-dp_tlm_probe) on this context — in practice, a language binding, where the id is whatever the caller passed. [**dp\_tlm\_emit()**](dp__tlm__core_8h.md#function-dp_tlm_emit) checks only the ARRAY bound (see its docs: checking `n_probes` there costs ~16% of the decimated path), so an in-range but unregistered id reaches it and emits a record against a probe nobody registered. Here that is an error.


C hot loops keep calling [**dp\_tlm\_emit()**](dp__tlm__core_8h.md#function-dp_tlm_emit) directly and pay nothing for this.




**Parameters:**


* `t` Context. NULL is rejected. 
* `id` Probe id from [**dp\_tlm\_probe()**](dp__tlm__core_8h.md#function-dp_tlm_probe) on THIS context. 
* `v` The scalar, narrowed to float by the ring record. 



**Returns:**

[**DP\_OK**](clib__common_8h.md#define-dp_ok), or [**DP\_ERR\_INVALID**](clib__common_8h.md#define-dp_err_invalid) on a NULL context or an id outside the registry.



```C++
>>> from doppler.telemetry import Telemetry
>>> tlm = Telemetry(1 << 12)
>>> pid = tlm.probe("rx.snr_db")
>>> tlm.emit(pid, 12.5)
>>> float(tlm.read()[0]["value"])
12.5

An id the registry never issued is refused, not written:

>>> tlm.emit(pid + 1, 1.0)
Traceback (most recent call last):
ValueError: emit failed (rc=-4)
```
 


        

<hr>



### function dp\_tlm\_emitted 

_Records written for probe_ `id` _(post-decimation, post-drop)._
```C++
uint64_t dp_tlm_emitted (
    const dp_tlm_t * t,
    int id
) 
```



Reconcile against [**dp\_tlm\_dropped()**](dp__tlm__core_8h.md#function-dp_tlm_dropped) to account for losses: what a probe emitted is what reached the ring, not what the call sites offered it.




**Parameters:**


* `t` Context. 
* `id` Probe id from [**dp\_tlm\_probe()**](dp__tlm__core_8h.md#function-dp_tlm_probe). 



**Returns:**

Records written for that probe, 0 for an unknown id.



```C++
>>> from doppler.telemetry import Telemetry
>>> tlm = Telemetry(1 << 12)
>>> eid = tlm.probe("sync.e", decim=2)
>>> for i in range(4):
...     tlm.emit(eid, i / 10)
>>> tlm.emitted(eid)            # decim=2: half the events
2
>>> tlm.dropped
0
```
 


        

<hr>



### function dp\_tlm\_probe 

_Registers (or re-registers) a named probe. Setup path, not hot._ 
```C++
int dp_tlm_probe (
    dp_tlm_t * t,
    const char * name,
    uint32_t decim
) 
```



Idempotent by name: registering an existing name returns its id and updates `decim` (re-attach after a reset keeps ids stable). The decimation phase is primed so the FIRST event after registration emits.




**Parameters:**


* `t` Context. 
* `name` Probe name, e.g. "agc.gain\_db". Must be shorter than DP\_TLM\_NAME\_MAX. 
* `decim` Emit every decim-th event; &gt;= 1. 



**Returns:**

Probe id (&gt;= 0), or DP\_ERR\_INVALID on NULL/overlong name, decim == 0, or a full table.



```C++
>>> from doppler.telemetry import Telemetry
>>> tlm = Telemetry(1 << 12)
>>> tlm.probe("sync.e", decim=4)
0
>>> tlm.probe("sync.e")     # same name: same id, decim retuned
0
>>> tlm.probe_count
1
```
 


        

<hr>



### function dp\_tlm\_probe\_count 

_Number of registered probes._ 
```C++
size_t dp_tlm_probe_count (
    const dp_tlm_t * t
) 
```




<hr>



### function dp\_tlm\_probe\_id 

_Looks up a probe id by name;_ [_**DP\_ERR\_INVALID**_](clib__common_8h.md#define-dp_err_invalid) _if unknown._
```C++
int dp_tlm_probe_id (
    const dp_tlm_t * t,
    const char * name
) 
```





**Parameters:**


* `t` Context. 
* `name` Probe name as passed to [**dp\_tlm\_probe()**](dp__tlm__core_8h.md#function-dp_tlm_probe). 



**Returns:**

Probe id (&gt;= 0), or [**DP\_ERR\_INVALID**](clib__common_8h.md#define-dp_err_invalid) if no such probe.



```C++
>>> from doppler.telemetry import Telemetry
>>> tlm = Telemetry(1 << 12)
>>> _ = tlm.probe("agc.gain_db")
>>> tlm.probe_id("agc.gain_db")
0
>>> tlm.probe_id("never.registered")
Traceback (most recent call last):
KeyError: 'no probe by that name (rc=-4)'
```
 


        

<hr>



### function dp\_tlm\_probe\_id\_at 

_Probe id at registry slot_ `i` _. Always_`i` _— ids ARE slots._
```C++
int dp_tlm_probe_id_at (
    const dp_tlm_t * t,
    size_t i
) 
```



Exists so a `{name: id}` mapping can be built from the plain-C triple (dp\_tlm\_probe\_count, dp\_tlm\_probe\_name, this) without the caller needing to know that the identity holds. 


        

<hr>



### function dp\_tlm\_probe\_name 

_Probe name for_ `id` _, or NULL if out of range._
```C++
const char * dp_tlm_probe_name (
    const dp_tlm_t * t,
    int id
) 
```




<hr>



### function dp\_tlm\_read 

_Drains records into_ `out` _. Non-blocking._
```C++
size_t dp_tlm_read (
    dp_tlm_t * t,
    size_t n,
    dp_tlm_rec_t * out,
    size_t max_out
) 
```



Consumer side of the SPSC ring: safe to call from a different thread than the producer. Returns immediately with whatever is available (possibly 0) — never spins.




**Parameters:**


* `t` Context. 
* `n` Records wanted; 0 means "everything available". 
* `out` Destination. 
* `max_out` Capacity of `out`, in records.

`n` and `max_out` are separate because the binding allocates `out` from [**dp\_tlm\_read\_max\_out()**](dp__tlm__core_8h.md#function-dp_tlm_read_max_out) and then resizes to what came back — so the request and the buffer are genuinely two numbers, and the read is clamped to the smaller.




**Returns:**

Number of records copied out.



```C++
>>> from doppler.telemetry import Telemetry
>>> tlm = Telemetry(1 << 12)
>>> eid = tlm.probe("sync.e")
>>> for i in range(5):
...     tlm.emit(eid, i / 10)
>>> recs = tlm.read(2)          # take two
>>> recs.shape, recs.dtype.names
((2,), ('n', 'value', 'probe', 'flags'))
>>> tlm.read().shape            # 0 means "everything left"
(3,)
>>> tlm.read().shape            # drained
(0,)
```
 


        

<hr>



### function dp\_tlm\_read\_max\_out 

_Upper bound on what_ [_**dp\_tlm\_read()**_](dp__tlm__core_8h.md#function-dp_tlm_read) _can return right now._
```C++
size_t dp_tlm_read_max_out (
    dp_tlm_t * t
) 
```



Simply the available count: a caller sizing a destination cannot know the request will be smaller, and jm's generated binding allocates this much, reads, then resizes to what actually came back. 


        

<hr>



### function dp\_tlm\_resize 

_Replaces the ring with one holding at least_ `records` _._
```C++
int dp_tlm_resize (
    dp_tlm_t * t,
    size_t records
) 
```



Rounds `records` up to a power of two ([**buffer.h**](buffer_8h.md) requires it) and then to the page minimum. A no-op returning [**DP\_OK**](clib__common_8h.md#define-dp_ok) when the ring is already big enough, so it is cheap to call speculatively at every boundary.




**Warning:**

**Destroys whatever the ring holds** and is unsynchronised with the producer. Legal only where the producer is quiescent AND the ring has been drained — i.e. a block boundary. [**dp\_tlm\_capture\_block()**](dp__tlm__capture__core_8h.md#function-dp_tlm_capture_block) is the only caller that needs it; call it yourself only if you own the same guarantee.




**Returns:**

[**DP\_OK**](clib__common_8h.md#define-dp_ok), or [**DP\_ERR\_INVALID**](clib__common_8h.md#define-dp_err_invalid) on NULL / allocation failure (in which case the existing ring is left intact). 





        

<hr>



### function dp\_tlm\_set\_decim 

_Retunes an EXISTING probe's decimation, by name._ 
```C++
int dp_tlm_set_decim (
    dp_tlm_t * t,
    const char * name,
    uint32_t decim
) 
```



Distinct from [**dp\_tlm\_probe()**](dp__tlm__core_8h.md#function-dp_tlm_probe), which registers on a miss: this refuses an unknown name rather than quietly creating a probe nothing emits to, which is what a typo in a retune call deserves.




**Parameters:**


* `t` Context. 
* `name` Name of an ALREADY registered probe. 
* `decim` Emit every decim-th event; &gt;= 1. 



**Returns:**

[**DP\_OK**](clib__common_8h.md#define-dp_ok), or [**DP\_ERR\_INVALID**](clib__common_8h.md#define-dp_err_invalid) on NULL, an unknown name, or `decim` == 0.



```C++
>>> from doppler.telemetry import Telemetry
>>> tlm = Telemetry(1 << 12)
>>> _ = tlm.probe("sync.e", decim=1)
>>> tlm.set_decim("sync.e", 8)      # retune the existing probe
>>> tlm.set_decim("typo.e", 8)      # refused, not silently created
Traceback (most recent call last):
ValueError: set_decim failed (rc=-4)
```
 


        

<hr>



### function dp\_tlm\_stats 

_Snapshots the context's counters. Zeroed for a NULL context._ 
```C++
dp_tlm_stats_t dp_tlm_stats (
    const dp_tlm_t * t
) 
```





**Parameters:**


* `t` Context, or NULL for an all-zero record. 



**Returns:**

The four counters as one [**dp\_tlm\_stats\_t**](structdp__tlm__stats__t.md) value.



```C++
>>> from doppler.telemetry import Telemetry
>>> tlm = Telemetry(1 << 12)
>>> pid = tlm.probe("agc.gain_db")
>>> tlm.emit(pid, -3.5)
>>> tlm.stats()
doppler.telemetry.TelemetryStats(dropped=0, emitted=1, capacity=4096, probes=1)
>>> tlm.stats().emitted
1
```
 


        

<hr>
## Public Static Functions Documentation




### function dp\_tlm\_set\_now 

_Stamps the sample index carried by subsequent records, and — when a capture is open — closes out the block just finished._ 
```C++
static inline void dp_tlm_set_now (
    dp_tlm_t * t,
    uint64_t n
) 
```



Call once per block from whoever owns the pipeline's sample clock (`dp_tlm_set_now (tlm, clk->n)`). NULL-safe so pipeline glue can call it unconditionally.


Callers already place this at the top of the block loop, _before_ stepping, which makes it exactly the boundary a lossless capture needs: delegating here drains the PREVIOUS block, leaving the ring empty as the next one starts. That is the invariant [**dp\_tlm\_block\_bound()**](dp__tlm__core_8h.md#function-dp_tlm_block_bound) is sized against, so an existing `set_now / steps / read` loop becomes lossless by opening a capture and changing nothing else.


With no capture open the behaviour is byte-identical to a bare assignment. The delegation is a cold branch on a per-block call, never a per-sample one, so it is nowhere near the hot loops [**dp\_tlm\_emit()**](dp__tlm__core_8h.md#function-dp_tlm_emit) cares about.




**Parameters:**


* `t` Context; NULL is a no-op. 
* `n` Sample index stamped into every subsequent record.


```C++
>>> from doppler.telemetry import Telemetry
>>> tlm = Telemetry(1 << 12)
>>> pid = tlm.probe("agc.gain_db")
>>> tlm.set_now(1000)           # top of the block, before stepping
>>> tlm.emit(pid, -3.5)
>>> rec = tlm.read()[0]
>>> int(rec["n"]), float(rec["value"])
(1000, -3.5)
```
 


        

<hr>
## Macro Definition Documentation





### define DP\_TLM 

_Probe-site wrapper around_ [_**dp\_tlm\_emit()**_](dp__tlm__core_8h.md#function-dp_tlm_emit) _._
```C++
#define DP_TLM (
    ctx,
    id,
    v
) `dp_tlm_emit ((ctx), (id), (v))`
```



Instrumented hot loops use this form so a consumer building with `-DDP_TLM_DISABLE` compiles every probe site out entirely. 


        

<hr>



### define DP\_TLM\_MAX\_PROBES 

```C++
#define DP_TLM_MAX_PROBES `64`
```



Maximum probes per context. Registration fails once full. 


        

<hr>



### define DP\_TLM\_NAME\_MAX 

```C++
#define DP_TLM_NAME_MAX `32`
```



Maximum probe-name length including the NUL terminator. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/dp_tlm/dp_tlm_core.h`

