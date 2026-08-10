

# File tlm\_recorder.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**telemetry**](dir_d4543964ddc0423cd91d16ab74a4089e.md) **>** [**tlm\_recorder.h**](tlm__recorder_8h.md)

[Go to the source code of this file](tlm__recorder_8h_source.md)

_Lossless capture: computed sizing, ping-pong staging, flush to file._ [More...](#detailed-description)

* `#include "telemetry/telemetry.h"`

















## Public Types

| Type | Name |
| ---: | :--- |
| typedef struct dp\_tlm\_recorder | [**dp\_tlm\_recorder\_t**](#typedef-dp_tlm_recorder_t)  <br> |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  size\_t | [**dp\_tlm\_block\_bound**](#function-dp_tlm_block_bound) (const [**dp\_tlm\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_t) \* t, size\_t block\_samples) <br>_Records one block can produce, at worst, for_ `t` _._ |
|  uint64\_t | [**dp\_tlm\_recorder\_count**](#function-dp_tlm_recorder_count) (const [**dp\_tlm\_recorder\_t**](tlm__recorder_8h.md#typedef-dp_tlm_recorder_t) \* r) <br>_Records captured so far (flushed + staged)._  |
|  [**dp\_tlm\_recorder\_t**](tlm__recorder_8h.md#typedef-dp_tlm_recorder_t) \* | [**dp\_tlm\_recorder\_create**](#function-dp_tlm_recorder_create) ([**dp\_tlm\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_t) \* t, const char \* path, size\_t block\_samples) <br>_Creates a recorder and RESIZES the context's ring to fit one block._  |
|  void | [**dp\_tlm\_recorder\_destroy**](#function-dp_tlm_recorder_destroy) ([**dp\_tlm\_recorder\_t**](tlm__recorder_8h.md#typedef-dp_tlm_recorder_t) \* r) <br>_Finishes if needed, then frees. NULL-safe._  |
|  uint64\_t | [**dp\_tlm\_recorder\_dropped**](#function-dp_tlm_recorder_dropped) (const [**dp\_tlm\_recorder\_t**](tlm__recorder_8h.md#typedef-dp_tlm_recorder_t) \* r) <br>_Records lost. Zero by construction — non-zero is a BUG here._  |
|  int | [**dp\_tlm\_recorder\_finish**](#function-dp_tlm_recorder_finish) ([**dp\_tlm\_recorder\_t**](tlm__recorder_8h.md#typedef-dp_tlm_recorder_t) \* r) <br>_Final drain, flush and close. Joins the flusher._  |
|  size\_t | [**dp\_tlm\_recorder\_ring\_records**](#function-dp_tlm_recorder_ring_records) (const [**dp\_tlm\_recorder\_t**](tlm__recorder_8h.md#typedef-dp_tlm_recorder_t) \* r) <br>_Ring capacity the recorder computed, in records._  |
|  int | [**dp\_tlm\_recorder\_tick**](#function-dp_tlm_recorder_tick) ([**dp\_tlm\_recorder\_t**](tlm__recorder_8h.md#typedef-dp_tlm_recorder_t) \* r) <br>_Drains one block and hands it to the flusher. Producer thread._  |




























## Detailed Description


## Why this exists



The ring drops on overrun. That is correct for the _ring_ — `dp_tlm_emit` runs in the DSP hot loop, so it must never block and never allocate, and a bounded lock-free buffer with a drop counter is the only structure that honours both. But "correct for the ring" became "the caller's problem" in practice: every consumer hand-rolled a drain loop, guessed a ring size, and discovered loss (if ever) from a counter nobody read.


A capture with a hole is not a smaller capture, it is a wrong one. So the recorder makes loss structurally impossible rather than merely counted, and takes the sizing decision away from the caller entirely.



## The three pieces



**1. The size is COMPUTED, not guessed.** A probe emits at most once per event and events are at most one per input sample, so one block's worst case is bounded and knowable:  Both terms are in hand: probes are registered before the producer starts (the registry is setup-time by contract), and `block_samples` is the block the caller is already stepping with. The ring is sized to that ceiling, so it cannot overflow _within_ a block.


**2. The drain happens at the BLOCK BOUNDARY, on the producer thread.** That is the one instant the producer is quiescent by construction — between `steps()` calls — so a synchronous drain there needs the ring to hold one block, never a whole capture. A background thread cannot offer this: it is best-effort by nature, and a producer emitting in a tight loop outruns any consumer, which is how the earlier threaded draft lost records.


**3. Staging PING-PONGS, and the boundary may wait.** `tick()` drains the ring into one half of a staging pair and hands the full half to the flusher thread, which writes it out while the producer fills the other. If the flusher is still busy when the next swap comes, `tick()` WAITS for it. Waiting at a block boundary is legal — it is not the hot loop — and it is what converts "drop" into "back-pressure". The emit path is untouched and still never blocks.



## Contract




* `tick()` runs on the PRODUCER thread, once per block, after stepping.
* The recorder is the ring's only consumer; do not also call `dp_tlm_read()` on the same context while one is attached.
* Probes must all be registered before `create()`, since the sizing depends on the count (this is already the registry's contract).





```C++
dp_tlm_recorder_t *r =
    dp_tlm_recorder_create (tlm, "cap.tlm16", 256);  // block = 256
for (size_t i = 0; i < n; i += 256) {
  dp_tlm_set_now (tlm, i);
  obj_steps (obj, x + i, 256);
  dp_tlm_recorder_tick (r);        // drain + hand off; never drops
}
dp_tlm_recorder_finish (r);        // final flush, close, join
```
 



    
## Public Types Documentation




### typedef dp\_tlm\_recorder\_t 

```C++
typedef struct dp_tlm_recorder dp_tlm_recorder_t;
```



Opaque recorder: computed ring, ping-pong staging, flusher thread. 


        

<hr>
## Public Functions Documentation




### function dp\_tlm\_block\_bound 

_Records one block can produce, at worst, for_ `t` _._
```C++
size_t dp_tlm_block_bound (
    const dp_tlm_t * t,
    size_t block_samples
) 
```



`n_probes * block_samples / min_decim`, where `min_decim` is the smallest decimation across the registered probes (the fastest emitter sets the bound). Exposed because it is the number that replaces the caller's guess — a caller who wants to know what a capture will cost can ask.




**Parameters:**


* `t` Context, with every probe already registered. 
* `block_samples` Samples per step() call. 



**Returns:**

Upper bound in records; 0 if `t` is NULL or has no probes. 





        

<hr>



### function dp\_tlm\_recorder\_count 

_Records captured so far (flushed + staged)._ 
```C++
uint64_t dp_tlm_recorder_count (
    const dp_tlm_recorder_t * r
) 
```




<hr>



### function dp\_tlm\_recorder\_create 

_Creates a recorder and RESIZES the context's ring to fit one block._ 
```C++
dp_tlm_recorder_t * dp_tlm_recorder_create (
    dp_tlm_t * t,
    const char * path,
    size_t block_samples
) 
```



The ring is replaced with one sized from [**dp\_tlm\_block\_bound()**](tlm__recorder_8h.md#function-dp_tlm_block_bound), which is safe here and only here: no producer is running yet, so nothing is mid-write. Whatever size the context was created with is irrelevant afterwards — that is the point, the caller stops having to pick one.




**Parameters:**


* `t` Context. Must outlive the recorder. All probes registered. 
* `path` Where to flush records, or NULL to keep the capture in memory only (it still never drops; it just grows). 
* `block_samples` Samples per step() call — the term the bound needs. 



**Returns:**

New recorder, or NULL on bad arguments / allocation / open failure. 





        

<hr>



### function dp\_tlm\_recorder\_destroy 

_Finishes if needed, then frees. NULL-safe._ 
```C++
void dp_tlm_recorder_destroy (
    dp_tlm_recorder_t * r
) 
```




<hr>



### function dp\_tlm\_recorder\_dropped 

_Records lost. Zero by construction — non-zero is a BUG here._ 
```C++
uint64_t dp_tlm_recorder_dropped (
    const dp_tlm_recorder_t * r
) 
```



Kept as an assertion surface rather than an expected outcome: the sizing and the boundary back-pressure between them mean a drop cannot happen in the supported usage, so a caller seeing one has found a defect, not a tuning problem. 


        

<hr>



### function dp\_tlm\_recorder\_finish 

_Final drain, flush and close. Joins the flusher._ 
```C++
int dp_tlm_recorder_finish (
    dp_tlm_recorder_t * r
) 
```



Sweeps anything the last tick left, writes it, and closes the file. After this the capture on disk is complete.




**Returns:**

[**DP\_OK**](clib__common_8h.md#define-dp_ok), or [**DP\_ERR\_INVALID**](clib__common_8h.md#define-dp_err_invalid) on NULL / a write error. 





        

<hr>



### function dp\_tlm\_recorder\_ring\_records 

_Ring capacity the recorder computed, in records._ 
```C++
size_t dp_tlm_recorder_ring_records (
    const dp_tlm_recorder_t * r
) 
```




<hr>



### function dp\_tlm\_recorder\_tick 

_Drains one block and hands it to the flusher. Producer thread._ 
```C++
int dp_tlm_recorder_tick (
    dp_tlm_recorder_t * r
) 
```



Call once per block, after stepping. Blocks only if the flusher has not finished the previous half — back-pressure at a boundary, never in the emit path.




**Returns:**

[**DP\_OK**](clib__common_8h.md#define-dp_ok), or [**DP\_ERR\_INVALID**](clib__common_8h.md#define-dp_err_invalid) on NULL / a write error. 





        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/telemetry/tlm_recorder.h`

