

# File telemetry.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**telemetry**](dir_d4543964ddc0423cd91d16ab74a4089e.md) **>** [**telemetry.h**](telemetry_8h.md)

[Go to the source code of this file](telemetry_8h_source.md)

_Lightweight scalar telemetry taps for running DSP objects._ [More...](#detailed-description)

* `#include "buffer/buffer.h"`
* `#include "clib_common.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**dp\_tlm**](structdp__tlm.md) <br>_Telemetry context: probe registry + SPSC record ring._  |
| struct | [**dp\_tlm\_probe\_t**](structdp__tlm__probe__t.md) <br>_Per-probe registry entry: name, decimation and accounting._  |
| struct | [**dp\_tlm\_rec\_t**](structdp__tlm__rec__t.md) <br>_One telemetry sample: a probe's scalar value at sample index_ `n` _._ |


## Public Types

| Type | Name |
| ---: | :--- |
| typedef char | [**dp\_tlm\_rec\_fits\_slot**](#typedef-dp_tlm_rec_fits_slot)  <br> |
| typedef struct [**dp\_tlm**](structdp__tlm.md) | [**dp\_tlm\_t**](#typedef-dp_tlm_t)  <br>_Telemetry context: probe registry + SPSC record ring._  |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  size\_t | [**dp\_tlm\_capacity**](#function-dp_tlm_capacity) (const [**dp\_tlm\_t**](telemetry_8h.md#typedef-dp_tlm_t) \* t) <br>_Authoritative ring capacity in records (post page rounding)._  |
|  [**dp\_tlm\_t**](telemetry_8h.md#typedef-dp_tlm_t) \* | [**dp\_tlm\_create**](#function-dp_tlm_create) (size\_t ring\_records) <br>_Creates a telemetry context with a ring of_ `ring_records` _slots._ |
|  void | [**dp\_tlm\_destroy**](#function-dp_tlm_destroy) ([**dp\_tlm\_t**](telemetry_8h.md#typedef-dp_tlm_t) \* t) <br>_Destroys a context. NULL-safe. Detach all objects first._  |
|  uint64\_t | [**dp\_tlm\_dropped**](#function-dp_tlm_dropped) (const [**dp\_tlm\_t**](telemetry_8h.md#typedef-dp_tlm_t) \* t) <br>_Total records dropped on ring overrun (monotonic)._  |
|  uint64\_t | [**dp\_tlm\_emitted**](#function-dp_tlm_emitted) (const [**dp\_tlm\_t**](telemetry_8h.md#typedef-dp_tlm_t) \* t, int id) <br>_Records written for probe_ `id` _(post-decimation, post-drop)._ |
|  int | [**dp\_tlm\_lookup**](#function-dp_tlm_lookup) (const [**dp\_tlm\_t**](telemetry_8h.md#typedef-dp_tlm_t) \* t, const char \* name) <br>_Looks up a probe id by name; DP\_ERR\_INVALID if unknown._  |
|  int | [**dp\_tlm\_probe**](#function-dp_tlm_probe) ([**dp\_tlm\_t**](telemetry_8h.md#typedef-dp_tlm_t) \* t, const char \* name, uint32\_t decim) <br>_Registers (or re-registers) a named probe. Setup path, not hot._  |
|  size\_t | [**dp\_tlm\_probe\_count**](#function-dp_tlm_probe_count) (const [**dp\_tlm\_t**](telemetry_8h.md#typedef-dp_tlm_t) \* t) <br>_Number of registered probes._  |
|  const char \* | [**dp\_tlm\_probe\_name**](#function-dp_tlm_probe_name) (const [**dp\_tlm\_t**](telemetry_8h.md#typedef-dp_tlm_t) \* t, int id) <br>_Probe name for_ `id` _, or NULL if out of range._ |
|  size\_t | [**dp\_tlm\_read**](#function-dp_tlm_read) ([**dp\_tlm\_t**](telemetry_8h.md#typedef-dp_tlm_t) \* t, [**dp\_tlm\_rec\_t**](structdp__tlm__rec__t.md) \* out, size\_t max\_recs) <br>_Drains up to_ `max_recs` _records into_`out` _. Non-blocking._ |


## Public Static Functions

| Type | Name |
| ---: | :--- |
|  void | [**dp\_tlm\_emit**](#function-dp_tlm_emit) ([**dp\_tlm\_t**](telemetry_8h.md#typedef-dp_tlm_t) \* t, int32\_t id, double v) <br>_Records one scalar for probe_ `id` _. The hot-path primitive._ |
|  void | [**dp\_tlm\_set\_now**](#function-dp_tlm_set_now) ([**dp\_tlm\_t**](telemetry_8h.md#typedef-dp_tlm_t) \* t, uint64\_t n) <br>_Stamps the sample index carried by subsequent records._  |

























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**DP\_TLM**](telemetry_8h.md#define-dp_tlm) (ctx, id, v) `dp\_tlm\_emit ((ctx), (id), (v))`<br>_Probe-site wrapper around dp\_tlm\_emit()._  |
| define  | [**DP\_TLM\_MAX\_PROBES**](telemetry_8h.md#define-dp_tlm_max_probes)  `64`<br> |
| define  | [**DP\_TLM\_NAME\_MAX**](telemetry_8h.md#define-dp_tlm_name_max)  `32`<br> |

## Detailed Description


A `dp_tlm_t` context lets a hot loop publish named scalar time series (tracking-loop stress, AGC gain, lock metrics, ...) without perturbing the signal path:



* **Detached (the default)**: an instrumented object holds a NULL `dp_tlm_t *`; every probe site is a single pointer load and a predicted-not-taken branch, and only at _event_ rate (per recovered symbol, per gain update) — never per input sample. Consumers who want literal zero can compile with `-DDP_TLM_DISABLE`, which turns the `DP_TLM()` probe macro into `((void) 0)`.
* **Attached**: each emit is a per-probe decimation check plus one 16-byte record written into a lock-free VM-mirrored SPSC ring ([**buffer/buffer.h**](buffer_8h.md)). The write never blocks and never allocates; on overrun the record is dropped and counted, so a slow (or absent) reader can never stall the DSP thread.




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
size_t n = dp_tlm_read (tlm, recs, 512);   // on the consumer side
dp_tlm_destroy (tlm);
```
 



    
## Public Types Documentation




### typedef dp\_tlm\_rec\_fits\_slot 

```C++
typedef char dp_tlm_rec_fits_slot[sizeof(dp_tlm_rec_t)==2 *sizeof(uint64_t) ? 1 :-1];
```




<hr>



### typedef dp\_tlm\_t 

_Telemetry context: probe registry + SPSC record ring._ 
```C++
typedef struct dp_tlm dp_tlm_t;
```



Public (not opaque) because the emit path is inline; treat the fields as read-only outside telemetry\_core.c and dp\_tlm\_emit. 


        

<hr>
## Public Functions Documentation




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


* `ring_records` Requested ring capacity in records. MUST be a power of 2. Sub-page requests are rounded up to the page minimum ([**buffer.h**](buffer_8h.md) semantics) — read the authoritative value back with [**dp\_tlm\_capacity()**](telemetry_8h.md#function-dp_tlm_capacity). 



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



### function dp\_tlm\_emitted 

_Records written for probe_ `id` _(post-decimation, post-drop)._
```C++
uint64_t dp_tlm_emitted (
    const dp_tlm_t * t,
    int id
) 
```




<hr>



### function dp\_tlm\_lookup 

_Looks up a probe id by name; DP\_ERR\_INVALID if unknown._ 
```C++
int dp_tlm_lookup (
    const dp_tlm_t * t,
    const char * name
) 
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





        

<hr>



### function dp\_tlm\_probe\_count 

_Number of registered probes._ 
```C++
size_t dp_tlm_probe_count (
    const dp_tlm_t * t
) 
```




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

_Drains up to_ `max_recs` _records into_`out` _. Non-blocking._
```C++
size_t dp_tlm_read (
    dp_tlm_t * t,
    dp_tlm_rec_t * out,
    size_t max_recs
) 
```



Consumer side of the SPSC ring: safe to call from a different thread than the producer. Returns immediately with whatever is available (possibly 0) — never spins.




**Returns:**

Number of records copied out. 





        

<hr>
## Public Static Functions Documentation




### function dp\_tlm\_emit 

_Records one scalar for probe_ `id` _. The hot-path primitive._
```C++
static inline void dp_tlm_emit (
    dp_tlm_t * t,
    int32_t id,
    double v
) 
```



Detached (`t` NULL) this is one branch — the entire disabled cost. Attached: bump the probe's decimation phase, and on the decim-th event write one 16-byte record (value narrowed to float, stamped with the context's current `now`). Never blocks, never allocates; on ring overrun the record is dropped and counted.


`id` must come from a successful [**dp\_tlm\_probe()**](telemetry_8h.md#function-dp_tlm_probe) on this context — an object's set\_telemetry fails the whole attach otherwise. 


        

<hr>



### function dp\_tlm\_set\_now 

_Stamps the sample index carried by subsequent records._ 
```C++
static inline void dp_tlm_set_now (
    dp_tlm_t * t,
    uint64_t n
) 
```



Call once per block from whoever owns the pipeline's sample clock (`dp_tlm_set_now (tlm, clk->n)`). NULL-safe so pipeline glue can call it unconditionally. 


        

<hr>
## Macro Definition Documentation





### define DP\_TLM 

_Probe-site wrapper around dp\_tlm\_emit()._ 
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
The documentation for this class was generated from the following file `native/inc/telemetry/telemetry.h`

