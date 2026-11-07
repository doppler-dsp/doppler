

# File correlator\_private.h



[**FileList**](files.md) **>** [**correlator**](dir_296d11a08af6c23b555f7cd87cd10cbd.md) **>** [**correlator\_private.h**](correlator__private_8h.md)

[Go to the source code of this file](correlator__private_8h_source.md)

_The correlator's detection layer: the ring, the noise estimate, the one peak picker and the framed push._ [More...](#detailed-description)

* `#include <math.h>`
* `#include <stdint.h>`
* `#include <stdlib.h>`
* `#include <string.h>`
* `#include "doppler/correlator/correlator_core.h"`
* `#include "doppler/f32_buffer/f32_buffer_core.h"`
* `#include "doppler/util/util_core.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**search\_extra\_t**](structsearch__extra__t.md) <br>_Per-object extra header for an engine's cross-call state._  |


## Public Types

| Type | Name |
| ---: | :--- |
| typedef int(\* | [**det\_frame\_step\_fn**](#typedef-det_frame_step_fn)  <br>_One frame's work for det\_framed\_push(): correlate_ `frame` _and, on a dump that passes the gate, write ONE result at index_`slot` _of_`result` _. Returns 1 if it wrote one, 0 if not_ _never more._ |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  size\_t | [**dp\_\_search\_consumed**](#function-dp__search_consumed) (const [**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* st) <br>[_**dp\_correlator\_consumed()**_](correlator__core_8h.md#function-dp_correlator_consumed) _on a search flavor._ |
|  void | [**dp\_\_search\_destroy**](#function-dp__search_destroy) ([**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* st) <br>[_**dp\_correlator\_destroy()**_](correlator__core_8h.md#function-dp_correlator_destroy) _on a search flavor._ |
|  void | [**dp\_\_search\_get\_state**](#function-dp__search_get_state) (const [**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* st, void \* blob) <br>[_**dp\_correlator\_get\_state()**_](correlator__core_8h.md#function-dp_correlator_get_state) _on a search flavor._ |
|  size\_t | [**dp\_\_search\_push**](#function-dp__search_push) ([**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* st, const float \_Complex \* x, size\_t n\_in, [**dp\_correlator\_hit\_t**](structdp__correlator__hit__t.md) \* out, size\_t max\_out) <br>[_**dp\_correlator\_push()**_](correlator__core_8h.md#function-dp_correlator_push) _on a search flavor._ |
|  void | [**dp\_\_search\_reset**](#function-dp__search_reset) ([**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* st) <br>[_**dp\_correlator\_reset()**_](correlator__core_8h.md#function-dp_correlator_reset) _on a search flavor._ |
|  int | [**dp\_\_search\_set\_state**](#function-dp__search_set_state) ([**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* st, const void \* blob) <br>[_**dp\_correlator\_set\_state()**_](correlator__core_8h.md#function-dp_correlator_set_state) _on a search flavor._ |
|  size\_t | [**dp\_\_search\_state\_bytes**](#function-dp__search_state_bytes) (const [**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* st) <br>[_**dp\_correlator\_state\_bytes()**_](correlator__core_8h.md#function-dp_correlator_state_bytes) _on a search flavor._ |
|  int | [**dp\_\_search\_state\_check**](#function-dp__search_state_check) (const [**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* st, const void \* blob) <br>[_**dp\_correlator\_state\_check()**_](correlator__core_8h.md#function-dp_correlator_state_check) _on a search flavor._ |


## Public Static Functions

| Type | Name |
| ---: | :--- |
|  int | [**det\_cmp\_f32\_asc**](#function-det_cmp_f32_asc) (const void \* a, const void \* b) <br> |
|  size\_t | [**det\_framed\_push**](#function-det_framed_push) (dp\_f32\_framer\_t \* fr, const float \_Complex \* in, size\_t n\_in, void \* result, size\_t max\_results, [**det\_frame\_step\_fn**](correlator__private_8h.md#typedef-det_frame_step_fn) step, void \* obj, size\_t \* consumed) <br>_The detectors' push: any chunk in through the ring's framer, frames of_ `n` _at hop_`n` _out through_`step` _, never a lost input._ |
|  float | [**det\_noise\_chunk**](#function-det_noise_chunk) (const float \* mag, size\_t lo, size\_t hi, [**det\_noise\_mode\_t**](correlator__core_8h.md#enum-det_noise_mode_t) mode) <br>_The per-chunk noise aggregates_  _MEAN, MIN, MAX_ _over bins &#91;lo, hi&#93;, needing no scratch buffer._ |
|  float | [**det\_noise\_estimate**](#function-det_noise_estimate) (const float \* mag, size\_t lo, size\_t hi, float \* scratch, [**det\_noise\_mode\_t**](correlator__core_8h.md#enum-det_noise_mode_t) mode) <br>_Aggregate \|corr\| over bins &#91;lo, hi&#93; using the selected mode._  |
|  size\_t | [**det\_peak\_list**](#function-det_peak_list) (const float \* surf, size\_t ny, size\_t nx, float gate, size\_t excl\_rows, size\_t excl\_cols, uint8\_t \* mask, [**det\_peak\_t**](structdet__peak__t.md) \* out, size\_t max\_peaks) <br>_The maximum of a surface, iterated with exclusion zones: every peak above a gate, strongest first, at most_ `max_peaks` _of them._ |
|  size\_t | [**det\_peak\_scan**](#function-det_peak_scan) (const float \* surf, const uint8\_t \* mask, size\_t k0, size\_t k1) <br>_One scan of det\_peak\_list(): the first maximum of_ `surf` _over the cells_`[k0, k1)` _that_`mask` _leaves as candidates._ |
|  void | [**det\_peak\_zone**](#function-det_peak_zone) (uint8\_t \* mask, size\_t ny, size\_t nx, size\_t r, size\_t c, size\_t excl\_rows, size\_t excl\_cols) <br>_The exclusion zone of a pick at_ `(r, c)` _, marked into_`mask` _:_`excl_rows` _either side along the rows and_`excl_cols` _along the columns, CIRCULAR on both axes (an FFT bin axis by a circular correlation lag axis), each half-width clamped to half the axis._ |
|  dp\_f32\_t \* | [**det\_ring\_create**](#function-det_ring_create) (size\_t cap\_min) <br> |
|  int | [**search\_rate\_better**](#function-search_rate_better) (size\_t h, float stat, float best) <br>_Does hypothesis_ `h's` _statistic_`stat` _replace the best so far,_`best` _? The rule the rate search picks its winner by._ |
|  size\_t | [**search\_rate\_count\_at**](#function-search_rate_count_at) (size\_t rate\_hyps, size\_t D) <br> |
|  size\_t | [**search\_resolve\_tile\_alias**](#function-search_resolve_tile_alias) (const [**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* st, size\_t row, size\_t col) <br> |


























## Detailed Description


Not part of the public API. The correlator's detector flavors and the acquisition engine include it (docs/design/correlator.md section 4: the stream and detection layers, once). 


    
## Public Types Documentation




### typedef det\_frame\_step\_fn 

_One frame's work for det\_framed\_push(): correlate_ `frame` _and, on a dump that passes the gate, write ONE result at index_`slot` _of_`result` _. Returns 1 if it wrote one, 0 if not_ _never more._
```C++
typedef int(* det_frame_step_fn) (void *obj, const float _Complex *frame, void *result, size_t slot);
```




<hr>
## Public Functions Documentation




### function dp\_\_search\_consumed 

[_**dp\_correlator\_consumed()**_](correlator__core_8h.md#function-dp_correlator_consumed) _on a search flavor._
```C++
size_t dp__search_consumed (
    const dp_correlator_state_t * st
) 
```





**Parameters:**


* `st` A search flavor (non-NULL). 



**Returns:**

Samples the last push took. 





        

<hr>



### function dp\_\_search\_destroy 

[_**dp\_correlator\_destroy()**_](correlator__core_8h.md#function-dp_correlator_destroy) _on a search flavor._
```C++
void dp__search_destroy (
    dp_correlator_state_t * st
) 
```





**Parameters:**


* `st` A search flavor; may be NULL. 




        

<hr>



### function dp\_\_search\_get\_state 

[_**dp\_correlator\_get\_state()**_](correlator__core_8h.md#function-dp_correlator_get_state) _on a search flavor._
```C++
void dp__search_get_state (
    const dp_correlator_state_t * st,
    void * blob
) 
```





**Parameters:**


* `st` A search flavor (non-NULL). 
* `blob` Destination, [**dp\_\_search\_state\_bytes()**](correlator__private_8h.md#function-dp__search_state_bytes) bytes. 




        

<hr>



### function dp\_\_search\_push 

[_**dp\_correlator\_push()**_](correlator__core_8h.md#function-dp_correlator_push) _on a search flavor._
```C++
size_t dp__search_push (
    dp_correlator_state_t * st,
    const float _Complex * x,
    size_t n_in,
    dp_correlator_hit_t * out,
    size_t max_out
) 
```





**Parameters:**


* `st` A search flavor (non-NULL). 
* `x` Input, `n_in` complex samples. 
* `n_in` Samples in `x`. 
* `out` Hits, room for `max_out`. 
* `max_out` Capacity of `out`. 



**Returns:**

Hits written. 





        

<hr>



### function dp\_\_search\_reset 

[_**dp\_correlator\_reset()**_](correlator__core_8h.md#function-dp_correlator_reset) _on a search flavor._
```C++
void dp__search_reset (
    dp_correlator_state_t * st
) 
```





**Parameters:**


* `st` A search flavor (non-NULL). 




        

<hr>



### function dp\_\_search\_set\_state 

[_**dp\_correlator\_set\_state()**_](correlator__core_8h.md#function-dp_correlator_set_state) _on a search flavor._
```C++
int dp__search_set_state (
    dp_correlator_state_t * st,
    const void * blob
) 
```





**Parameters:**


* `st` A search flavor (non-NULL). 
* `blob` Source blob. 



**Returns:**

DP\_OK, or DP\_ERR\_INVALID with `st` unchanged. 





        

<hr>



### function dp\_\_search\_state\_bytes 

[_**dp\_correlator\_state\_bytes()**_](correlator__core_8h.md#function-dp_correlator_state_bytes) _on a search flavor._
```C++
size_t dp__search_state_bytes (
    const dp_correlator_state_t * st
) 
```





**Parameters:**


* `st` A search flavor (non-NULL). 



**Returns:**

The blob size, bytes. 





        

<hr>



### function dp\_\_search\_state\_check 

[_**dp\_correlator\_state\_check()**_](correlator__core_8h.md#function-dp_correlator_state_check) _on a search flavor._
```C++
int dp__search_state_check (
    const dp_correlator_state_t * st,
    const void * blob
) 
```





**Parameters:**


* `st` A search flavor (non-NULL). 
* `blob` Candidate blob. 



**Returns:**

DP\_OK or DP\_ERR\_INVALID; `st` is never written. 





        

<hr>
## Public Static Functions Documentation




### function det\_cmp\_f32\_asc 

```C++
static int det_cmp_f32_asc (
    const void * a,
    const void * b
) 
```




<hr>



### function det\_framed\_push 

_The detectors' push: any chunk in through the ring's framer, frames of_ `n` _at hop_`n` _out through_`step` _, never a lost input._
```C++
static inline size_t det_framed_push (
    dp_f32_framer_t * fr,
    const float _Complex * in,
    size_t n_in,
    void * result,
    size_t max_results,
    det_frame_step_fn step,
    void * obj,
    size_t * consumed
) 
```



[**dp\_correlator\_push()**](correlator__core_8h.md#function-dp_correlator_push) is this, with the detector flavors' step. Each frame yields at most one result, so the framer is fed only what completes as many WHOLE frames as `result` has room for, and no partial frame past them  dp\_f32\_framer\_feed\_frames(), the rule acq shares  and every frame fed is drained before the next feed. That makes a batch exact rather than an estimate: a batch can never write past the room (each frame takes at most one slot) or strand a whole frame in the framer (all are drained). Break either and the push overfills `result` or strands frames; acq breaks the first (a dump reports several peaks), which is why it has its own drain.


Once `result` is full  and with no room it is full from the start  the push takes the rest of its input only if the rest completes no frame (dp\_f32\_framer\_feed\_carry()): that rest is then the carry, taken without loss. Otherwise it takes nothing more, so a push that stops short of its input stops on a frame boundary, never part-way into a frame it cannot report. A caller that cannot resume (Python: one call per push) loses only what a push leaves, and stays frame-aligned when that is a whole number of frames. A resume loop needs room for at least one: with none, a push takes nothing that completes a frame. Input that runs out mid-frame is the carry, held for the next call.




**Parameters:**


* `fr` The object's framer. 
* `in` Input samples. 
* `n_in` Samples in `in`. 
* `result` The caller's results, handed to `step`. 
* `max_results` Room in `result`. 
* `step` The object's per-frame work. 
* `obj` Handed to `step`. 
* `consumed` Set to the samples of `in` this push took. 



**Returns:**

Results written. 





        

<hr>



### function det\_noise\_chunk 

_The per-chunk noise aggregates_  _MEAN, MIN, MAX_ _over bins &#91;lo, hi&#93;, needing no scratch buffer._
```C++
static inline float det_noise_chunk (
    const float * mag,
    size_t lo,
    size_t hi,
    det_noise_mode_t mode
) 
```



MEDIAN has no per-chunk form (it must sort the whole range), so it is not computed here and returns 0; use det\_noise\_estimate() for it. A caller that works a chunk at a time (acq's parallel tiles) calls THIS, so it cannot even ask for a median without a scratch buffer: the invariant is in the signature, not in a NULL passed to something that would dereference it.




**Parameters:**


* `mag` Magnitude vector (length &gt;= hi+1). 
* `lo` First bin, inclusive. 
* `hi` Last bin, inclusive. 
* `mode` DET\_NOISE\_MEAN, DET\_NOISE\_MIN or DET\_NOISE\_MAX. 



**Returns:**

The aggregate; 0 if lo &gt; hi or mode is DET\_NOISE\_MEDIAN. 





        

<hr>



### function det\_noise\_estimate 

_Aggregate \|corr\| over bins &#91;lo, hi&#93; using the selected mode._ 
```C++
static inline float det_noise_estimate (
    const float * mag,
    size_t lo,
    size_t hi,
    float * scratch,
    det_noise_mode_t mode
) 
```



Returns 0 if lo &gt; hi (empty range) — the caller maps that to test\_stat=0.




**Parameters:**


* `mag` Magnitude vector (length &gt;= hi+1). 
* `lo` First bin, inclusive. 
* `hi` Last bin, inclusive. 
* `scratch` Caller-allocated buffer of length &gt;= (hi-lo+1) floats; used only for DET\_NOISE\_MEDIAN (avoids a heap alloc per push). 
* `mode` Aggregation mode. 



**Returns:**

Aggregated noise estimate, or 0 if lo &gt; hi. 





        

<hr>



### function det\_peak\_list 

_The maximum of a surface, iterated with exclusion zones: every peak above a gate, strongest first, at most_ `max_peaks` _of them._
```C++
static inline size_t det_peak_list (
    const float * surf,
    size_t ny,
    size_t nx,
    float gate,
    size_t excl_rows,
    size_t excl_cols,
    uint8_t * mask,
    det_peak_t * out,
    size_t max_peaks
) 
```



The one argmax under both detectors (docs/design/async-dsss-receiver.md §7.1). Each pick is the largest unmasked cell; if it is not above `gate` the list ends there (the gate is `eta` in the surface's own units, so a second peak is another draw from the same cells against the same union bound  the threshold does not change with the list). A pick's zone  `excl_rows` either side along the rows and `excl_cols` along the columns, CIRCULAR on both axes, since every surface this serves is an FFT bin axis by a circular correlation lag axis  is masked so the emitter just reported cannot be reported again from its own shoulders; outside the zone a second emitter has its own maximum. The zone is therefore the detector's resolution, and it is the caller's to size from the code and the dwell (one Doppler bin by one chip: the main lobe's first nulls).


`mask` is the caller's, `ny * nx` bytes, initialised by the caller: 0 for a candidate cell, non-zero for one that is never a candidate (a Doppler band the engine does not search). On return every listed peak's zone is marked as well. Nothing here allocates, and the cost is `max_peaks` scans of the surface plus the zones  the duration rule of §5.1.


\*\*`mask` may be NULL when `max_peaks` is 1.\*\* The mask exists to carry a pick's zone to the next pick; with one pick there is no next, and every cell is a candidate. The call is then the classic detector's own loop  one pass, one compare per cell, nothing written  rather than a mask cleared over the surface and read back once per cell for a zone that is never applied. Measured (doppler#1208): the masked form at one peak cost `detector2d::push` 22-43%. A NULL mask with `max_peaks > 1` is a caller error and lists one peak.




**Parameters:**


* `surf` The surface, row-major `ny x nx`. 
* `ny` Its geometry. 
* `gate` A peak must exceed this (strictly) to be listed. 
* `excl_rows` Zone half-width along rows (0 = the row alone). 
* `excl_cols` Zone half-width along columns (0 = the column alone). 
* `mask` `ny * nx` bytes, 0 = candidate; updated in place. 
* `out` Receives up to `max_peaks` peaks, strongest first. 
* `max_peaks` Capacity of `out`. 



**Returns:**

Peaks listed (0 when nothing exceeds the gate). 





        

<hr>



### function det\_peak\_scan 

_One scan of det\_peak\_list(): the first maximum of_ `surf` _over the cells_`[k0, k1)` _that_`mask` _leaves as candidates._
```C++
static size_t det_peak_scan (
    const float * surf,
    const uint8_t * mask,
    size_t k0,
    size_t k1
) 
```



Returns the cell, or `k1` when no cell in the range is a candidate. A NaN cell is never a candidate (docs/design/correlator.md, U2), on either path: a range of NaNs returns `k1`, as a fully masked one does. The pick is the FIRST maximum (strict `>`), so a caller that cuts the surface into chunks, scans each, and merges the chunks' picks in order with the same strict `>` makes exactly the pick one scan over the whole surface makes  which is what lets the acquisition engine run the scan per tile on its pool and merge serially, bit-identical at any thread count (doppler#1243). `mask` may be NULL: every cell is then a candidate, and the loop is the plain argmax on purpose  a four-lane unrolled form runs 4x faster in isolation but moves detector2d::push by nothing measurable (doppler#1208), so the simple one stays.


With a mask the surface can be large (an acquisition dwell over a wide Doppler span is millions of cells, scanned once a pick), and there the plain loop was 37% of a full-span AsyncDsssPool push (30.4 of 81.6 ms). So the masked scan is split into blocks: a lane-wise pass finds each block's largest candidate, and the plain loop, seeded with the pick so far, runs only on a block whose largest candidate beats it. The pick is the plain loop's, NaN cells included; test\_correlator\_detector\_2d.c holds it to that loop over multi-block surfaces. `isnan` is reliable here because every doppler build passes -fno-finite-math-only (CMakeLists.txt, with its canary); a consumer of this installed header under a bare -ffast-math loses the NaN rule. 


        

<hr>



### function det\_peak\_zone 

_The exclusion zone of a pick at_ `(r, c)` _, marked into_`mask` _:_`excl_rows` _either side along the rows and_`excl_cols` _along the columns, CIRCULAR on both axes (an FFT bin axis by a circular correlation lag axis), each half-width clamped to half the axis._
```C++
static void det_peak_zone (
    uint8_t * mask,
    size_t ny,
    size_t nx,
    size_t r,
    size_t c,
    size_t excl_rows,
    size_t excl_cols
) 
```




<hr>



### function det\_ring\_create 

```C++
static inline dp_f32_t * det_ring_create (
    size_t cap_min
) 
```




<hr>



### function search\_rate\_better 

_Does hypothesis_ `h's` _statistic_`stat` _replace the best so far,_`best` _? The rule the rate search picks its winner by._
```C++
static inline int search_rate_better (
    size_t h,
    float stat,
    float best
) 
```



Hypothesis 0 always wins the first place, whatever it reads: a dwell whose every statistic is NaN (a power that overflows float is inf/inf) would otherwise fail `stat > best` at every hypothesis, pick none, and leave the search restoring the PREVIOUS dwell's winning surface. After it, a hypothesis must beat the best strictly, so a NaN never displaces one and ties keep the earlier.


Unit-tested as a rule on NaN / inf / finite values, because the end-to-end route to a NaN statistic (a float power overflowing) depends on the compiler's code generation and differs between Release and Debug. 


        

<hr>



### function search\_rate\_count\_at 

```C++
static inline size_t search_rate_count_at (
    size_t rate_hyps,
    size_t D
) 
```




<hr>



### function search\_resolve\_tile\_alias 

```C++
static inline size_t search_resolve_tile_alias (
    const dp_correlator_state_t * st,
    size_t row,
    size_t col
) 
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/correlator/correlator_private.h`

