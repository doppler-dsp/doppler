

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
|  void | [**dp\_sample\_clock\_init**](#function-dp_sample_clock_init) ([**dp\_sample\_clock\_t**](structdp__sample__clock__t.md) \* c, double fs, int resync) <br> |
|  double | [**dp\_sample\_clock\_pace**](#function-dp_sample_clock_pace) ([**dp\_sample\_clock\_t**](structdp__sample__clock__t.md) \* c, size\_t count) <br> |
|  void | [**dp\_sample\_clock\_reset**](#function-dp_sample_clock_reset) ([**dp\_sample\_clock\_t**](structdp__sample__clock__t.md) \* c) <br> |
|  void | [**dp\_sample\_clock\_resync**](#function-dp_sample_clock_resync) ([**dp\_sample\_clock\_t**](structdp__sample__clock__t.md) \* c) <br> |
|  uint64\_t | [**dp\_sample\_clock\_stamp**](#function-dp_sample_clock_stamp) (const [**dp\_sample\_clock\_t**](structdp__sample__clock__t.md) \* c) <br> |




























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



Ideal wall-clock timestamp (ns since the UNIX epoch) of the next sample to be produced — sample index `n`. Call it before pace() to tag the block you are about to emit, or after to tag the following block. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/timing/timing_core.h`

