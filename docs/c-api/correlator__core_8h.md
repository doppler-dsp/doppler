

# File correlator\_core.h



[**FileList**](files.md) **>** [**correlator**](dir_296d11a08af6c23b555f7cd87cd10cbd.md) **>** [**correlator\_core.h**](correlator__core_8h.md)

[Go to the source code of this file](correlator__core_8h_source.md)

_The one FFT cross-correlator, with coherent integrate-and-dump._ [More...](#detailed-description)

* `#include "doppler/clib_common.h"`
* `#include "doppler/dp_parallel.h"`
* `#include "doppler/dp_state.h"`
* `#include "doppler/f32_buffer/f32_buffer_core.h"`
* `#include "doppler/fft/fft_core.h"`
* `#include "doppler/fft2d/fft2d_core.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**det\_peak\_t**](structdet__peak__t.md) <br>_One listed peak of a surface: a cell and its value, in the surface's own units (a magnitude on a coherent surface, a power on a non-coherent one). What det\_peak\_list() (_ [_**correlator\_private.h**_](correlator__private_8h.md) _) returns, and what the acquisition engine keeps per dwell._ |
| struct | [**dp\_correlator\_fold\_t**](structdp__correlator__fold__t.md) <br>_A C reducer: called once per map, in h order, by_ [_**dp\_correlator\_execute\_fold()**_](correlator__core_8h.md#function-dp_correlator_execute_fold) _._ |
| struct | [**dp\_correlator\_hit\_t**](structdp__correlator__hit__t.md) <br>_One detection, from either detector flavor's_ [_**dp\_correlator\_push()**_](correlator__core_8h.md#function-dp_correlator_push) _._ |
| struct | [**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) <br>_Correlator state, every flavor._  |


## Public Types

| Type | Name |
| ---: | :--- |
| enum  | [**det\_noise\_mode\_t**](#enum-det_noise_mode_t)  <br>_How a detector flavor aggregates the noise floor from the correlation magnitudes over the flat bins noise\_lo to noise\_hi, inclusive._  |
| enum  | [**dp\_correlator\_flavor\_t**](#enum-dp_correlator_flavor_t)  <br>_Which constructor built a correlator._  |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  size\_t | [**dp\_correlator\_bank\_detect**](#function-dp_correlator_bank_detect) (const [**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state, const float \_Complex \* x, size\_t x\_len, size\_t L\_d, size\_t L\_r, int g\_min, int g\_max, float t\_candidate, float t\_confirm, const float \_Complex \* rates, size\_t rates\_len, float \* out, int32\_t \* out1, uint32\_t \* out2, uint32\_t \* out3, uint32\_t \* out4, size\_t max\_out) <br>_Detect on a coarse carrier grid, refine each candidate at its own cell on a fine one, and keep what confirms._  |
|  size\_t | [**dp\_correlator\_bank\_detect\_max\_out**](#function-dp_correlator_bank_detect_max_out) ([**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state, size\_t x\_len) <br>_Capacity_ [_**dp\_correlator\_bank\_detect()**_](correlator__core_8h.md#function-dp_correlator_bank_detect) _needs: one record per map cell._ |
|  size\_t | [**dp\_correlator\_bank\_fold**](#function-dp_correlator_bank_fold) (const [**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state, const float \_Complex \* x, size\_t x\_len, size\_t L, int g\_min, int g\_max, const float \_Complex \* rates, size\_t rates\_len, const [**dp\_correlator\_fold\_t**](structdp__correlator__fold__t.md) \* fold) <br>_Run one frame through a bank of carrier and rate hypotheses and hand each map to a reducer._  |
|  size\_t | [**dp\_correlator\_bank\_power\_max**](#function-dp_correlator_bank_power_max) (const [**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state, const float \_Complex \* x, size\_t x\_len, size\_t L, int g\_min, int g\_max, const float \_Complex \* rates, size\_t rates\_len, float \* out, uint32\_t \* out1, size\_t max\_out) <br>_The strongest power at every cell over a bank, and which hypothesis it came from._  |
|  size\_t | [**dp\_correlator\_bank\_power\_max\_max\_out**](#function-dp_correlator_bank_power_max_max_out) ([**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state, size\_t x\_len) <br>_Capacity_ [_**dp\_correlator\_bank\_power\_max()**_](correlator__core_8h.md#function-dp_correlator_bank_power_max) _needs: one map._ |
|  size\_t | [**dp\_correlator\_consumed**](#function-dp_correlator_consumed) (const [**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state) <br>_Input samples the last_ [_**dp\_correlator\_push()**_](correlator__core_8h.md#function-dp_correlator_push) _took._ |
|  [**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* | [**dp\_correlator\_create\_1d**](#function-dp_correlator_create_1d) (const float \_Complex \* ref, size\_t ref\_len, size\_t dwell, int nthreads, size\_t n\_out) <br>_Allocate a 1-D FFT correlator with coherent integrate-and-dump. Pre-computes conj(FFT(ref)) once at construction so each execute() call costs only two FFTs and n complex multiplies._ `ref` _may be freed after this returns. With_`dwell` _== 1 every call produces output; with larger values the accumulator absorbs_`dwell` _frames before dumping._ |
|  [**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* | [**dp\_correlator\_create\_2d**](#function-dp_correlator_create_2d) (const float \_Complex \* ref, size\_t ny, size\_t nx, size\_t dwell, int nthreads, size\_t ny\_out, size\_t nx\_out, int col\_out) <br>_Allocate a 2-D FFT correlator with coherent integrate-and-dump. Two-dimensional extension of_ [_**dp\_correlator\_create\_1d()**_](correlator__core_8h.md#function-dp_correlator_create_1d) _. The reference is a flat row-major ny×nx CF32 array; its conjugate spectrum is pre-computed once so each execute() call costs two 2-D FFTs plus ny\*nx complex multiplies. The Python wrapper requires_`ref` _to be a 2-D ndarray with shape (ny, nx); it passes a flat view to C._ |
|  [**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* | [**dp\_correlator\_create\_delay\_doppler**](#function-dp_correlator_create_delay_doppler) (const float \_Complex \* code, size\_t code\_len, size\_t ny, size\_t dwell, int nthreads, size\_t ny\_out) <br>_Allocate the delay-Doppler flavor: a code correlator that maps a raw stream of_ `ny` _code periods onto (Doppler, delay)._ |
|  [**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* | [**dp\_correlator\_create\_detector\_1d**](#function-dp_correlator_create_detector_1d) (const float \_Complex \* ref, size\_t ref\_len, size\_t dwell, size\_t noise\_lo, size\_t noise\_hi, [**det\_noise\_mode\_t**](correlator__core_8h.md#enum-det_noise_mode_t) noise\_mode, float threshold, int nthreads) <br>_Allocate a 1-D streaming detector: the 2-D detector at one row (docs/design/correlator.md section 2.1), so its hits are the 2-D one's at ny = 1, each with row 0 and the lag in col._  |
|  [**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* | [**dp\_correlator\_create\_detector\_2d**](#function-dp_correlator_create_detector_2d) (const float \_Complex \* ref, size\_t ny, size\_t nx, size\_t dwell, size\_t noise\_lo, size\_t noise\_hi, [**det\_noise\_mode\_t**](correlator__core_8h.md#enum-det_noise_mode_t) noise\_mode, float threshold, int nthreads) <br>_Allocate a 2-D streaming detector: the 2-D correlator with the stream and detection layers._  |
|  void | [**dp\_correlator\_destroy**](#function-dp_correlator_destroy) ([**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state) <br>_Destroy and free a correlator, any flavor._  |
|  size\_t | [**dp\_correlator\_execute**](#function-dp_correlator_execute) ([**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state, const float \_Complex \* in, size\_t n\_in, float \_Complex \* out, size\_t max\_out) <br>_Correlate one 2-D frame and optionally dump the coherent accumulator. Runs the 2-D pipeline: FFT2 → pointwise multiply with ref\_spec → accumulate the cross-spectrum; on dump, IFFT2 → normalise (÷ ny\*nx). Accumulating in the frequency domain and inverting once is exactly the per-frame inverse summed, by linearity of the IFFT — valid because the dwell is_ **coherent** _(a complex sum); a non-coherent (magnitude) integration could not defer the inverse._ |
|  size\_t | [**dp\_correlator\_execute\_all**](#function-dp_correlator_execute_all) ([**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state, const float \_Complex \* in, size\_t n\_in, float \_Complex \* out, size\_t max\_out) <br>_Every map of a batch, unreduced: row h of the (H, n\_out) result is the map of frame h (in C, map h is at_ `out + h * n_out` _)._ |
|  size\_t | [**dp\_correlator\_execute\_all\_max\_out**](#function-dp_correlator_execute_all_max_out) (const [**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state, size\_t n\_in) <br>_Samples_ [_**dp\_correlator\_execute\_all()**_](correlator__core_8h.md#function-dp_correlator_execute_all) _writes for_`n_in` _input samples: H \* n\_out, H = n\_in / (ny \* nx)._ |
|  size\_t | [**dp\_correlator\_execute\_coherent**](#function-dp_correlator_execute_coherent) ([**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state, const float \_Complex \* in, size\_t n\_in, float \_Complex \* out, size\_t max\_out) <br>_The coherent sum of a batch's maps, inverted once._  |
|  size\_t | [**dp\_correlator\_execute\_coherent\_max\_out**](#function-dp_correlator_execute_coherent_max_out) (const [**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state) <br>_Samples_ [_**dp\_correlator\_execute\_coherent()**_](correlator__core_8h.md#function-dp_correlator_execute_coherent) _writes: n\_out._ |
|  size\_t | [**dp\_correlator\_execute\_fold**](#function-dp_correlator_execute_fold) ([**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state, const float \_Complex \* in, size\_t n\_in, const [**dp\_correlator\_fold\_t**](structdp__correlator__fold__t.md) \* fold) <br>_Make the map of every frame in a batch and hand each to a reducer._  |
|  size\_t | [**dp\_correlator\_execute\_max\_out**](#function-dp_correlator_execute_max_out) ([**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state) <br>_Maximum output samples per execute call (_ [_**dp\_correlator\_state\_t::n\_out**_](structdp__correlator__state__t.md#variable-n_out) __ _ny\_out\*nx\_out normally, or ny when a single_[_**dp\_correlator\_state\_t::col\_out**_](structdp__correlator__state__t.md#variable-col_out) _was selected)._ |
|  size\_t | [**dp\_correlator\_execute\_peaks**](#function-dp_correlator_execute_peaks) ([**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state, const float \_Complex \* in, size\_t n\_in, float \* out, uint32\_t \* out1, uint32\_t \* out2, float \* out3, size\_t max\_out) <br>_One record per map of a batch: its peak power, where the peak is, and its mean power._  |
|  size\_t | [**dp\_correlator\_execute\_peaks\_max\_out**](#function-dp_correlator_execute_peaks_max_out) (const [**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state, size\_t n\_in) <br>_Records_ [_**dp\_correlator\_execute\_peaks()**_](correlator__core_8h.md#function-dp_correlator_execute_peaks) _writes for_`n_in` _samples: H._ |
|  size\_t | [**dp\_correlator\_execute\_power\_max**](#function-dp_correlator_execute_power_max) ([**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state, const float \_Complex \* in, size\_t n\_in, float \* out, uint32\_t \* out1, size\_t max\_out) <br>_The largest power over a batch's maps, cell by cell, and the map it came from._  |
|  size\_t | [**dp\_correlator\_execute\_power\_max\_max\_out**](#function-dp_correlator_execute_power_max_max_out) (const [**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state) <br>_Values per output_ [_**dp\_correlator\_execute\_power\_max()**_](correlator__core_8h.md#function-dp_correlator_execute_power_max) _writes: n\_out._ |
|  size\_t | [**dp\_correlator\_execute\_power\_sum**](#function-dp_correlator_execute_power_sum) ([**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state, const float \_Complex \* in, size\_t n\_in, float \* out, size\_t max\_out) <br>_The power of a batch's maps, summed cell by cell: each output cell is the sum over h of that cell's_ `|map_h|^2` _, summed in h order in float._ |
|  size\_t | [**dp\_correlator\_execute\_power\_sum\_max\_out**](#function-dp_correlator_execute_power_sum_max_out) (const [**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state) <br>_Powers_ [_**dp\_correlator\_execute\_power\_sum()**_](correlator__core_8h.md#function-dp_correlator_execute_power_sum) _writes: n\_out._ |
|  void | [**dp\_correlator\_get\_state**](#function-dp_correlator_get_state) (const [**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state, void \* blob) <br>_Serialize the frame count and spectrum accumulator into_ `blob` _._ |
|  size\_t | [**dp\_correlator\_push**](#function-dp_correlator_push) ([**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state, const float \_Complex \* in, size\_t n\_in, [**dp\_correlator\_hit\_t**](structdp__correlator__hit__t.md) \* result, size\_t max\_results) <br>_Stream an arbitrary-length CF32 chunk through a detector flavor._  |
|  void | [**dp\_correlator\_reset**](#function-dp_correlator_reset) ([**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state) <br>_Zero the accumulator and reset the integration counter to 0. Equivalent to starting a fresh dwell cycle without rebuilding FFT plans or recomputing ref\_spec._  |
|  int | [**dp\_correlator\_set\_ref**](#function-dp_correlator_set_ref) ([**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state, const float \_Complex \* ref) <br>_Replace the reference and recompute its spectrum._  |
|  int | [**dp\_correlator\_set\_state**](#function-dp_correlator_set_state) ([**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state, const void \* blob) <br>_Restore a mid-dwell correlator from_ [_**dp\_correlator\_get\_state()**_](correlator__core_8h.md#function-dp_correlator_get_state) _'s blob._ |
|  int | [**dp\_correlator\_set\_threshold**](#function-dp_correlator_set_threshold) ([**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state, float threshold) <br>_Change a detector flavor's threshold without rebuilding it._  |
|  size\_t | [**dp\_correlator\_state\_bytes**](#function-dp_correlator_state_bytes) (const [**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state) <br>_Size in bytes of this correlator's state blob._  |
|  int | [**dp\_correlator\_state\_check**](#function-dp_correlator_state_check) (const [**dp\_correlator\_state\_t**](structdp__correlator__state__t.md) \* state, const void \* blob) <br>_Whether_ [_**dp\_correlator\_set\_state()**_](correlator__core_8h.md#function-dp_correlator_set_state) _would accept_`blob` _, without writing anything._ |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**CORRELATOR\_STATE\_MAGIC**](correlator__core_8h.md#define-correlator_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('C','O','R','L')`<br>_State-blob magic ('CORL'); set\_state() rejects any other._  |
| define  | [**CORRELATOR\_STATE\_VERSION**](correlator__core_8h.md#define-correlator_state_version)  `1u`<br>_State-blob layout version; set\_state() rejects any other._  |

## Detailed Description


One object, built by flavor (docs/design/correlator.md section 4):



|constructor   |flavor    |
|-----|-----|
|[**dp\_correlator\_create\_1d()**](correlator__core_8h.md#function-dp_correlator_create_1d)   |a 1-D reference, one row    |
|[**dp\_correlator\_create\_2d()**](correlator__core_8h.md#function-dp_correlator_create_2d)   |a (ny, nx) reference    |
|[**dp\_correlator\_create\_delay\_doppler()**](correlator__core_8h.md#function-dp_correlator_create_delay_doppler)   |a code, mapped to (Doppler, delay)    |
|[**dp\_correlator\_create\_detector\_2d()**](correlator__core_8h.md#function-dp_correlator_create_detector_2d)   |a streaming 2-D detector    |
|[**dp\_correlator\_create\_detector\_1d()**](correlator__core_8h.md#function-dp_correlator_create_detector_1d)   |a streaming 1-D detector   |






The methods are shared. The 1-D flavor is the 2-D one at ny = 1, which the single-row path below serves, so a length-n correlation and a one-row 2-D correlation are the same bits (test\_corr\_equiv), and so are the two detectors' hits (test\_detector\_equiv). The map flavors serve execute and its batch family; the detector flavors serve push, and each refuses the other's (SIZE\_MAX).


All buffers are ny×nx row-major flat arrays of length ny\*nx. The correlation theorem extends naturally:


`R_xh[i,j] = IFFT2( FFT2(x) · conj(FFT2(h)) ) / (ny*nx)`


The reference spectrum is pre-computed at create time. The int-dump semantics are identical to the 1-D case: coherently sum `dwell` frames, then dump.


**Single-row-reference fast path.** When `ref` is nonzero only in row 0 (the DSSS acquisition/detection shape: a code replica with no Doppler content of its own) and `ny_out` == `ny` (no row-axis interpolation), `ref_spec[u,v]` is independent of `u` — the row axis of the 2-D transform pair cancels to an exact identity (DFT orthogonality: sum\_u exp(2\*pi\*i\*u\*(i-i)/ny) = ny`iff`i==i'`, else 0), for *any* row content.`dp\_correlator\_execute`then reduces exactly to `R(i,j) = IFFT\_nx(FFT\_nx(row\_i) · conj(FFT\_nx(ref\_row0)))(j) / nx`, applied independently per row — so`dp\_correlator\_create\_2d`detects this shape and runs`ny`independent length-`nx`1-D FFTs instead of a full`(ny,nx)` 2-D FFT, skipping the row-axis work that would otherwise cancel to a no-op. Any other reference shape, or`ny\_out &gt; ny`, uses the general 2-D path unchanged.


Lifecycle: 
```C++
float _Complex ref[NY * NX] = { ... };    // row-major 2-D reference
dp_correlator_state_t *c = dp_correlator_create_2d(ref, NY, NX, 4, 1, 0, 0, -1);
float _Complex out[NY * NX];
for (int i = 0; i < 4; i++) {
    size_t n_out = dp_correlator_execute(c, frame[i], NY*NX, out, NY*NX);
    if (n_out) process_2d(out, NY, NX);   // fires once, on i == 3
}
dp_correlator_destroy(c);
```
 


    
## Public Types Documentation




### enum det\_noise\_mode\_t 

_How a detector flavor aggregates the noise floor from the correlation magnitudes over the flat bins noise\_lo to noise\_hi, inclusive._ 
```C++
enum det_noise_mode_t {
    DET_NOISE_MEAN = 0,
    DET_NOISE_MEDIAN = 1,
    DET_NOISE_MIN = 2,
    DET_NOISE_MAX = 3
};
```



The test statistic is peak\_mag / noise\_est. A zero noise\_est (e.g., when all bins in the range are zero) yields test\_stat = 0, and so does a NaN one (a NaN anywhere in the range is a NaN mean, median, min or max). 


        

<hr>



### enum dp\_correlator\_flavor\_t 

_Which constructor built a correlator._ 
```C++
enum dp_correlator_flavor_t {
    DP_CORRELATOR_1D = 1,
    DP_CORRELATOR_2D = 2,
    DP_CORRELATOR_DELAY_DOPPLER = 3,
    DP_CORRELATOR_DETECTOR_1D = 4,
    DP_CORRELATOR_DETECTOR_2D = 5
};
```



Fixed at create and the state blob's first field: a blob restores only into a correlator of its own flavor. 


        

<hr>
## Public Functions Documentation




### function dp\_correlator\_bank\_detect 

_Detect on a coarse carrier grid, refine each candidate at its own cell on a fine one, and keep what confirms._ 
```C++
size_t dp_correlator_bank_detect (
    const dp_correlator_state_t * state,
    const float _Complex * x,
    size_t x_len,
    size_t L_d,
    size_t L_r,
    int g_min,
    int g_max,
    float t_candidate,
    float t_confirm,
    const float _Complex * rates,
    size_t rates_len,
    float * out,
    int32_t * out1,
    uint32_t * out2,
    uint32_t * out3,
    uint32_t * out4,
    size_t max_out
) 
```



The bank searches on `L_d` and resolves on `L_r` (docs/design/corr2d-hypotheses.md section 6):



* **Detect.** [**dp\_correlator\_bank\_power\_max()**](correlator__core_8h.md#function-dp_correlator_bank_power_max) over carriers `g_min` .. `g_max` on the `1 / (L_d nx)` grid, with every rate.
* **Candidates.** Every cell whose power exceeds `t_candidate` and is at least each of its 8 neighbors, both axes circular (a delay lag and a Doppler bin both wrap).
* **Refine.** For a candidate found by coarse carrier g\_d and rate r, the map value at its cell under every fine carrier g\_r from `m g_d - (m - 1)` to `m g_d + (m - 1)`, m = `L_r / L_d`: the carriers between its coarse neighbors, on the `1 / (L_r nx)` grid. On the fast path and DelayDoppler with a native delay axis each is a time-domain correlation at the cell's lag and a slow-time DFT at its Doppler row, in double, with no FFT: `2 ny nx` multiply-adds a carrier. Any other object makes that hypothesis's map and reads the cell. The best g\_r wins, the lowest on a tie.
* **Confirm.** A candidate whose refined power exceeds `t_confirm` is a record.




Records come out in cell order (row-major), one entry of each output per record: `out` the refined power, `out1` g\_r on the fine grid, `out2` the rate r, `out3` the row and `out4` the column, the cell as execute() lays out the map. Measured at 64 x 512 against an oracle that corrects the exact offset, `L_d` = 2 with `L_r` = 4 holds L = 4's sensitivity within 0.16 dB, for about half the bank (the design page). `L_d` = `L_r` is the one-stage bank. Thresholds are powers in the map's own units; the caller sets them for its noise, as for any detector.




**Parameters:**


* `state` As [**dp\_correlator\_bank\_fold()**](correlator__core_8h.md#function-dp_correlator_bank_fold); `g_min` and `g_max` are on the coarse grid. 
* `L_d` The search grid; at least 1. 
* `L_r` The resolution grid; a multiple of `L_d`. 
* `t_candidate` Candidate threshold, on the coarse power. 
* `t_confirm` Confirm threshold, on the refined power. 
* `out` Refined powers. 
* `out1` Refined carriers, on the `1 / (L_r nx)` grid. 
* `out2` Rates. 
* `out3` Rows. 
* `out4` Columns. 
* `max_out` Capacity of each output; at least `n_out`. 



**Returns:**

Python: the tuple (power, g, rate, row, col), one entry per confirmed record (possibly none): float32 refined power, int32 refined carrier, uint32 rate, row and column, always new arrays (this call takes no `out=`); anything below is a ValueError. C: the record count (0 included), or SIZE\_MAX with nothing written for anything [**dp\_correlator\_bank\_fold()**](correlator__core_8h.md#function-dp_correlator_bank_fold) refuses, `L_d == 0`, an `L_r` that `L_d` does not divide, a refined carrier past int32, or `max_out` below `n_out`. 
```C++
>>> import numpy as np
>>> from doppler.spectral import DelayDoppler
>>> rng = np.random.default_rng(0)
>>> ny, nx, d = 8, 32, 5
>>> code = rng.choice([-1.0, 1.0], nx).astype(np.complex64)
>>> m = np.arange(ny * nx)
>>> x = (np.tile(np.roll(code, d), ny)    # 1.25 bins: between L_d = 2
...      * np.exp(2j * np.pi * 1.25 / nx * m)).astype(np.complex64)
>>> dd = DelayDoppler(code, ny=ny)
>>> no_rates = np.empty(0, np.complex64)
>>> coarse, _ = dd.bank_power_max(x, 2, -4, 4, no_rates)
>>> t = 0.5 * coarse.max()
>>> power, g, rate, row, col = dd.bank_detect(x, 2, 4, -4, 4, t, t,
...                                           no_rates)
>>> len(power), int(g[0]), int(col[0])    # g on the 1/(4 nx) grid
(1, 5, 5)
>>> round(float(power[0] / coarse.max()), 3)  # the straddle recovered
1.233
```
 





        

<hr>



### function dp\_correlator\_bank\_detect\_max\_out 

_Capacity_ [_**dp\_correlator\_bank\_detect()**_](correlator__core_8h.md#function-dp_correlator_bank_detect) _needs: one record per map cell._
```C++
size_t dp_correlator_bank_detect_max_out (
    dp_correlator_state_t * state,
    size_t x_len
) 
```





**Parameters:**


* `state` The correlator. 
* `x_len` The frame's length (the bound does not depend on it). 



**Returns:**

`n_out`. 





        

<hr>



### function dp\_correlator\_bank\_fold 

_Run one frame through a bank of carrier and rate hypotheses and hand each map to a reducer._ 
```C++
size_t dp_correlator_bank_fold (
    const dp_correlator_state_t * state,
    const float _Complex * x,
    size_t x_len,
    size_t L,
    int g_min,
    int g_max,
    const float _Complex * rates,
    size_t rates_len,
    const dp_correlator_fold_t * fold
) 
```



A bank is what a frame batch of corrected copies would compute, without the copies: carrier g corrects an offset of `g / (L * nx)` cycles/sample, for every g from `g_min` to `g_max`, and each rate r weights the slow-time samples by the r-th ny-long vector of `rates`. Map h, for `h = (g - g_min) * R + r` (R = `rates_len / ny`, or 1 with no rates), is what a fresh dwell-1 object's execute() returns for the frame with period i multiplied by element i of rate vector r and by `exp(-j 2 pi g n / (L nx))`, where n counts samples WITHIN period i (0 .. nx-1), up to float rounding.


The correction restarts at every period on purpose: the phase a carrier adds between periods is slow-time Doppler, which the slow-time FFT measures, so a signal stays on its own absolute Doppler row under every carrier hypothesis (docs/design/corr2d-hypotheses.md, rule 4).


Every path runs a bank, and only the single-row fast path and DelayDoppler share work: whole bins (g a multiple of L) are a roll of the row spectrum, each fraction `g mod L` mixes and transforms the rows once for every whole bin that shares it. On DelayDoppler the one delay-Doppler map then runs in the order its rule picks for the bank's carriers per fraction and its rates (docs/design/acq-surface.md §18.1): rows first inverts each carrier's rows once for all its rates and pays a slow-time FFT per rate; slow first shares each fraction's slow time across its whole-bin carriers and pays their rows per rate. On Corr2D, whose rows are its periods, a rate scales the carrier's rows. The general 2-D path and a known column run each hypothesis's corrected frame through their own frame code, which is the definition with nothing shared.


Maps reach `fold` in h order, so a reducer's tie rule is the batch's. The stages split across the object's workers exactly as execute() does, and every map is bit-identical for any worker count. The dwell accumulator, `count` and the state blob are never read or written.




**Parameters:**


* `state` Any Corr2D or DelayDoppler object. 
* `x` One frame, `x_len` samples. 
* `x_len` Must be exactly `ny * nx`. 
* `L` Fractions per whole bin; at least 1. 
* `g_min` Lowest carrier, in units of `1 / (L * nx)` cycles/sample. 
* `g_max` Highest carrier; at least `g_min`. 
* `rates` Per-period weights, R vectors of ny, or NULL with `rates_len` 0 (one unit vector). 
* `rates_len` A whole multiple of ny, 0 included. 
* `fold` The reducer, called once per map. 



**Returns:**

The number of maps, `(g_max - g_min + 1) * R`; or SIZE\_MAX, with nothing folded, for a frame of another length, `L == 0`, `g_min > g_max`, a `rates_len` that is not a whole number of ny-long vectors, a count past 2^32, or scratch that cannot be allocated. 
```C++
#include "doppler/correlator/correlator_core.h"
#include <stdio.h>
static void count_maps (void *ctx, size_t h, const float _Complex *map,
                        size_t n_out)
{
  (void)h, (void)map, (void)n_out;
  ++*(size_t *)ctx;
}
int main (void)
{
  float _Complex code[8] = { 1, -1, 1, 1, -1, 1, -1, -1 }, x[4 * 8] = { 0 };
  dp_correlator_state_t *dd = dp_correlator_create_delay_doppler (code, 8, 4, 1, 1, 0);
  size_t maps = 0;
  dp_correlator_fold_t f = { count_maps, &maps };
  size_t h = dp_correlator_bank_fold (dd, x, 4 * 8, 4, -4, 4, NULL, 0, &f);
  printf ("%zu %zu\n", h, maps); // 9 carriers, one rate: 9 maps
  dp_correlator_destroy (dd);
  return !(h == 9 && maps == 9);
}
```
 





        

<hr>



### function dp\_correlator\_bank\_power\_max 

_The strongest power at every cell over a bank, and which hypothesis it came from._ 
```C++
size_t dp_correlator_bank_power_max (
    const dp_correlator_state_t * state,
    const float _Complex * x,
    size_t x_len,
    size_t L,
    int g_min,
    int g_max,
    const float _Complex * rates,
    size_t rates_len,
    float * out,
    uint32_t * out1,
    size_t max_out
) 
```



[**dp\_correlator\_bank\_fold()**](correlator__core_8h.md#function-dp_correlator_bank_fold) with the `POWER_MAX` reducer: at each cell, `out` holds the largest \|map\_h\|^2 over every hypothesis h, and `out1` that h, `(g - g_min) * R + r`, the lowest h on a tie. Every argument and refusal is [**dp\_correlator\_bank\_fold()**](correlator__core_8h.md#function-dp_correlator_bank_fold)'s; in addition `max_out` below `n_out` is refused with nothing written.




**Parameters:**


* `state` As [**dp\_correlator\_bank\_fold()**](correlator__core_8h.md#function-dp_correlator_bank_fold). 
* `out` `n_out` powers. 
* `out1` `n_out` hypothesis indices. 
* `max_out` Capacity of each output; at least `n_out`. 



**Returns:**

Python: the tuple (power, index), each (n\_out,): float32 powers and uint32 hypothesis indices, always new arrays (this call takes no `out=`); a refusal is a ValueError. C: `n_out`, or SIZE\_MAX with nothing written. 
```C++
>>> import numpy as np
>>> from doppler.spectral import DelayDoppler
>>> rng = np.random.default_rng(0)
>>> ny, nx, d = 8, 32, 5                  # code delayed by 5 samples
>>> code = rng.choice([-1.0, 1.0], nx).astype(np.complex64)
>>> m = np.arange(ny * nx)
>>> x = (np.tile(np.roll(code, d), ny)    # on a carrier 1.5 bins off
...      * np.exp(2j * np.pi * 1.5 / nx * m)).astype(np.complex64)
>>> dd = DelayDoppler(code, ny=ny)
>>> no_rates = np.empty(0, np.complex64)
>>> pw, h = dd.bank_power_max(x, 2, -4, 4, no_rates)  # g / (2 nx), -4..4
>>> cell = int(np.argmax(pw))
>>> cell % nx, int(h[cell]) - 4           # (delay, g = h + g_min)
(5, 3)
```
 





        

<hr>



### function dp\_correlator\_bank\_power\_max\_max\_out 

_Capacity_ [_**dp\_correlator\_bank\_power\_max()**_](correlator__core_8h.md#function-dp_correlator_bank_power_max) _needs: one map._
```C++
size_t dp_correlator_bank_power_max_max_out (
    dp_correlator_state_t * state,
    size_t x_len
) 
```





**Parameters:**


* `state` The correlator. 
* `x_len` The frame's length (one map whatever it is). 



**Returns:**

`n_out`. 





        

<hr>



### function dp\_correlator\_consumed 

_Input samples the last_ [_**dp\_correlator\_push()**_](correlator__core_8h.md#function-dp_correlator_push) _took._
```C++
size_t dp_correlator_consumed (
    const dp_correlator_state_t * state
) 
```



Equal to its `n_in` unless `result` filled up; then the caller resumes at in + consumed. 0 after create, reset, set\_ref and set\_state, and always 0 for a map flavor.




**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Samples of the last push's input consumed (including any kept as the carry). 





        

<hr>



### function dp\_correlator\_create\_1d 

_Allocate a 1-D FFT correlator with coherent integrate-and-dump. Pre-computes conj(FFT(ref)) once at construction so each execute() call costs only two FFTs and n complex multiplies._ `ref` _may be freed after this returns. With_`dwell` _== 1 every call produces output; with larger values the accumulator absorbs_`dwell` _frames before dumping._
```C++
dp_correlator_state_t * dp_correlator_create_1d (
    const float _Complex * ref,
    size_t ref_len,
    size_t dwell,
    int nthreads,
    size_t n_out
) 
```



It is [**dp\_correlator\_create\_2d()**](correlator__core_8h.md#function-dp_correlator_create_2d) at ny = 1, nx = `ref_len:` the single-row path, so the same bits (test\_corr\_equiv). Its frame and its map are both 1-D: execute() takes `ref_len` samples and a dump writes n\_out.




**Parameters:**


* `ref` Reference signal, CF32, length `ref_len`. 
* `ref_len` Reference / FFT length in samples. 
* `dwell` Integration depth; must be &gt;= 1 (0 is refused). Pass 1 for immediate output on every call. 
* `nthreads` Accepted for API compatibility; ignored. 
* `n_out` Inverse/output length; 0 =&gt; native (n). Must be &gt;= n. A larger value zero-pads the cross-spectrum before the inverse, returning the band-limited (Dirichlet) interpolation of the correlation on a finer length-n\_out grid — same peak, sub-bin lag resolution. Native is bit-exact and allocates no extra buffer. 



**Returns:**

Heap-allocated state (free with [**dp\_correlator\_destroy()**](correlator__core_8h.md#function-dp_correlator_destroy)), or NULL on a dwell of 0, an `n_out` below `ref_len`, or allocation failure. 
```C++
>>> from doppler.spectral import Corr
>>> import numpy as np
>>> ref = np.zeros(4, dtype=np.complex64); ref[0] = 1.0
>>> corr = Corr(ref=ref, dwell=1, nthreads=1)
>>> corr.n, corr.dwell, corr.count
(4, 1, 0)
```
 





        

<hr>



### function dp\_correlator\_create\_2d 

_Allocate a 2-D FFT correlator with coherent integrate-and-dump. Two-dimensional extension of_ [_**dp\_correlator\_create\_1d()**_](correlator__core_8h.md#function-dp_correlator_create_1d) _. The reference is a flat row-major ny×nx CF32 array; its conjugate spectrum is pre-computed once so each execute() call costs two 2-D FFTs plus ny\*nx complex multiplies. The Python wrapper requires_`ref` _to be a 2-D ndarray with shape (ny, nx); it passes a flat view to C._
```C++
dp_correlator_state_t * dp_correlator_create_2d (
    const float _Complex * ref,
    size_t ny,
    size_t nx,
    size_t dwell,
    int nthreads,
    size_t ny_out,
    size_t nx_out,
    int col_out
) 
```





**Parameters:**


* `ref` Reference image, 2-D (ny, nx) CF32 ndarray in Python. 
* `ny` Number of rows in the reference and input frames. 
* `nx` Number of columns in the reference and input frames. 
* `dwell` Integration depth; must be &gt;= 1. 
* `nthreads` Workers for the single-row-reference fast path's per-row stage: 1 runs serially (no threads), N runs N workers (the caller included), &lt;= 0 uses every online core. The count is capped at `ny`, one row per worker at most. The output is bit-identical whatever the count. The general 2-D path and the `col_out` path run serially and ignore it. 
* `ny_out` Inverse/output rows; 0 =&gt; native (ny). Must be &gt;= ny. A larger output zero-pads the cross-spectrum before the inverse, returning the band-limited (Dirichlet) interpolation of the correlation on a finer (ny\_out, nx\_out) grid — same peak, sub-bin resolution. Native is bit-exact and allocates no extra buffers. 
* `nx_out` Inverse/output columns; 0 =&gt; native (nx). Must be &gt;= nx. 
* `col_out` Emit ONLY this correlation lag, or &lt; 0 for the whole map. When set, a dump writes `ny` values  one per row  instead of ny\*nx\_out, computed as the time-domain sum over p of `conj(ref(p)) * row((p + col_out) mod nx)`, which is what R(i, col\_out) expands to once the 1/nx cancels. No transform runs in either direction, so the cost per row is O(nx) rather than O(nx log nx). Requires the single-row-reference fast path (a caller that knows its lag is by construction correlating against a code replica) and the NATIVE output grid: an interpolated column is a fractional lag, which no time-domain sum produces. create() returns NULL if `col_out` &gt;= 0 with a reference the fast path rejects, with `ny_out` or `nx_out` decoupled, or with a lag outside 0 .. nx-1. 



**Returns:**

Heap-allocated state, or NULL on failure. 
```C++
>>> from doppler.spectral import Corr2D
>>> import numpy as np
>>> ref = np.zeros((4, 4), dtype=np.complex64); ref[0, 0] = 1.0
>>> c = Corr2D(ref=ref, dwell=1, nthreads=1)
>>> c.ny, c.nx, c.dwell, c.count
(4, 4, 1, 0)
```
 





        

<hr>



### function dp\_correlator\_create\_delay\_doppler 

_Allocate the delay-Doppler flavor: a code correlator that maps a raw stream of_ `ny` _code periods onto (Doppler, delay)._
```C++
dp_correlator_state_t * dp_correlator_create_delay_doppler (
    const float _Complex * code,
    size_t code_len,
    size_t ny,
    size_t dwell,
    int nthreads,
    size_t ny_out
) 
```



Input to [**dp\_correlator\_execute()**](correlator__core_8h.md#function-dp_correlator_execute) is the raw stream  `ny` periods of `code_len` samples, back to back, no reshaping or reference padding by the caller. Each period (row) is correlated against `code` exactly as Corr2D's single-row fast path does (FFT, multiply by `conj(FFT(code))`, coherent `dwell` accumulation of the product spectrum, inverse scaled by `1/nx`), and on the dump the slow-time FFT runs down every delay column. The library's one delay-Doppler map computes it (docs/design/acq-surface.md §18) in the order its rule picks: here the rows first, each row inverted as it is accumulated, which is what makes the row stage fused. In numpy terms, for one frame `x` (dwell 1):  The output is `ny_out x nx` row-major: rows are Doppler bins, centered (zero Doppler on row `ny_out / 2`), columns are delay lags. A signal of amplitude `a` that is the code delayed by `d` samples and rotated by `f` cycles/sample peaks at column `d` and row `ny_out/2 + f / doppler_step`, where [**dp\_correlator\_state\_t::doppler\_step**](structdp__correlator__state__t.md#variable-doppler_step) is `1 / (ny_out * nx)` cycles/sample (times the sample rate for Hz). Its peak is `a * dwell * ny * sum(abs(code)**2)`: the 1/nx of the row inverse cancels the row correlation's own gain of nx, and the slow-time FFT is unnormalized, as numpy's is, so it sums the ny rows coherently.


`ny_out > ny` zero-pads the slow-time samples after the ny rows: the ny\_out-point DFT then samples the same Doppler response on a grid ny\_out/ny times finer (band-limited interpolation). The pad adds no energy, so the peak at a bin center is unchanged and a Doppler between native bins is found at its fractional row.


Both stages fan out over `nthreads` workers  the rows, then the columns  each worker on its own FFT plans, and the result is bit-identical for every thread count.




**Parameters:**


* `code` The code replica, `code_len` samples (one period). 
* `code_len` Samples per period, nx; must be &gt;= 1. 
* `ny` Periods per frame, the slow-time length; must be &gt;= 1. 
* `dwell` Frames coherently accumulated per dump; must be &gt;= 1. 
* `nthreads` Workers: 1 serial, N for N (the caller included), &lt;= 0 every online core; capped at the most chunks a stage can use, `max(ny, ceil(code_len / 16))`. 
* `ny_out` Doppler bins in the map; 0 means `ny`. Must be &gt;= `ny`. 



**Returns:**

Heap-allocated state (free with [**dp\_correlator\_destroy()**](correlator__core_8h.md#function-dp_correlator_destroy)), or NULL if `code` is NULL, `code_len` or `ny` is 0, `dwell` is 0, or `ny_out` is below `ny`. 
```C++
>>> import numpy as np
>>> from doppler.spectral import DelayDoppler
>>> rng = np.random.default_rng(0)
>>> ny, nx, d, k = 16, 64, 5, 3       # delay 5, Doppler 3 bins
>>> code = np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)
>>> n = np.arange(ny * nx)
>>> f = k / (ny * nx)                 # cycles/sample
>>> x = (np.tile(np.roll(code, d), ny)
...      * np.exp(2j * np.pi * f * n)).astype(np.complex64)
>>> dd = DelayDoppler(code, ny=ny, dwell=2)
>>> out = np.empty(dd.execute_max_out(), np.complex64)
>>> for _ in range(2):                # one preallocated map per dump
...     m = dd.execute(x, out=out)
>>> m is not None and m.base is out
True
>>> row, col = np.unravel_index(np.argmax(np.abs(out)), (ny, nx))
>>> int(row) - ny // 2, int(col)      # (Doppler bins, delay)
(3, 5)
>>> dd.doppler_step == 1 / (ny * nx)
True
```
 





        

<hr>



### function dp\_correlator\_create\_detector\_1d 

_Allocate a 1-D streaming detector: the 2-D detector at one row (docs/design/correlator.md section 2.1), so its hits are the 2-D one's at ny = 1, each with row 0 and the lag in col._ 
```C++
dp_correlator_state_t * dp_correlator_create_detector_1d (
    const float _Complex * ref,
    size_t ref_len,
    size_t dwell,
    size_t noise_lo,
    size_t noise_hi,
    det_noise_mode_t noise_mode,
    float threshold,
    int nthreads
) 
```



After every dump the peak-to-noise test statistic is compared against `threshold`, and a hit is emitted when it passes; 0.0 fires on every dump. The ring capacity is next\_pow\_two(max(n, 512)) complex samples.




**Parameters:**


* `ref` Reference signal, CF32 ndarray of length ref\_len. 
* `ref_len` Reference / FFT length in complex samples. 
* `dwell` Int-dump depth; must be &gt;= 1. 
* `noise_lo` Lower noise bin index (inclusive, 0-based). 
* `noise_hi` Upper noise bin index (inclusive, &lt; n). A value at or beyond the window clamps to n - 1, so the default sentinel selects the full window. 
* `noise_mode` Noise aggregation: "mean", "median", "min", or "max". 
* `threshold` Test-stat gate; 0.0 = always emit. 
* `nthreads` Accepted for API compatibility; ignored. 



**Returns:**

Heap-allocated state, or NULL when the arguments are refused (an empty reference, `dwell` 0) or the stream's ring cannot be made. Out of memory for the detection buffers aborts, by the library's dp\_x\* convention ([**clib\_common.h**](clib__common_8h.md)). 
```C++
>>> from doppler.spectral import CorrDetector
>>> import numpy as np
>>> ref = np.zeros(8, dtype=np.complex64); ref[0] = 1.0
>>> det = CorrDetector(ref=ref, dwell=1, noise_lo=1, noise_hi=7,
...                noise_mode="mean", threshold=0.0)
>>> det.n, det.dwell, det.ring_cap
(8, 1, 512)
```
 





        

<hr>



### function dp\_correlator\_create\_detector\_2d 

_Allocate a 2-D streaming detector: the 2-D correlator with the stream and detection layers._ 
```C++
dp_correlator_state_t * dp_correlator_create_detector_2d (
    const float _Complex * ref,
    size_t ny,
    size_t nx,
    size_t dwell,
    size_t noise_lo,
    size_t noise_hi,
    det_noise_mode_t noise_mode,
    float threshold,
    int nthreads
) 
```



Input frames are flat row-major CF32 arrays of length ny\*nx, cut from the stream by the ring's framer. On every dump the peak's flat index is decomposed into (row, col), and a [**dp\_correlator\_hit\_t**](structdp__correlator__hit__t.md) is emitted when test\_stat &gt; threshold. The Python class accepts a (ny, nx) CF32 ndarray for both `ref` and the push input. The ring capacity is next\_pow\_two(max(ny\*nx, 512)) complex samples.




**Parameters:**


* `ref` 2-D reference image, (ny, nx) CF32 ndarray in Python. 
* `ny` Number of rows in the reference and input frames. 
* `nx` Number of columns in the reference and input frames. 
* `dwell` Int-dump depth; must be &gt;= 1. 
* `noise_lo` Lower flat-index noise bin (inclusive, 0-based). 
* `noise_hi` Upper flat-index noise bin (inclusive, &lt; ny\*nx). A value at or beyond the window clamps to ny\*nx - 1, so the default sentinel selects the full window. 
* `noise_mode` Noise aggregation: "mean", "median", "min", or "max". 
* `threshold` Test-stat gate; 0.0 = always emit. 
* `nthreads` Workers for the single-row-reference fast path (1 serial, &lt;= 0 every online core); the general 2-D path ignores it. 



**Returns:**

Heap-allocated state, or NULL when the arguments are refused (an empty axis, `dwell` 0) or the stream's ring cannot be made. Out of memory for the detection buffers aborts, by the library's dp\_x\* convention ([**clib\_common.h**](clib__common_8h.md)), as the map's plans do. 
```C++
>>> from doppler.spectral import CorrDetector2D
>>> import numpy as np
>>> ref = np.zeros((4, 4), dtype=np.complex64); ref[0, 0] = 1.0
>>> det = CorrDetector2D(ref=ref, dwell=1, noise_lo=1, noise_hi=15,
...                  noise_mode="mean", threshold=0.0)
>>> det.ny, det.nx, det.n, det.dwell
(4, 4, 16, 1)
```
 





        

<hr>



### function dp\_correlator\_destroy 

_Destroy and free a correlator, any flavor._ 
```C++
void dp_correlator_destroy (
    dp_correlator_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function dp\_correlator\_execute 

_Correlate one 2-D frame and optionally dump the coherent accumulator. Runs the 2-D pipeline: FFT2 → pointwise multiply with ref\_spec → accumulate the cross-spectrum; on dump, IFFT2 → normalise (÷ ny\*nx). Accumulating in the frequency domain and inverting once is exactly the per-frame inverse summed, by linearity of the IFFT — valid because the dwell is_ **coherent** _(a complex sum); a non-coherent (magnitude) integration could not defer the inverse._
```C++
size_t dp_correlator_execute (
    dp_correlator_state_t * state,
    const float _Complex * in,
    size_t n_in,
    float _Complex * out,
    size_t max_out
) 
```



**Layout.** A frame is ny\*nx complex samples in row-major order: row i is samples i\*nx .. (i+1)\*nx - 1. For Corr2D that is a (ny, nx) image; for DelayDoppler it is the raw stream, ny code periods of nx = code\_len samples back to back. The map a dump produces is flat, n\_out = ny\_out \* nx\_out values (ny values with `col_out`), also row-major: reshape it to (ny\_out, nx\_out). On a DelayDoppler, axis 0 is Doppler (centered, zero Doppler on row ny\_out / 2) and axis 1 is delay (the lag, in samples).


**Ownership.** In Python, a call that dumps returns a new array when no `out=` is given, and a view of `out=` when one is (a 1-D array holding at least n\_out values; it is written on a dump only, and the next dump overwrites it). A call that does not dump returns None and touches neither.




**Parameters:**


* `state` Allocated 2-D correlator (non-NULL). 
* `in` Input frame, flat row-major CF32, length ny\*nx. 
* `n_in` Number of input samples; must equal ny\*nx; else REFUSED. 
* `out` Output buffer for the correlation map (CF32, length n\_out: ny\_out\*nx\_out, or ny with `col_out`); written only on a dump call. 
* `max_out` Capacity of `out` in elements. Emission stops there, so the return value is the number actually written. 



**Returns:**

Python: the map on a dump, None otherwise; a frame that is not ny\*nx samples is REFUSED with nothing read, written or counted, as a ValueError (#1925). C: n\_out on a dump (or `max_out` if smaller), 0 otherwise, and SIZE\_MAX for the refused frame. 
```C++
>>> from doppler.spectral import Corr2D
>>> import numpy as np
>>> ref = np.zeros((2, 2), dtype=np.complex64); ref[0, 0] = 1.0
>>> c = Corr2D(ref=ref, dwell=2)
>>> x = np.ones((2, 2), dtype=np.complex64)
>>> c.execute(x) is None   # frame 1 — no dump
True
>>> c.execute(x).tolist()  # frame 2 — dump
[(2+0j), (2+0j), (2+0j), (2+0j)]
```
 





        

<hr>



### function dp\_correlator\_execute\_all 

_Every map of a batch, unreduced: row h of the (H, n\_out) result is the map of frame h (in C, map h is at_ `out + h * n_out` _)._
```C++
size_t dp_correlator_execute_all (
    dp_correlator_state_t * state,
    const float _Complex * in,
    size_t n_in,
    float _Complex * out,
    size_t max_out
) 
```



Python: `x` is the H frames back to back, a flat array of H \* ny \* nx samples (H &gt;= 1): frame h is the samples from h \* ny \* nx up to, not including, (h + 1) \* ny \* nx. With no `out=` each call returns a new array; with `out=` it returns a view of it, and the next call into the same `out=` overwrites that view. `out=` is a 2-D complex64 array of shape (rows, n\_out) with at least H rows (a shorter one is refused); the result is its first H rows. The object's dwell is untouched.




**Parameters:**


* `state` Must be non-NULL. 
* `in` H frames back to back (CF32). 
* `n_in` A nonzero multiple of ny \* nx. 
* `out` H \* n\_out values (CF32). 
* `max_out` Capacity of `out`; at least [**dp\_correlator\_execute\_all\_max\_out()**](correlator__core_8h.md#function-dp_correlator_execute_all_max_out). 



**Returns:**

Python: the (H, n\_out) array; a refused batch (empty, not a whole number of frames, or an `out=` with fewer than H rows) is a ValueError with nothing written. C: H \* n\_out, or SIZE\_MAX for the refused batch. 
```C++
>>> import numpy as np
>>> from doppler.spectral import DelayDoppler
>>> rng = np.random.default_rng(0)
>>> ny, nx = 8, 32
>>> code = np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)
>>> dd = DelayDoppler(code, ny=ny)
>>> frames = np.concatenate([np.tile(np.roll(code, d), ny)
...                          for d in (3, 11)])   # delays 3 and 11
>>> maps = dd.execute_all(frames)
>>> maps.shape
(2, 256)
>>> [int(np.argmax(np.abs(m))) % nx for m in maps]   # each map's delay
[3, 11]
>>> dd.count                    # the dwell never moved
0
```
 





        

<hr>



### function dp\_correlator\_execute\_all\_max\_out 

_Samples_ [_**dp\_correlator\_execute\_all()**_](correlator__core_8h.md#function-dp_correlator_execute_all) _writes for_`n_in` _input samples: H \* n\_out, H = n\_in / (ny \* nx)._
```C++
size_t dp_correlator_execute_all_max_out (
    const dp_correlator_state_t * state,
    size_t n_in
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `n_in` Input samples about to be passed. 



**Returns:**

The capacity `execute_all` needs for `n_in` samples: H \* n\_out values (C: SIZE\_MAX if that overflows). 
```C++
>>> import numpy as np
>>> from doppler.spectral import DelayDoppler
>>> rng = np.random.default_rng(0)
>>> ny, nx = 8, 32
>>> code = np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)
>>> dd = DelayDoppler(code, ny=ny)
>>> frames = np.concatenate([np.tile(np.roll(code, d), ny)
...                          for d in (3, 11)])   # delays 3 and 11
>>> dd.execute_all_max_out(len(frames))   # 2 maps of 8 x 32
512
```
 





        

<hr>



### function dp\_correlator\_execute\_coherent 

_The coherent sum of a batch's maps, inverted once._ 
```C++
size_t dp_correlator_execute_coherent (
    dp_correlator_state_t * state,
    const float _Complex * in,
    size_t n_in,
    float _Complex * out,
    size_t max_out
) 
```



Python: `x` is the H frames back to back, a flat array of H \* ny \* nx samples (H &gt;= 1).


The frames' product spectra are summed and the sum is inverted, as a dwell does: the result is bit-identical to what a fresh object with dwell = H dumps for the same frames, and equal to the sum of the H maps up to rounding. The object's own dwell is untouched.




**Parameters:**


* `state` Must be non-NULL. 
* `in` H frames back to back (CF32). 
* `n_in` A nonzero multiple of ny \* nx. 
* `out` n\_out values (CF32). 
* `max_out` Capacity of `out`; at least n\_out. 



**Returns:**

Python: the one map, a new (n\_out,) array; a refused batch is a ValueError with nothing written. C: n\_out, or SIZE\_MAX for the refused batch. 
```C++
>>> import numpy as np
>>> from doppler.spectral import DelayDoppler
>>> rng = np.random.default_rng(0)
>>> ny, nx = 8, 32
>>> code = np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)
>>> dd = DelayDoppler(code, ny=ny)
>>> frames = np.concatenate([np.tile(np.roll(code, d), ny)
...                          for d in (3, 11)])   # delays 3 and 11
>>> x = np.tile(code, ny)
>>> total = dd.execute_coherent(np.concatenate([x, -x]))
>>> bool(np.abs(total).max() < 1e-3)          # x and -x cancel
True
```
 





        

<hr>



### function dp\_correlator\_execute\_coherent\_max\_out 

_Samples_ [_**dp\_correlator\_execute\_coherent()**_](correlator__core_8h.md#function-dp_correlator_execute_coherent) _writes: n\_out._
```C++
size_t dp_correlator_execute_coherent_max_out (
    const dp_correlator_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

n\_out, fixed at create(). 
```C++
>>> import numpy as np
>>> from doppler.spectral import DelayDoppler
>>> rng = np.random.default_rng(0)
>>> ny, nx = 8, 32
>>> code = np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)
>>> dd = DelayDoppler(code, ny=ny)
>>> frames = np.concatenate([np.tile(np.roll(code, d), ny)
...                          for d in (3, 11)])   # delays 3 and 11
>>> dd.execute_coherent_max_out()
256
```
 





        

<hr>



### function dp\_correlator\_execute\_fold 

_Make the map of every frame in a batch and hand each to a reducer._ 
```C++
size_t dp_correlator_execute_fold (
    dp_correlator_state_t * state,
    const float _Complex * in,
    size_t n_in,
    const dp_correlator_fold_t * fold
) 
```



The one loop every typed batch call runs on. Maps are made as described above and passed to `fold->fold(fold->ctx, h, map, n_out)` for h = 0 .. H-1, in that order, from the calling thread.




**Parameters:**


* `state` Must be non-NULL. 
* `in` H frames, each ny \* nx samples (CF32), back to back. 
* `n_in` Samples in `in:` a nonzero multiple of ny \* nx. 
* `fold` The reducer; must be non-NULL. 



**Returns:**

H, or SIZE\_MAX (nothing folded) when `n_in` is 0 or not a multiple of the frame. 
```C++
#include "doppler/correlator/correlator_core.h"
#include <stdio.h>

// A reducer: the energy of each map, one value per frame.
static void
energy (void *ctx, size_t h, const float _Complex *m, size_t n_out)
{
  double e = 0.0;
  for (size_t k = 0; k < n_out; k++)
    e += crealf (m[k]) * crealf (m[k]) + cimagf (m[k]) * cimagf (m[k]);
  ((double *)ctx)[h] = e;
}

int
main (void)
{
  const float _Complex code[4] = { 1.0f, 1.0f, -1.0f, 1.0f };
  dp_correlator_state_t   *dd
      = dp_correlator_create_delay_doppler (code, 4, 2, 1, 1, 0);
  float _Complex frames[3 * 2 * 4]; // 3 frames of ny x nx = 2 x 4
  for (size_t k = 0; k < 3 * 2 * 4; k++)
    frames[k] = code[k % 4] * (float)(1 + k / 8); // frame h is (h+1) x
  double           e[3];
  dp_correlator_fold_t f = { energy, e };
  size_t           H = dp_correlator_execute_fold (dd, frames, 3 * 2 * 4, &f);
  printf ("%zu maps, energy ratios %.0f %.0f\n", H, e[1] / e[0],
          e[2] / e[0]); // 3 maps, energy ratios 4 9
  dp_correlator_destroy (dd);
  return !(H == 3 && e[1] > 3.99 * e[0] && e[1] < 4.01 * e[0]);
}
```
 





        

<hr>



### function dp\_correlator\_execute\_max\_out 

_Maximum output samples per execute call (_ [_**dp\_correlator\_state\_t::n\_out**_](structdp__correlator__state__t.md#variable-n_out) __ _ny\_out\*nx\_out normally, or ny when a single_[_**dp\_correlator\_state\_t::col\_out**_](structdp__correlator__state__t.md#variable-col_out) _was selected)._
```C++
size_t dp_correlator_execute_max_out (
    dp_correlator_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

n\_out, fixed at create(). 





        

<hr>



### function dp\_correlator\_execute\_peaks 

_One record per map of a batch: its peak power, where the peak is, and its mean power._ 
```C++
size_t dp_correlator_execute_peaks (
    dp_correlator_state_t * state,
    const float _Complex * in,
    size_t n_in,
    float * out,
    uint32_t * out1,
    uint32_t * out2,
    float * out3,
    size_t max_out
) 
```



Python: `x` is the H frames back to back, a flat array of H \* ny \* nx samples (H &gt;= 1).


Entry h of each output describes map h: `out` its largest `|map|^2`, `out1` and `out2` the row and column of that cell (the first in row-major order on a tie), and `out3` the mean `|map|^2` over its n\_out cells. A map is ny\_out x nx\_out; with col\_out it is ny x 1, and the column reported is col\_out. The peak-to-mean of each map is `out` over `out3`, entry by entry: a statistic of each map, which the same statistic of a pointwise maximum over maps does not give.




**Parameters:**


* `state` Must be non-NULL. 
* `in` H frames back to back (CF32). 
* `n_in` A nonzero multiple of ny \* nx. 
* `out` H peak powers (float). 
* `out1` H peak rows (uint32). 
* `out2` H peak columns (uint32). 
* `out3` H mean powers (float). 
* `max_out` Capacity of each output; at least H. 



**Returns:**

Python: the tuple (peak, row, col, mean), each (H,): float32 peak power, uint32 row and column, float32 mean power, always new arrays (this call takes no `out=`); a refused batch is a ValueError. C: H, or SIZE\_MAX with nothing written. 
```C++
>>> import numpy as np
>>> from doppler.spectral import DelayDoppler
>>> rng = np.random.default_rng(0)
>>> ny, nx = 8, 32
>>> code = np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)
>>> dd = DelayDoppler(code, ny=ny)
>>> frames = np.concatenate([np.tile(np.roll(code, d), ny)
...                          for d in (3, 11)])   # delays 3 and 11
>>> peak, row, col, mean = dd.execute_peaks(frames)
>>> row.tolist(), col.tolist()     # zero Doppler is row ny / 2
([4, 4], [3, 11])
>>> bool(np.all(peak / mean > 100))   # each map's own peak-to-mean
True
```
 





        

<hr>



### function dp\_correlator\_execute\_peaks\_max\_out 

_Records_ [_**dp\_correlator\_execute\_peaks()**_](correlator__core_8h.md#function-dp_correlator_execute_peaks) _writes for_`n_in` _samples: H._
```C++
size_t dp_correlator_execute_peaks_max_out (
    const dp_correlator_state_t * state,
    size_t n_in
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `n_in` Input samples about to be passed. 



**Returns:**

n\_in / (ny \* nx). 
```C++
const float _Complex code[4] = { 1.0f, 1.0f, -1.0f, 1.0f };
dp_correlator_state_t *dd = dp_correlator_create_delay_doppler (code, 4, 2, 1, 1,
                                                         0);
size_t h = dp_correlator_execute_peaks_max_out (dd, 3 * 2 * 4); // 3 frames
dp_correlator_destroy (dd);
return h != 3;
```
 





        

<hr>



### function dp\_correlator\_execute\_power\_max 

_The largest power over a batch's maps, cell by cell, and the map it came from._ 
```C++
size_t dp_correlator_execute_power_max (
    dp_correlator_state_t * state,
    const float _Complex * in,
    size_t n_in,
    float * out,
    uint32_t * out1,
    size_t max_out
) 
```



Python: `x` is the H frames back to back, a flat array of H \* ny \* nx samples (H &gt;= 1).


Each cell of `out` is the largest `|map_h|^2` that cell takes over the maps, and the same cell of `out1` is the h it came from. A tie goes to the lowest h: a later map replaces the running maximum only when it is strictly greater.




**Parameters:**


* `state` Must be non-NULL. 
* `in` H frames back to back (CF32); H at most 2^32. 
* `n_in` A nonzero multiple of ny \* nx. 
* `out` n\_out powers (float). 
* `out1` n\_out map indices (uint32). 
* `max_out` Capacity of each of `out` and `out1`; at least n\_out. 



**Returns:**

Python: the tuple (power, index), each (n\_out,): float32 powers and uint32 map indices, always new arrays (this call takes no `out=`); a refused batch, or H above 2\*\*32, is a ValueError. C: n\_out, or SIZE\_MAX with nothing written. 
```C++
>>> import numpy as np
>>> from doppler.spectral import DelayDoppler
>>> rng = np.random.default_rng(0)
>>> ny, nx = 8, 32
>>> code = np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)
>>> dd = DelayDoppler(code, ny=ny)
>>> frames = np.concatenate([np.tile(np.roll(code, d), ny)
...                          for d in (3, 11)])   # delays 3 and 11
>>> power, index = dd.execute_power_max(frames)
>>> int(index[4 * nx + 3]), int(index[4 * nx + 11])  # which map peaks
(0, 1)
```
 





        

<hr>



### function dp\_correlator\_execute\_power\_max\_max\_out 

_Values per output_ [_**dp\_correlator\_execute\_power\_max()**_](correlator__core_8h.md#function-dp_correlator_execute_power_max) _writes: n\_out._
```C++
size_t dp_correlator_execute_power_max_max_out (
    const dp_correlator_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

n\_out, fixed at create(). 
```C++
const float _Complex code[4] = { 1.0f, 1.0f, -1.0f, 1.0f };
dp_correlator_state_t *dd = dp_correlator_create_delay_doppler (code, 4, 2, 1, 1,
                                                         0);
size_t n = dp_correlator_execute_power_max_max_out (dd); // 2 x 4 = 8
dp_correlator_destroy (dd);
return n != 8;
```
 





        

<hr>



### function dp\_correlator\_execute\_power\_sum 

_The power of a batch's maps, summed cell by cell: each output cell is the sum over h of that cell's_ `|map_h|^2` _, summed in h order in float._
```C++
size_t dp_correlator_execute_power_sum (
    dp_correlator_state_t * state,
    const float _Complex * in,
    size_t n_in,
    float * out,
    size_t max_out
) 
```



Python: `x` is the H frames back to back, a flat array of H \* ny \* nx samples (H &gt;= 1).


Unlike the coherent sum, phase does not cancel: frames x and -x sum to twice one map's power, where their coherent sum is zero.




**Parameters:**


* `state` Must be non-NULL. 
* `in` H frames back to back (CF32). 
* `n_in` A nonzero multiple of ny \* nx. 
* `out` n\_out powers (float). 
* `max_out` Capacity of `out`; at least n\_out. 



**Returns:**

Python: the (n\_out,) float32 powers, a new array; a refused batch is a ValueError with nothing written. C: n\_out, or SIZE\_MAX for the refused batch. 
```C++
>>> import numpy as np
>>> from doppler.spectral import DelayDoppler
>>> rng = np.random.default_rng(0)
>>> ny, nx = 8, 32
>>> code = np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)
>>> dd = DelayDoppler(code, ny=ny)
>>> frames = np.concatenate([np.tile(np.roll(code, d), ny)
...                          for d in (3, 11)])   # delays 3 and 11
>>> x = np.tile(code, ny)
>>> power = dd.execute_power_sum(np.concatenate([x, -x]))
>>> one = np.abs(dd.execute_all(x)[0]) ** 2
>>> bool(np.allclose(power, 2 * one))        # x and -x do not cancel
True
```
 





        

<hr>



### function dp\_correlator\_execute\_power\_sum\_max\_out 

_Powers_ [_**dp\_correlator\_execute\_power\_sum()**_](correlator__core_8h.md#function-dp_correlator_execute_power_sum) _writes: n\_out._
```C++
size_t dp_correlator_execute_power_sum_max_out (
    const dp_correlator_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

n\_out, fixed at create(). 
```C++
>>> import numpy as np
>>> from doppler.spectral import DelayDoppler
>>> rng = np.random.default_rng(0)
>>> ny, nx = 8, 32
>>> code = np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)
>>> dd = DelayDoppler(code, ny=ny)
>>> frames = np.concatenate([np.tile(np.roll(code, d), ny)
...                          for d in (3, 11)])   # delays 3 and 11
>>> dd.execute_power_sum_max_out()
256
```
 





        

<hr>



### function dp\_correlator\_get\_state 

_Serialize the frame count and spectrum accumulator into_ `blob` _._
```C++
void dp_correlator_get_state (
    const dp_correlator_state_t * state,
    void * blob
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `blob` Destination; at least [**dp\_correlator\_state\_bytes()**](correlator__core_8h.md#function-dp_correlator_state_bytes) bytes. 




        

<hr>



### function dp\_correlator\_push 

_Stream an arbitrary-length CF32 chunk through a detector flavor._ 
```C++
size_t dp_correlator_push (
    dp_correlator_state_t * state,
    const float _Complex * in,
    size_t n_in,
    dp_correlator_hit_t * result,
    size_t max_results
) 
```



Takes the input in order, runs each complete n-sample frame (n = ny\*nx) through the correlator, and on every dump computes the test statistic peak\_mag / noise\_est. Each dump that passes the threshold is one hit. In Python the result is always a list: of (row, col, peak\_mag, noise\_est, test\_stat) tuples from CorrDetector2D, and of (lag, peak\_mag, noise\_est, test\_stat) tuples from CorrDetector, whose lag is the hit's col.


Python's push() has room for 1024 detections a call, so a push that completes at most 1024 frames, counting the carry, loses nothing. One that fills the room loses the rest of its input, every later frame whether or not it would have made a detection, unless the rest is shorter than a frame: that is kept as the carry. The next push stays on the frame grid only when what was lost is a whole number of frames; otherwise its peaks are off by the remainder. #1992 and just-buildit/just-makeit#2184 track sizing the list to the call.




**Parameters:**


* `state` A detector flavor (non-NULL). 
* `in` CF32 input chunk of arbitrary length. 
* `n_in` Number of input samples in `in`. 
* `result` Caller-supplied array of at least `max_results` hits; filled on return. 
* `max_results` Capacity of `result` (maximum detections to emit). A full `result` never loses input: a frame yields at most one detection, and once `result` is full the push takes the rest of its input only if the rest completes no frame (it is then the carry); otherwise it takes nothing more, stopping on the boundary of its last frame. [**dp\_correlator\_consumed()**](correlator__core_8h.md#function-dp_correlator_consumed) says how many samples it took, and the caller offers the rest again. Input that runs out mid-frame is the carry, held inside (fewer than n samples). With room 0 the push is full from the start, so a resume loop needs room for at least one; with that, a push of any input takes at least one sample. 



**Returns:**

Python: the list of hits, empty when no dump passed the gate. C: the number of hits written to `result`, or SIZE\_MAX (nothing taken, nothing changed) when `state` is a map flavor. 
```C++
>>> from doppler.spectral import CorrDetector, CorrDetector2D
>>> import numpy as np
>>> ref = np.zeros((4, 4), dtype=np.complex64); ref[0, 0] = 1.0
>>> det = CorrDetector2D(ref=ref, dwell=1, noise_lo=1, noise_hi=15,
...                  noise_mode="mean", threshold=0.0)
>>> results = det.push(np.ones((4, 4), dtype=np.complex64))
>>> row, col, peak, noise, stat = results[0]
>>> row, col, round(peak, 4), round(noise, 4), round(stat, 4)
(0, 0, 1.0, 1.0, 1.0)
>>> det1 = CorrDetector(ref=ref[0], dwell=1, noise_lo=1, noise_hi=3,
...                 noise_mode="mean", threshold=0.0)
>>> lag, peak, noise, stat = det1.push(np.ones(4, dtype=np.complex64))[0]
>>> lag, round(peak, 4), round(noise, 4), round(stat, 4)
(0, 1.0, 1.0, 1.0)
```
 





        

<hr>



### function dp\_correlator\_reset 

_Zero the accumulator and reset the integration counter to 0. Equivalent to starting a fresh dwell cycle without rebuilding FFT plans or recomputing ref\_spec._ 
```C++
void dp_correlator_reset (
    dp_correlator_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL.


```C++
>>> from doppler.spectral import Corr2D
>>> import numpy as np
>>> ref = np.zeros((2, 2), dtype=np.complex64); ref[0, 0] = 1.0
>>> c = Corr2D(ref=ref, dwell=3)
>>> _ = c.execute(np.ones((2, 2), dtype=np.complex64))
>>> c.count
1
>>> c.reset()
>>> c.count
0
```
 


        

<hr>



### function dp\_correlator\_set\_ref 

_Replace the reference and recompute its spectrum._ 
```C++
int dp_correlator_set_ref (
    dp_correlator_state_t * state,
    const float _Complex * ref
) 
```



Also resets accumulator and counter on success. The object's fast-path mode (see the file doc comment) is fixed at create() time and never changes here: on a fast-path object, `ref` must still be nonzero only in row 0, or the call is rejected and the object's existing reference and spectrum are left completely untouched (never a silent partial update) — a caller that needs a genuinely different reference shape must build a new [**dp\_correlator\_state\_t**](structdp__correlator__state__t.md). A general-path object accepts any (ny,nx) `ref` and always succeeds. A detector flavor resets first (see [**dp\_correlator\_reset()**](correlator__core_8h.md#function-dp_correlator_reset)), so it is reset even when the reference is then refused.




**Parameters:**


* `state` Must be non-NULL. 
* `ref` New reference, flat row-major CF32, length ny\*nx. 



**Returns:**

0 on success, -1 if `state` is fast-path and `ref` is no longer single-row. 





        

<hr>



### function dp\_correlator\_set\_state 

_Restore a mid-dwell correlator from_ [_**dp\_correlator\_get\_state()**_](correlator__core_8h.md#function-dp_correlator_get_state) _'s blob._
```C++
int dp_correlator_set_state (
    dp_correlator_state_t * state,
    const void * blob
) 
```





**Parameters:**


* `state` Must be non-NULL, created with the same (ny, nx) and reference (the reference is not in the blob). 
* `blob` Source blob of [**dp\_correlator\_state\_bytes()**](correlator__core_8h.md#function-dp_correlator_state_bytes) bytes. 



**Returns:**

`DP_OK`, or `DP_ERR_INVALID` (state untouched) when the header's magic, version, endianness or size does not match, when the blob's flavor, ny, ny\_out, fast\_path, col\_out or nx\_out differs from `state's` (a blob of one flavor never restores into another), when its frame count is not below `state's` dwell, or, for a detector flavor, when noise\_lo, noise\_hi or noise\_mode differs, the framer's child blob is refused, the frames that blob has handed out are not the dwell's position (frames mod dwell must equal the count, as every push, reset and set\_ref keeps it), or the last peak is not a cell of the map. Everything is checked before anything is written. 





        

<hr>



### function dp\_correlator\_set\_threshold 

_Change a detector flavor's threshold without rebuilding it._ 
```C++
int dp_correlator_set_threshold (
    dp_correlator_state_t * state,
    float threshold
) 
```



The value is part of the state blob, so set\_state restores it.




**Parameters:**


* `state` Must be non-NULL. 
* `threshold` New threshold; 0.0 = always fire. 



**Returns:**

DP\_OK, or DP\_ERR\_INVALID (nothing changed) for a map flavor. 





        

<hr>



### function dp\_correlator\_state\_bytes 

_Size in bytes of this correlator's state blob._ 
```C++
size_t dp_correlator_state_bytes (
    const dp_correlator_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Header + flavor, ny, ny\_out, fast\_path, col\_out, nx\_out and frame count (u64 each) + the ny\*nx complex-float accumulator; a detector flavor adds 5 u64, 4 f32 and the framer's child blob. 





        

<hr>



### function dp\_correlator\_state\_check 

_Whether_ [_**dp\_correlator\_set\_state()**_](correlator__core_8h.md#function-dp_correlator_set_state) _would accept_`blob` _, without writing anything._
```C++
int dp_correlator_state_check (
    const dp_correlator_state_t * state,
    const void * blob
) 
```



DP\_OK exactly when set\_state would accept it, and DP\_ERR\_INVALID exactly when it would refuse it: both read the blob through one check. What lets a composition check this child before it restores any other, so a refusal of either changes nothing (carrier\_acq's 'CAQR' does).




**Parameters:**


* `state` Must be non-NULL. 
* `blob` Candidate blob of [**dp\_correlator\_state\_bytes()**](correlator__core_8h.md#function-dp_correlator_state_bytes) bytes. 



**Returns:**

`DP_OK` or `DP_ERR_INVALID`; `state` is never written. 





        

<hr>
## Macro Definition Documentation





### define CORRELATOR\_STATE\_MAGIC 

_State-blob magic ('CORL'); set\_state() rejects any other._ 
```C++
#define CORRELATOR_STATE_MAGIC `DP_FOURCC ('C','O','R','L')`
```




<hr>



### define CORRELATOR\_STATE\_VERSION 

_State-blob layout version; set\_state() rejects any other._ 
```C++
#define CORRELATOR_STATE_VERSION `1u`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/correlator/correlator_core.h`

