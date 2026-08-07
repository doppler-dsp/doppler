

# File timing\_core.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**timing**](dir_0a8cc616bc028a416e339204953e39da.md) **>** [**timing\_core.h**](timing__core_8h.md)

[Go to the source code of this file](timing__core_8h_source.md)



* `#include <stddef.h>`
* `#include <stdint.h>`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**dp\_sample\_clock\_t**](structdp__sample__clock__t.md) <br> |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  uint64\_t | [**dp\_mono\_ns**](#function-dp_mono_ns) (void) <br> |
|  uint64\_t | [**dp\_real\_ns**](#function-dp_real_ns) (void) <br> |
|  [**dp\_sample\_clock\_t**](structdp__sample__clock__t.md) \* | [**dp\_sample\_clock\_create**](#function-dp_sample_clock_create) (double fs, int resync) <br> |
|  void | [**dp\_sample\_clock\_destroy**](#function-dp_sample_clock_destroy) ([**dp\_sample\_clock\_t**](structdp__sample__clock__t.md) \* c) <br> |
|  void | [**dp\_sample\_clock\_init**](#function-dp_sample_clock_init) ([**dp\_sample\_clock\_t**](structdp__sample__clock__t.md) \* c, double fs, int resync) <br> |
|  double | [**dp\_sample\_clock\_pace**](#function-dp_sample_clock_pace) ([**dp\_sample\_clock\_t**](structdp__sample__clock__t.md) \* c, size\_t count) <br> |
|  void | [**dp\_sample\_clock\_reset**](#function-dp_sample_clock_reset) ([**dp\_sample\_clock\_t**](structdp__sample__clock__t.md) \* c) <br> |
|  void | [**dp\_sample\_clock\_resync**](#function-dp_sample_clock_resync) ([**dp\_sample\_clock\_t**](structdp__sample__clock__t.md) \* c) <br> |
|  uint64\_t | [**dp\_sample\_clock\_stamp**](#function-dp_sample_clock_stamp) (const [**dp\_sample\_clock\_t**](structdp__sample__clock__t.md) \* c) <br> |
|  uint64\_t | [**dp\_sample\_clock\_stamp\_at**](#function-dp_sample_clock_stamp_at) (const [**dp\_sample\_clock\_t**](structdp__sample__clock__t.md) \* c, uint64\_t n) <br> |
|  void | [**dp\_sample\_clock\_stats**](#function-dp_sample_clock_stats) (const [**dp\_sample\_clock\_t**](structdp__sample__clock__t.md) \* c, [**dp\_sample\_clock\_t**](structdp__sample__clock__t.md) \* out) <br> |
|  int | [**dp\_sample\_clock\_track**](#function-dp_sample_clock_track) ([**dp\_sample\_clock\_t**](structdp__sample__clock__t.md) \* c, uint64\_t observed\_timestamp\_ns, uint64\_t n\_at\_observation, uint64\_t tolerance\_ns) <br> |




























## Public Functions Documentation




### function dp\_mono\_ns 

```C++
uint64_t dp_mono_ns (
    void
) 
```



Current monotonic clock in ns (CLOCK\_MONOTONIC) — for pacing. 


        

<hr>



### function dp\_real\_ns 

```C++
uint64_t dp_real_ns (
    void
) 
```



Current wall-clock in ns since the UNIX epoch (CLOCK\_REALTIME). 


        

<hr>



### function dp\_sample\_clock\_create 

```C++
dp_sample_clock_t * dp_sample_clock_create (
    double fs,
    int resync
) 
```



Heap-allocate and initialise a clock for sample rate `fs` (Hz); see dp\_sample\_clock\_init for `resync`. Returns NULL on allocation failure. This is the opaque-handle constructor the generated realtime composer stream drives (`Composer.stream(realtime=fs)`): it owns a `void *clk` created here and freed by dp\_sample\_clock\_destroy. 


        

<hr>



### function dp\_sample\_clock\_destroy 

```C++
void dp_sample_clock_destroy (
    dp_sample_clock_t * c
) 
```



Free a clock from dp\_sample\_clock\_create (NULL-safe). 


        

<hr>



### function dp\_sample\_clock\_init 

```C++
void dp_sample_clock_init (
    dp_sample_clock_t * c,
    double fs,
    int resync
) 
```



Initialise `c` for sample rate `fs` (Hz), capturing both epochs now. If `resync` is nonzero, pace() re-anchors the timeline to "now" whenever it falls behind (absorbing the slip) instead of keeping the absolute schedule. 


        

<hr>



### function dp\_sample\_clock\_pace 

```C++
double dp_sample_clock_pace (
    dp_sample_clock_t * c,
    size_t count
) 
```



Advance by `count` samples and sleep until that block's deadline (`epoch + n/fs`). Returns the slack in seconds measured before sleeping: `>= 0` means early (and it slept that long); `< 0` means it arrived late — an underrun, which is counted (and the epoch re-anchored when `resync` is set), with no sleep. 


        

<hr>



### function dp\_sample\_clock\_reset 

```C++
void dp_sample_clock_reset (
    dp_sample_clock_t * c
) 
```



Re-capture both epochs and zero the counters — a fresh clock at n=0. 


        

<hr>



### function dp\_sample\_clock\_resync 

```C++
void dp_sample_clock_resync (
    dp_sample_clock_t * c
) 
```



Re-anchor the pacing epoch to "now" without clearing `n` or counters, dropping any accumulated lateness so future blocks pace forward from the present. (pace() does this automatically when `resync` is set.) 


        

<hr>



### function dp\_sample\_clock\_stamp 

```C++
uint64_t dp_sample_clock_stamp (
    const dp_sample_clock_t * c
) 
```



Ideal wall-clock timestamp (ns since the UNIX epoch) of the next sample to be produced — sample index `n`. Call it before pace() to tag the block you are about to emit, or after to tag the following block. Equivalent to `dp_sample_clock_stamp_at(c, c->n)`. 


        

<hr>



### function dp\_sample\_clock\_stamp\_at 

```C++
uint64_t dp_sample_clock_stamp_at (
    const dp_sample_clock_t * c,
    uint64_t n
) 
```



Ideal wall-clock timestamp (ns since the UNIX epoch) of an ARBITRARY sample index `n` — past, present, or future, not just the clock's own live position. The receive-side counterpart of [**dp\_sample\_clock\_stamp()**](timing__core_8h.md#function-dp_sample_clock_stamp): a block emitting several per-record outputs from one buffered input (e.g. several detections spanning different epochs from one streamed message) stamps each at its own historical sample offset instead of reusing the whole buffer's single arrival time. 


        

<hr>



### function dp\_sample\_clock\_stats 

```C++
void dp_sample_clock_stats (
    const dp_sample_clock_t * c,
    dp_sample_clock_t * out
) 
```




<hr>



### function dp\_sample\_clock\_track 

```C++
int dp_sample_clock_track (
    dp_sample_clock_t * c,
    uint64_t observed_timestamp_ns,
    uint64_t n_at_observation,
    uint64_t tolerance_ns
) 
```



Reconcile `c's` epoch\_real\_ns against one OBSERVED (timestamp, sample index) pair read off an incoming stream header — the receive-side dual of pace()'s resync: instead of sleeping toward a deadline, this adopts or corrects the epoch from ground truth the sender already stamped.


The FIRST call always adopts `observed_timestamp_ns` as the epoch (`has_anchor` starts false — a fresh clock has no real observation yet, so there is nothing to compare against). Every later call only re-anchors if the discrepancy between the observation and what the clock's current model predicts exceeds `tolerance_ns` (same step-correction semantics as pace()'s own resync, applied to tracking instead of sleeping) — this corrects accumulated epoch OFFSET only, it does not model sample-rate SKEW, exactly like pace()'s resync.


Rejects (no-op, returns 0) any observation with `n_at_observation` less than the clock's current `n` outright: a stale, out-of-order, or redelivered header must never walk the epoch backward. Never treat two reconciled observations as literal replay-safe state — always resync from an ARRIVING message, not a cached one.




**Returns:**

Nonzero if this call adopted or re-anchored the epoch; 0 if it was accepted as already consistent, or rejected as stale. 





        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/timing/timing_core.h`

