

# File timing\_core.h



[**FileList**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**timing**](dir_f312c7d1315f2596c2b71ef05d5d7b2b.md) **>** [**timing\_core.h**](timing__core_8h.md)

[Go to the source code of this file](timing__core_8h_source.md)

_Sample-clock pacing + timestamping._ [More...](#detailed-description)

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
|  void | [**dp\_sample\_clock\_set\_rate**](#function-dp_sample_clock_set_rate) ([**dp\_sample\_clock\_t**](structdp__sample__clock__t.md) \* c, double fs) <br> |
|  uint64\_t | [**dp\_sample\_clock\_stamp**](#function-dp_sample_clock_stamp) (const [**dp\_sample\_clock\_t**](structdp__sample__clock__t.md) \* c) <br> |
|  uint64\_t | [**dp\_sample\_clock\_stamp\_at**](#function-dp_sample_clock_stamp_at) (const [**dp\_sample\_clock\_t**](structdp__sample__clock__t.md) \* c, uint64\_t n) <br> |
|  void | [**dp\_sample\_clock\_stats**](#function-dp_sample_clock_stats) (const [**dp\_sample\_clock\_t**](structdp__sample__clock__t.md) \* c, [**dp\_sample\_clock\_t**](structdp__sample__clock__t.md) \* out) <br> |
|  int | [**dp\_sample\_clock\_track**](#function-dp_sample_clock_track) ([**dp\_sample\_clock\_t**](structdp__sample__clock__t.md) \* c, uint64\_t observed\_timestamp\_ns, uint64\_t n\_at\_observation, uint64\_t tolerance\_ns) <br> |




























## Detailed Description


A `dp_sample_clock_t` models an ideal sample clock running at `fs` Hz. It does two things off one shared, drift-free timeline anchored when the clock is created or reset:



* PACE a producer so each block leaves at its real-time deadline `epoch + n/fs` — mimicking a hardware sample clock feeding a DAC.
* STAMP a block with the ideal wall-clock time of its samples (`epoch_real + n/fs`), for reproducible capture metadata.




Because both derive from the cumulative sample count `n` against a fixed epoch — not from summed per-block sleeps — the schedule is drift-free: an over- or under-sleep on one block is corrected on the next. The residual error is per-block jitter bounded by the OS scheduler, which averages out.


Pacing has a platform split, inside timing\_core.c: POSIX reads `clock_gettime`; Linux sleeps each slice toward an absolute deadline with `clock_nanosleep`, and other POSIX systems (macOS) sleep each slice as a relative `nanosleep` and re-read the clock. Windows reads QueryPerformanceCounter and sleeps on a high-resolution waitable timer, falling back to `Sleep()` if that timer cannot be created. Either way the sleep is sliced at [**dp\_interrupt\_latency\_ms()**](dp__interrupt_8h.md#function-dp_interrupt_latency_ms) and returns early once [**dp\_interrupted()**](dp__interrupt_8h.md#function-dp_interrupted) is set, so a stop request ends a long pace.


A non-positive or NaN `fs` gives every sample a zero offset from the epoch: every stamp is the epoch, and pace() never sleeps  every call finds its deadline already passed and counts an underrun. 


    
## Public Functions Documentation




### function dp\_mono\_ns 

```C++
uint64_t dp_mono_ns (
    void
) 
```



Current monotonic clock in ns (CLOCK\_MONOTONIC) — for pacing.




**Returns:**

Nanoseconds on the monotonic clock (QueryPerformanceCounter on Windows); only differences between two readings mean anything. 





        

<hr>



### function dp\_real\_ns 

```C++
uint64_t dp_real_ns (
    void
) 
```



Current wall-clock in ns since the UNIX epoch (CLOCK\_REALTIME).




**Returns:**

Nanoseconds since 1970-01-01T00:00:00Z. 





        

<hr>



### function dp\_sample\_clock\_create 

```C++
dp_sample_clock_t * dp_sample_clock_create (
    double fs,
    int resync
) 
```



Heap-allocate and initialise a clock for sample rate `fs` (Hz); see dp\_sample\_clock\_init for `resync`. Returns NULL on allocation failure. This is the opaque-handle constructor the generated realtime composer stream drives (`Composer.stream(realtime=fs)`): it owns a `void *clk` created here and freed by dp\_sample\_clock\_destroy.




**Parameters:**


* `fs` Sample rate, Hz. 
* `resync` Nonzero to re-anchor on underrun. 



**Returns:**

A clock initialized as by [**dp\_sample\_clock\_init()**](timing__core_8h.md#function-dp_sample_clock_init), or NULL. 





        

<hr>



### function dp\_sample\_clock\_destroy 

```C++
void dp_sample_clock_destroy (
    dp_sample_clock_t * c
) 
```



Free a clock from dp\_sample\_clock\_create (NULL-safe).




**Parameters:**


* `c` Clock to free, or NULL. 




        

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




**Parameters:**


* `c` Clock to initialize; every field is overwritten, the counters and `n` zeroed and `has_anchor` cleared. 
* `fs` Sample rate, Hz. 
* `resync` Nonzero to re-anchor on underrun (see above). 




        

<hr>



### function dp\_sample\_clock\_pace 

```C++
double dp_sample_clock_pace (
    dp_sample_clock_t * c,
    size_t count
) 
```



Advance by `count` samples and sleep until that block's deadline (`epoch + n/fs`). Returns the slack in seconds measured before sleeping: `> 0` means early (and it slept that long); `<= 0` means it arrived late — an underrun, which is counted (and the epoch re-anchored when `resync` is set), with no sleep.




**Parameters:**


* `c` Clock to advance. 
* `count` Samples in the block being paced; `n` grows by this much before the deadline is computed. 



**Returns:**

Seconds from now to the deadline, read before any sleep. A deadline reached exactly (slack 0) also counts as an underrun. The sleep itself returns early if [**dp\_interrupted()**](dp__interrupt_8h.md#function-dp_interrupted) is set, so it may sleep less than a positive slack. 





        

<hr>



### function dp\_sample\_clock\_reset 

```C++
void dp_sample_clock_reset (
    dp_sample_clock_t * c
) 
```



Re-capture both epochs and zero the counters — a fresh clock at n=0.




**Parameters:**


* `c` Clock to reset; `fs` and `resync` are kept, and `has_anchor` is cleared. 




        

<hr>



### function dp\_sample\_clock\_resync 

```C++
void dp_sample_clock_resync (
    dp_sample_clock_t * c
) 
```



Re-anchor the pacing epoch to "now" without clearing `n` or counters, dropping any accumulated lateness so future blocks pace forward from the present. (pace() does this automatically when `resync` is set.)




**Parameters:**


* `c` Clock to re-anchor. A clock that is not behind its schedule is left unchanged. 




        

<hr>



### function dp\_sample\_clock\_set\_rate 

```C++
void dp_sample_clock_set_rate (
    dp_sample_clock_t * c,
    double fs
) 
```



Change `c's` rate to `fs` from the next sample on, keeping the schedule: the samples already paced keep their times, and the next one is due exactly when it was. The epochs move forward to the current position (`n/fs_old`) and `n` restarts at 0 at the new rate, so pace() and stamp() continue seamlessly; stamp\_at() of a sample BEFORE the change no longer applies. A rate equal to the current one changes nothing. This is how one clock paces a scene whose segments run at different rates (doppler#1733).



```C++
dp_sample_clock_t c;
dp_sample_clock_init (&c, 1e3, 0);
c.n = 500;                         // 0.5 s in, at 1 kHz
uint64_t due = dp_sample_clock_stamp (&c);
dp_sample_clock_set_rate (&c, 2e3);
int ok = c.n == 0 && c.fs == 2e3 && dp_sample_clock_stamp (&c) == due;
return ok ? 0 : 1;
```





**Parameters:**


* `c` Clock to retune. 
* `fs` New sample rate, Hz. 




        

<hr>



### function dp\_sample\_clock\_stamp 

```C++
uint64_t dp_sample_clock_stamp (
    const dp_sample_clock_t * c
) 
```



Ideal wall-clock timestamp (ns since the UNIX epoch) of the next sample to be produced — sample index `n`. Call it before pace() to tag the block you are about to emit, or after to tag the following block. Equivalent to `dp_sample_clock_stamp_at(c, c->n)`.




**Parameters:**


* `c` Clock to read. 



**Returns:**

`epoch_real_ns` plus `n/fs` seconds, in ns, rounded to the nearest ns. 





        

<hr>



### function dp\_sample\_clock\_stamp\_at 

```C++
uint64_t dp_sample_clock_stamp_at (
    const dp_sample_clock_t * c,
    uint64_t n
) 
```



Ideal wall-clock timestamp (ns since the UNIX epoch) of an ARBITRARY sample index `n` — past, present, or future, not just the clock's own live position. The receive-side counterpart of [**dp\_sample\_clock\_stamp()**](timing__core_8h.md#function-dp_sample_clock_stamp): a block emitting several per-record outputs from one buffered input (e.g. several detections spanning different epochs from one streamed message) stamps each at its own historical sample offset instead of reusing the whole buffer's single arrival time.




**Parameters:**


* `c` Clock to read. 
* `n` Sample index, counted on the clock's current timeline. 



**Returns:**

`epoch_real_ns` plus `n/fs` seconds, in ns, rounded to the nearest ns. 





        

<hr>



### function dp\_sample\_clock\_stats 

```C++
void dp_sample_clock_stats (
    const dp_sample_clock_t * c,
    dp_sample_clock_t * out
) 
```



A stats snapshot for the generated `SampleClock` handle (jm kind="handle"): the decoded-getter face wants one call that fills an out-struct, so this copies the (public) clock struct out. The handle constructs init-in-place via dp\_sample\_clock\_init above (jm#320 `init_fn`); jm owns that malloc/free.




**Parameters:**


* `c` Clock to read. 
* `out` Receives a copy of every field of `c`. 




        

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


An accepted observation, whether or not it re-anchors, also moves `n` to `n_at_observation`.




**Parameters:**


* `c` Clock to reconcile. 
* `observed_timestamp_ns` Wall-clock time, ns since the UNIX epoch, the sender stamped on sample `n_at_observation`. 
* `n_at_observation` Sample index that timestamp belongs to. 
* `tolerance_ns` Largest predicted-vs-observed gap, ns, accepted without re-anchoring. 



**Returns:**

Nonzero if this call adopted or re-anchored the epoch; 0 if it was accepted as already consistent, or rejected as stale. 





        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/timing/timing_core.h`

