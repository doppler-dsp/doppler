

# File corr2d\_core.h



[**FileList**](files.md) **>** [**corr2d**](dir_ac96ca94cfbb355eb7a82b081cfe387c.md) **>** [**corr2d\_core.h**](corr2d__core_8h.md)

[Go to the source code of this file](corr2d__core_8h_source.md)

_2-D FFT-based cross-correlator with coherent integrate-and-dump._ [More...](#detailed-description)

* `#include "doppler/clib_common.h"`
* `#include "doppler/dp_parallel.h"`
* `#include "doppler/dp_state.h"`
* `#include "doppler/fft/fft_core.h"`
* `#include "doppler/fft2d/fft2d_core.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**dp\_corr2d\_fold\_t**](structdp__corr2d__fold__t.md) <br>_A C reducer: called once per map, in h order, by_ [_**dp\_corr2d\_execute\_fold()**_](corr2d__core_8h.md#function-dp_corr2d_execute_fold) _._ |
| struct | [**dp\_corr2d\_state\_t**](structdp__corr2d__state__t.md) <br>_2-D FFT correlator state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  size\_t | [**dp\_corr2d\_bank\_detect**](#function-dp_corr2d_bank_detect) (const [**dp\_corr2d\_state\_t**](structdp__corr2d__state__t.md) \* state, const float \_Complex \* x, size\_t x\_len, size\_t L\_d, size\_t L\_r, int g\_min, int g\_max, float t\_candidate, float t\_confirm, const float \_Complex \* rates, size\_t rates\_len, float \* out, int32\_t \* out1, uint32\_t \* out2, uint32\_t \* out3, uint32\_t \* out4, size\_t max\_out) <br>_Detect on a coarse carrier grid, refine each candidate at its own cell on a fine one, and keep what confirms._  |
|  size\_t | [**dp\_corr2d\_bank\_detect\_max\_out**](#function-dp_corr2d_bank_detect_max_out) ([**dp\_corr2d\_state\_t**](structdp__corr2d__state__t.md) \* state, size\_t x\_len) <br>_Capacity_ [_**dp\_corr2d\_bank\_detect()**_](corr2d__core_8h.md#function-dp_corr2d_bank_detect) _needs: one record per map cell._ |
|  size\_t | [**dp\_corr2d\_bank\_fold**](#function-dp_corr2d_bank_fold) (const [**dp\_corr2d\_state\_t**](structdp__corr2d__state__t.md) \* state, const float \_Complex \* x, size\_t x\_len, size\_t L, int g\_min, int g\_max, const float \_Complex \* rates, size\_t rates\_len, const [**dp\_corr2d\_fold\_t**](structdp__corr2d__fold__t.md) \* fold) <br>_Run one frame through a bank of carrier and rate hypotheses and hand each map to a reducer._  |
|  size\_t | [**dp\_corr2d\_bank\_power\_max**](#function-dp_corr2d_bank_power_max) (const [**dp\_corr2d\_state\_t**](structdp__corr2d__state__t.md) \* state, const float \_Complex \* x, size\_t x\_len, size\_t L, int g\_min, int g\_max, const float \_Complex \* rates, size\_t rates\_len, float \* out, uint32\_t \* out1, size\_t max\_out) <br>_The strongest power at every cell over a bank, and which hypothesis it came from._  |
|  size\_t | [**dp\_corr2d\_bank\_power\_max\_max\_out**](#function-dp_corr2d_bank_power_max_max_out) ([**dp\_corr2d\_state\_t**](structdp__corr2d__state__t.md) \* state, size\_t x\_len) <br>_Capacity_ [_**dp\_corr2d\_bank\_power\_max()**_](corr2d__core_8h.md#function-dp_corr2d_bank_power_max) _needs: one map._ |
|  [**dp\_corr2d\_state\_t**](structdp__corr2d__state__t.md) \* | [**dp\_corr2d\_create**](#function-dp_corr2d_create) (const float \_Complex \* ref, size\_t ny, size\_t nx, size\_t dwell, int nthreads, size\_t ny\_out, size\_t nx\_out, int col\_out) <br>_Allocate a 2-D FFT correlator with coherent integrate-and-dump. Two-dimensional extension of_ [_**dp\_corr\_create()**_](corr__core_8h.md#function-dp_corr_create) _. The reference is a flat row-major ny×nx CF32 array; its conjugate spectrum is pre-computed once so each execute() call costs two 2-D FFTs plus ny\*nx complex multiplies. The Python wrapper requires_`ref` _to be a 2-D ndarray with shape (ny, nx); it passes a flat view to C._ |
|  [**dp\_corr2d\_state\_t**](structdp__corr2d__state__t.md) \* | [**dp\_corr2d\_create\_delay\_doppler**](#function-dp_corr2d_create_delay_doppler) (const float \_Complex \* code, size\_t code\_len, size\_t ny, size\_t dwell, int nthreads, size\_t ny\_out) <br>_Allocate the delay-Doppler flavor: a code correlator that maps a raw stream of_ `ny` _code periods onto (Doppler, delay)._ |
|  void | [**dp\_corr2d\_destroy**](#function-dp_corr2d_destroy) ([**dp\_corr2d\_state\_t**](structdp__corr2d__state__t.md) \* state) <br>_Destroy and free a corr2d instance._  |
|  size\_t | [**dp\_corr2d\_execute**](#function-dp_corr2d_execute) ([**dp\_corr2d\_state\_t**](structdp__corr2d__state__t.md) \* state, const float \_Complex \* in, size\_t n\_in, float \_Complex \* out, size\_t max\_out) <br>_Correlate one 2-D frame and optionally dump the coherent accumulator. Runs the 2-D pipeline: FFT2 → pointwise multiply with ref\_spec → accumulate the cross-spectrum; on dump, IFFT2 → normalise (÷ ny\*nx). Accumulating in the frequency domain and inverting once is exactly the per-frame inverse summed, by linearity of the IFFT — valid because the dwell is_ **coherent** _(a complex sum); a non-coherent (magnitude) integration could not defer the inverse. The Python wrapper accepts a (ny, nx) CF32 ndarray; a dump returns a flat length-ny\*nx ndarray, a no-dump returns None._ |
|  size\_t | [**dp\_corr2d\_execute\_all**](#function-dp_corr2d_execute_all) ([**dp\_corr2d\_state\_t**](structdp__corr2d__state__t.md) \* state, const float \_Complex \* in, size\_t n\_in, float \_Complex \* out, size\_t max\_out) <br>_Every map of a batch, unreduced: map h at_ `out + h * n_out` _._ |
|  size\_t | [**dp\_corr2d\_execute\_all\_max\_out**](#function-dp_corr2d_execute_all_max_out) (const [**dp\_corr2d\_state\_t**](structdp__corr2d__state__t.md) \* state, size\_t n\_in) <br>_Samples_ [_**dp\_corr2d\_execute\_all()**_](corr2d__core_8h.md#function-dp_corr2d_execute_all) _writes for_`n_in` _input samples: H \* n\_out, H = n\_in / (ny \* nx)._ |
|  size\_t | [**dp\_corr2d\_execute\_coherent**](#function-dp_corr2d_execute_coherent) ([**dp\_corr2d\_state\_t**](structdp__corr2d__state__t.md) \* state, const float \_Complex \* in, size\_t n\_in, float \_Complex \* out, size\_t max\_out) <br>_The coherent sum of a batch's maps, inverted once._  |
|  size\_t | [**dp\_corr2d\_execute\_coherent\_max\_out**](#function-dp_corr2d_execute_coherent_max_out) (const [**dp\_corr2d\_state\_t**](structdp__corr2d__state__t.md) \* state) <br>_Samples_ [_**dp\_corr2d\_execute\_coherent()**_](corr2d__core_8h.md#function-dp_corr2d_execute_coherent) _writes: n\_out._ |
|  size\_t | [**dp\_corr2d\_execute\_fold**](#function-dp_corr2d_execute_fold) ([**dp\_corr2d\_state\_t**](structdp__corr2d__state__t.md) \* state, const float \_Complex \* in, size\_t n\_in, const [**dp\_corr2d\_fold\_t**](structdp__corr2d__fold__t.md) \* fold) <br>_Make the map of every frame in a batch and hand each to a reducer._  |
|  size\_t | [**dp\_corr2d\_execute\_max\_out**](#function-dp_corr2d_execute_max_out) ([**dp\_corr2d\_state\_t**](structdp__corr2d__state__t.md) \* state) <br>_Maximum output samples per execute call (_ [_**dp\_corr2d\_state\_t::n\_out**_](structdp__corr2d__state__t.md#variable-n_out) __ _ny\_out\*nx\_out normally, or ny when a single_[_**dp\_corr2d\_state\_t::col\_out**_](structdp__corr2d__state__t.md#variable-col_out) _was selected)._ |
|  size\_t | [**dp\_corr2d\_execute\_peaks**](#function-dp_corr2d_execute_peaks) ([**dp\_corr2d\_state\_t**](structdp__corr2d__state__t.md) \* state, const float \_Complex \* in, size\_t n\_in, float \* out, uint32\_t \* out1, uint32\_t \* out2, float \* out3, size\_t max\_out) <br>_One record per map of a batch: its peak power, where the peak is, and its mean power._  |
|  size\_t | [**dp\_corr2d\_execute\_peaks\_max\_out**](#function-dp_corr2d_execute_peaks_max_out) (const [**dp\_corr2d\_state\_t**](structdp__corr2d__state__t.md) \* state, size\_t n\_in) <br>_Records_ [_**dp\_corr2d\_execute\_peaks()**_](corr2d__core_8h.md#function-dp_corr2d_execute_peaks) _writes for_`n_in` _samples: H._ |
|  size\_t | [**dp\_corr2d\_execute\_power\_max**](#function-dp_corr2d_execute_power_max) ([**dp\_corr2d\_state\_t**](structdp__corr2d__state__t.md) \* state, const float \_Complex \* in, size\_t n\_in, float \* out, uint32\_t \* out1, size\_t max\_out) <br>_The largest power over a batch's maps, cell by cell, and the map it came from._  |
|  size\_t | [**dp\_corr2d\_execute\_power\_max\_max\_out**](#function-dp_corr2d_execute_power_max_max_out) (const [**dp\_corr2d\_state\_t**](structdp__corr2d__state__t.md) \* state) <br>_Values per output_ [_**dp\_corr2d\_execute\_power\_max()**_](corr2d__core_8h.md#function-dp_corr2d_execute_power_max) _writes: n\_out._ |
|  size\_t | [**dp\_corr2d\_execute\_power\_sum**](#function-dp_corr2d_execute_power_sum) ([**dp\_corr2d\_state\_t**](structdp__corr2d__state__t.md) \* state, const float \_Complex \* in, size\_t n\_in, float \* out, size\_t max\_out) <br>_The power of a batch's maps, summed cell by cell: each output cell is the sum over h of that cell's_ `|map_h|^2` _, summed in h order in float._ |
|  size\_t | [**dp\_corr2d\_execute\_power\_sum\_max\_out**](#function-dp_corr2d_execute_power_sum_max_out) (const [**dp\_corr2d\_state\_t**](structdp__corr2d__state__t.md) \* state) <br>_Powers_ [_**dp\_corr2d\_execute\_power\_sum()**_](corr2d__core_8h.md#function-dp_corr2d_execute_power_sum) _writes: n\_out._ |
|  void | [**dp\_corr2d\_get\_state**](#function-dp_corr2d_get_state) (const [**dp\_corr2d\_state\_t**](structdp__corr2d__state__t.md) \* state, void \* blob) <br>_Serialize the frame count and spectrum accumulator into_ `blob` _._ |
|  void | [**dp\_corr2d\_reset**](#function-dp_corr2d_reset) ([**dp\_corr2d\_state\_t**](structdp__corr2d__state__t.md) \* state) <br>_Zero the accumulator and reset the integration counter to 0. Equivalent to starting a fresh dwell cycle without rebuilding FFT plans or recomputing ref\_spec._  |
|  int | [**dp\_corr2d\_set\_ref**](#function-dp_corr2d_set_ref) ([**dp\_corr2d\_state\_t**](structdp__corr2d__state__t.md) \* state, const float \_Complex \* ref) <br>_Replace the reference and recompute its spectrum._  |
|  int | [**dp\_corr2d\_set\_state**](#function-dp_corr2d_set_state) ([**dp\_corr2d\_state\_t**](structdp__corr2d__state__t.md) \* state, const void \* blob) <br>_Restore a mid-dwell correlator from_ [_**dp\_corr2d\_get\_state()**_](corr2d__core_8h.md#function-dp_corr2d_get_state) _'s blob._ |
|  size\_t | [**dp\_corr2d\_state\_bytes**](#function-dp_corr2d_state_bytes) (const [**dp\_corr2d\_state\_t**](structdp__corr2d__state__t.md) \* state) <br>_Size in bytes of this correlator's state blob._  |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**CORR2D\_STATE\_MAGIC**](corr2d__core_8h.md#define-corr2d_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('C','R','2','D')`<br>_State-blob magic ('CR2D'); set\_state() rejects any other._  |
| define  | [**CORR2D\_STATE\_VERSION**](corr2d__core_8h.md#define-corr2d_state_version)  `1u`<br>_State-blob layout version; set\_state() rejects any other._  |
| define  | [**DELAY\_DOPPLER\_STATE\_MAGIC**](corr2d__core_8h.md#define-delay_doppler_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('D','D','M','P')`<br>_The delay-Doppler flavor's state-blob magic ('DDMP'): a Corr2D blob and a delay-Doppler blob each refuse to restore into the other._  |
| define  | [**DELAY\_DOPPLER\_STATE\_VERSION**](corr2d__core_8h.md#define-delay_doppler_state_version)  `1u`<br>_The delay-Doppler flavor's blob layout version._  |

## Detailed Description


Two-dimensional extension of corr\_core: all buffers are ny×nx row-major flat arrays of length ny\*nx. The correlation theorem extends naturally:


`R_xh[i,j] = IFFT2( FFT2(x) · conj(FFT2(h)) ) / (ny*nx)`


The reference spectrum is pre-computed at create time. The int-dump semantics are identical to the 1-D case: coherently sum `dwell` frames, then dump.


**Single-row-reference fast path.** When `ref` is nonzero only in row 0 (the DSSS acquisition/detection shape: a code replica with no Doppler content of its own) and `ny_out` == `ny` (no row-axis interpolation), `ref_spec[u,v]` is independent of `u` — the row axis of the 2-D transform pair cancels to an exact identity (DFT orthogonality: sum\_u exp(2\*pi\*i\*u\*(i-i)/ny) = ny`iff`i==i'`, else 0), for *any* row content.`dp\_corr2d\_execute`then reduces exactly to `R(i,j) = IFFT\_nx(FFT\_nx(row\_i) · conj(FFT\_nx(ref\_row0)))(j) / nx`, applied independently per row — so`dp\_corr2d\_create`detects this shape and runs`ny`independent length-`nx`1-D FFTs instead of a full`(ny,nx)` 2-D FFT, skipping the row-axis work that would otherwise cancel to a no-op. Any other reference shape, or`ny\_out &gt; ny`, uses the general 2-D path unchanged.


Lifecycle: 
```C++
float _Complex ref[NY * NX] = { ... };    // row-major 2-D reference
dp_corr2d_state_t *c = dp_corr2d_create(ref, NY, NX, 4, 1, 0, 0, -1);
float _Complex out[NY * NX];
for (int i = 0; i < 4; i++) {
    size_t n_out = dp_corr2d_execute(c, frame[i], NY*NX, out, NY*NX);
    if (n_out) process_2d(out, NY, NX);   // fires once, on i == 3
}
dp_corr2d_destroy(c);
```
 


    
## Public Functions Documentation




### function dp\_corr2d\_bank\_detect 

_Detect on a coarse carrier grid, refine each candidate at its own cell on a fine one, and keep what confirms._ 
```C++
size_t dp_corr2d_bank_detect (
    const dp_corr2d_state_t * state,
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



* **Detect.** [**dp\_corr2d\_bank\_power\_max()**](corr2d__core_8h.md#function-dp_corr2d_bank_power_max) over carriers `g_min` .. `g_max` on the `1 / (L_d nx)` grid, with every rate.
* **Candidates.** Every cell whose power exceeds `t_candidate` and is at least each of its 8 neighbors, both axes circular (a delay lag and a Doppler bin both wrap).
* **Refine.** For a candidate found by coarse carrier g\_d and rate r, the map value at its cell under every fine carrier g\_r from `m g_d - (m - 1)` to `m g_d + (m - 1)`, m = `L_r / L_d`: the carriers between its coarse neighbors, on the `1 / (L_r nx)` grid. On the fast path and DelayDoppler with a native delay axis each is a time-domain correlation at the cell's lag and a slow-time DFT at its Doppler row, in double, with no FFT: `2 ny nx` multiply-adds a carrier. Any other object makes that hypothesis's map and reads the cell. The best g\_r wins, the lowest on a tie.
* **Confirm.** A candidate whose refined power exceeds `t_confirm` is a record.




Records come out in cell order (row-major), one entry of each output per record: `out` the refined power, `out1` g\_r on the fine grid, `out2` the rate r, `out3` the row and `out4` the column, the cell as execute() lays out the map. Measured at 64 x 512 against an oracle that corrects the exact offset, `L_d` = 2 with `L_r` = 4 holds L = 4's sensitivity within 0.16 dB, for about half the bank (the design page). `L_d` = `L_r` is the one-stage bank. Thresholds are powers in the map's own units; the caller sets them for its noise, as for any detector.




**Parameters:**


* `state` As [**dp\_corr2d\_bank\_fold()**](corr2d__core_8h.md#function-dp_corr2d_bank_fold); `g_min` and `g_max` are on the coarse grid. 
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

The record count (0 included), or SIZE\_MAX with nothing written for anything [**dp\_corr2d\_bank\_fold()**](corr2d__core_8h.md#function-dp_corr2d_bank_fold) refuses, `L_d == 0`, an `L_r` that `L_d` does not divide, a refined carrier past int32, or `max_out` below `n_out`. 
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



### function dp\_corr2d\_bank\_detect\_max\_out 

_Capacity_ [_**dp\_corr2d\_bank\_detect()**_](corr2d__core_8h.md#function-dp_corr2d_bank_detect) _needs: one record per map cell._
```C++
size_t dp_corr2d_bank_detect_max_out (
    dp_corr2d_state_t * state,
    size_t x_len
) 
```





**Parameters:**


* `state` The correlator. 
* `x_len` The frame's length (the bound does not depend on it). 



**Returns:**

`n_out`. 





        

<hr>



### function dp\_corr2d\_bank\_fold 

_Run one frame through a bank of carrier and rate hypotheses and hand each map to a reducer._ 
```C++
size_t dp_corr2d_bank_fold (
    const dp_corr2d_state_t * state,
    const float _Complex * x,
    size_t x_len,
    size_t L,
    int g_min,
    int g_max,
    const float _Complex * rates,
    size_t rates_len,
    const dp_corr2d_fold_t * fold
) 
```



A bank is what a frame batch of corrected copies would compute, without the copies: carrier g corrects an offset of `g / (L * nx)` cycles/sample, for every g from `g_min` to `g_max`, and each rate r weights the slow-time samples by the r-th ny-long vector of `rates`. Map h, for `h = (g - g_min) * R + r` (R = `rates_len / ny`, or 1 with no rates), is what a fresh dwell-1 object's execute() returns for the frame with period i multiplied by element i of rate vector r and by `exp(-j 2 pi g n / (L nx))`, where n counts samples WITHIN period i (0 .. nx-1), up to float rounding.


The correction restarts at every period on purpose: the phase a carrier adds between periods is slow-time Doppler, which the slow-time FFT measures, so a signal stays on its own absolute Doppler row under every carrier hypothesis (docs/design/corr2d-hypotheses.md, rule 4).


Every path runs a bank, and only the single-row fast path and DelayDoppler share work: whole bins (g a multiple of L) are a roll of the row spectrum, each fraction `g mod L` mixes and transforms the rows once for every whole bin that shares it, and each carrier inverts its rows once for all its rates. On DelayDoppler a rate pays only the slow-time FFT; on Corr2D, whose rows are its periods, it scales the carrier's rows. The general 2-D path and a known column run each hypothesis's corrected frame through their own frame code, which is the definition with nothing shared.


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
#include "doppler/corr2d/corr2d_core.h"
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
  dp_corr2d_state_t *dd = dp_corr2d_create_delay_doppler (code, 8, 4, 1, 1, 0);
  size_t maps = 0;
  dp_corr2d_fold_t f = { count_maps, &maps };
  size_t h = dp_corr2d_bank_fold (dd, x, 4 * 8, 4, -4, 4, NULL, 0, &f);
  printf ("%zu %zu\n", h, maps); // 9 carriers, one rate: 9 maps
  dp_corr2d_destroy (dd);
  return !(h == 9 && maps == 9);
}
```
 





        

<hr>



### function dp\_corr2d\_bank\_power\_max 

_The strongest power at every cell over a bank, and which hypothesis it came from._ 
```C++
size_t dp_corr2d_bank_power_max (
    const dp_corr2d_state_t * state,
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



[**dp\_corr2d\_bank\_fold()**](corr2d__core_8h.md#function-dp_corr2d_bank_fold) with the `POWER_MAX` reducer: at each cell, `out` holds the largest \|map\_h\|^2 over every hypothesis h, and `out1` that h, `(g - g_min) * R + r`, the lowest h on a tie. Every argument and refusal is [**dp\_corr2d\_bank\_fold()**](corr2d__core_8h.md#function-dp_corr2d_bank_fold)'s; in addition `max_out` below `n_out` is refused with nothing written.




**Parameters:**


* `state` As [**dp\_corr2d\_bank\_fold()**](corr2d__core_8h.md#function-dp_corr2d_bank_fold). 
* `out` `n_out` powers. 
* `out1` `n_out` hypothesis indices. 
* `max_out` Capacity of each output; at least `n_out`. 



**Returns:**

`n_out`, or SIZE\_MAX with nothing written. 
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



### function dp\_corr2d\_bank\_power\_max\_max\_out 

_Capacity_ [_**dp\_corr2d\_bank\_power\_max()**_](corr2d__core_8h.md#function-dp_corr2d_bank_power_max) _needs: one map._
```C++
size_t dp_corr2d_bank_power_max_max_out (
    dp_corr2d_state_t * state,
    size_t x_len
) 
```





**Parameters:**


* `state` The correlator. 
* `x_len` The frame's length (one map whatever it is). 



**Returns:**

`n_out`. 





        

<hr>



### function dp\_corr2d\_create 

_Allocate a 2-D FFT correlator with coherent integrate-and-dump. Two-dimensional extension of_ [_**dp\_corr\_create()**_](corr__core_8h.md#function-dp_corr_create) _. The reference is a flat row-major ny×nx CF32 array; its conjugate spectrum is pre-computed once so each execute() call costs two 2-D FFTs plus ny\*nx complex multiplies. The Python wrapper requires_`ref` _to be a 2-D ndarray with shape (ny, nx); it passes a flat view to C._
```C++
dp_corr2d_state_t * dp_corr2d_create (
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



### function dp\_corr2d\_create\_delay\_doppler 

_Allocate the delay-Doppler flavor: a code correlator that maps a raw stream of_ `ny` _code periods onto (Doppler, delay)._
```C++
dp_corr2d_state_t * dp_corr2d_create_delay_doppler (
    const float _Complex * code,
    size_t code_len,
    size_t ny,
    size_t dwell,
    int nthreads,
    size_t ny_out
) 
```



Input to [**dp\_corr2d\_execute()**](corr2d__core_8h.md#function-dp_corr2d_execute) is the raw stream  `ny` periods of `code_len` samples, back to back, no reshaping or reference padding by the caller. Each period (row) is correlated against `code` exactly as Corr2D's single-row fast path does (FFT, multiply by `conj(FFT(code))`, coherent `dwell` accumulation of the product spectrum, inverse scaled by `1/nx`), and on the dump the slow-time FFT runs down every delay column. In numpy terms, for one frame `x` (dwell 1):  The output is `ny_out x nx` row-major: rows are Doppler bins, centered (zero Doppler on row `ny_out / 2`), columns are delay lags. A signal of amplitude `a` that is the code delayed by `d` samples and rotated by `f` cycles/sample peaks at column `d` and row `ny_out/2 + f / doppler_step`, where [**dp\_corr2d\_state\_t::doppler\_step**](structdp__corr2d__state__t.md#variable-doppler_step) is `1 / (ny_out * nx)` cycles/sample (times the sample rate for Hz). Its peak is `a * dwell * ny * sum(abs(code)**2)`: the 1/nx of the row inverse cancels the row correlation's own gain of nx, and the slow-time FFT is unnormalized, as numpy's is, so it sums the ny rows coherently.


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

Heap-allocated state (free with [**dp\_corr2d\_destroy()**](corr2d__core_8h.md#function-dp_corr2d_destroy)), or NULL if `code` is NULL, `code_len` or `ny` is 0, `dwell` is 0, or `ny_out` is below `ny`. 
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



### function dp\_corr2d\_destroy 

_Destroy and free a corr2d instance._ 
```C++
void dp_corr2d_destroy (
    dp_corr2d_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function dp\_corr2d\_execute 

_Correlate one 2-D frame and optionally dump the coherent accumulator. Runs the 2-D pipeline: FFT2 → pointwise multiply with ref\_spec → accumulate the cross-spectrum; on dump, IFFT2 → normalise (÷ ny\*nx). Accumulating in the frequency domain and inverting once is exactly the per-frame inverse summed, by linearity of the IFFT — valid because the dwell is_ **coherent** _(a complex sum); a non-coherent (magnitude) integration could not defer the inverse. The Python wrapper accepts a (ny, nx) CF32 ndarray; a dump returns a flat length-ny\*nx ndarray, a no-dump returns None._
```C++
size_t dp_corr2d_execute (
    dp_corr2d_state_t * state,
    const float _Complex * in,
    size_t n_in,
    float _Complex * out,
    size_t max_out
) 
```





**Parameters:**


* `state` Allocated 2-D correlator (non-NULL). 
* `in` Input frame, flat row-major CF32, length ny\*nx. 
* `n_in` Number of input samples; must equal ny\*nx; else REFUSED. 
* `out` Output buffer for the correlation map (CF32, length n\_out: ny\_out\*nx\_out, or ny with `col_out`); written only on a dump call. 
* `max_out` Capacity of `out` in elements. Emission stops there, so the return value is the number actually written. 



**Returns:**

n\_out on a dump (or `max_out` if smaller), 0 otherwise (None in Python). If `n_in` is not the required length the call is REFUSED with nothing read, written or counted: SIZE\_MAX in C, ValueError in Python (#1925). 
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



### function dp\_corr2d\_execute\_all 

_Every map of a batch, unreduced: map h at_ `out + h * n_out` _._
```C++
size_t dp_corr2d_execute_all (
    dp_corr2d_state_t * state,
    const float _Complex * in,
    size_t n_in,
    float _Complex * out,
    size_t max_out
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `in` H frames back to back (CF32). 
* `n_in` A nonzero multiple of ny \* nx. 
* `out` H \* n\_out values (CF32). 
* `max_out` Capacity of `out`; at least [**dp\_corr2d\_execute\_all\_max\_out()**](corr2d__core_8h.md#function-dp_corr2d_execute_all_max_out). 



**Returns:**

H \* n\_out, or SIZE\_MAX (nothing written) for a refused batch. From Python, an (H, n\_out) array, a view of `out=` when given. 
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



### function dp\_corr2d\_execute\_all\_max\_out 

_Samples_ [_**dp\_corr2d\_execute\_all()**_](corr2d__core_8h.md#function-dp_corr2d_execute_all) _writes for_`n_in` _input samples: H \* n\_out, H = n\_in / (ny \* nx)._
```C++
size_t dp_corr2d_execute_all_max_out (
    const dp_corr2d_state_t * state,
    size_t n_in
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `n_in` Input samples about to be passed. 



**Returns:**

H \* n\_out, or SIZE\_MAX if that overflows. 
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



### function dp\_corr2d\_execute\_coherent 

_The coherent sum of a batch's maps, inverted once._ 
```C++
size_t dp_corr2d_execute_coherent (
    dp_corr2d_state_t * state,
    const float _Complex * in,
    size_t n_in,
    float _Complex * out,
    size_t max_out
) 
```



The frames' product spectra are summed and the sum is inverted, as a dwell does: the result is bit-identical to what a fresh object with dwell = H dumps for the same frames, and equal to the sum of the H maps up to rounding. The object's own dwell is untouched.




**Parameters:**


* `state` Must be non-NULL. 
* `in` H frames back to back (CF32). 
* `n_in` A nonzero multiple of ny \* nx. 
* `out` n\_out values (CF32). 
* `max_out` Capacity of `out`; at least n\_out. 



**Returns:**

n\_out, or SIZE\_MAX (nothing written) for a refused batch. 
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



### function dp\_corr2d\_execute\_coherent\_max\_out 

_Samples_ [_**dp\_corr2d\_execute\_coherent()**_](corr2d__core_8h.md#function-dp_corr2d_execute_coherent) _writes: n\_out._
```C++
size_t dp_corr2d_execute_coherent_max_out (
    const dp_corr2d_state_t * state
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



### function dp\_corr2d\_execute\_fold 

_Make the map of every frame in a batch and hand each to a reducer._ 
```C++
size_t dp_corr2d_execute_fold (
    dp_corr2d_state_t * state,
    const float _Complex * in,
    size_t n_in,
    const dp_corr2d_fold_t * fold
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
#include "doppler/corr2d/corr2d_core.h"
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
  dp_corr2d_state_t   *dd
      = dp_corr2d_create_delay_doppler (code, 4, 2, 1, 1, 0);
  float _Complex frames[3 * 2 * 4]; // 3 frames of ny x nx = 2 x 4
  for (size_t k = 0; k < 3 * 2 * 4; k++)
    frames[k] = code[k % 4] * (float)(1 + k / 8); // frame h is (h+1) x
  double           e[3];
  dp_corr2d_fold_t f = { energy, e };
  size_t           H = dp_corr2d_execute_fold (dd, frames, 3 * 2 * 4, &f);
  printf ("%zu maps, energy ratios %.0f %.0f\n", H, e[1] / e[0],
          e[2] / e[0]); // 3 maps, energy ratios 4 9
  dp_corr2d_destroy (dd);
  return !(H == 3 && e[1] > 3.99 * e[0] && e[1] < 4.01 * e[0]);
}
```
 





        

<hr>



### function dp\_corr2d\_execute\_max\_out 

_Maximum output samples per execute call (_ [_**dp\_corr2d\_state\_t::n\_out**_](structdp__corr2d__state__t.md#variable-n_out) __ _ny\_out\*nx\_out normally, or ny when a single_[_**dp\_corr2d\_state\_t::col\_out**_](structdp__corr2d__state__t.md#variable-col_out) _was selected)._
```C++
size_t dp_corr2d_execute_max_out (
    dp_corr2d_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

n\_out, fixed at create(). 





        

<hr>



### function dp\_corr2d\_execute\_peaks 

_One record per map of a batch: its peak power, where the peak is, and its mean power._ 
```C++
size_t dp_corr2d_execute_peaks (
    dp_corr2d_state_t * state,
    const float _Complex * in,
    size_t n_in,
    float * out,
    uint32_t * out1,
    uint32_t * out2,
    float * out3,
    size_t max_out
) 
```



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

H, or SIZE\_MAX (nothing written) for a refused batch. From Python, the tuple (peak, row, col, mean). 
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



### function dp\_corr2d\_execute\_peaks\_max\_out 

_Records_ [_**dp\_corr2d\_execute\_peaks()**_](corr2d__core_8h.md#function-dp_corr2d_execute_peaks) _writes for_`n_in` _samples: H._
```C++
size_t dp_corr2d_execute_peaks_max_out (
    const dp_corr2d_state_t * state,
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
dp_corr2d_state_t *dd = dp_corr2d_create_delay_doppler (code, 4, 2, 1, 1,
                                                         0);
size_t h = dp_corr2d_execute_peaks_max_out (dd, 3 * 2 * 4); // 3 frames
dp_corr2d_destroy (dd);
return h != 3;
```
 





        

<hr>



### function dp\_corr2d\_execute\_power\_max 

_The largest power over a batch's maps, cell by cell, and the map it came from._ 
```C++
size_t dp_corr2d_execute_power_max (
    dp_corr2d_state_t * state,
    const float _Complex * in,
    size_t n_in,
    float * out,
    uint32_t * out1,
    size_t max_out
) 
```



Each cell of `out` is the largest `|map_h|^2` that cell takes over the maps, and the same cell of `out1` is the h it came from. A tie goes to the lowest h: a later map replaces the running maximum only when it is strictly greater.




**Parameters:**


* `state` Must be non-NULL. 
* `in` H frames back to back (CF32); H at most 2^32. 
* `n_in` A nonzero multiple of ny \* nx. 
* `out` n\_out powers (float). 
* `out1` n\_out map indices (uint32). 
* `max_out` Capacity of each of `out` and `out1`; at least n\_out. 



**Returns:**

n\_out, or SIZE\_MAX (nothing written) for a refused batch or H above 2^32. From Python, the tuple (power, index). 
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



### function dp\_corr2d\_execute\_power\_max\_max\_out 

_Values per output_ [_**dp\_corr2d\_execute\_power\_max()**_](corr2d__core_8h.md#function-dp_corr2d_execute_power_max) _writes: n\_out._
```C++
size_t dp_corr2d_execute_power_max_max_out (
    const dp_corr2d_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

n\_out, fixed at create(). 
```C++
const float _Complex code[4] = { 1.0f, 1.0f, -1.0f, 1.0f };
dp_corr2d_state_t *dd = dp_corr2d_create_delay_doppler (code, 4, 2, 1, 1,
                                                         0);
size_t n = dp_corr2d_execute_power_max_max_out (dd); // 2 x 4 = 8
dp_corr2d_destroy (dd);
return n != 8;
```
 





        

<hr>



### function dp\_corr2d\_execute\_power\_sum 

_The power of a batch's maps, summed cell by cell: each output cell is the sum over h of that cell's_ `|map_h|^2` _, summed in h order in float._
```C++
size_t dp_corr2d_execute_power_sum (
    dp_corr2d_state_t * state,
    const float _Complex * in,
    size_t n_in,
    float * out,
    size_t max_out
) 
```



Unlike the coherent sum, phase does not cancel: frames x and -x sum to twice one map's power, where their coherent sum is zero.




**Parameters:**


* `state` Must be non-NULL. 
* `in` H frames back to back (CF32). 
* `n_in` A nonzero multiple of ny \* nx. 
* `out` n\_out powers (float). 
* `max_out` Capacity of `out`; at least n\_out. 



**Returns:**

n\_out, or SIZE\_MAX (nothing written) for a refused batch. 
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



### function dp\_corr2d\_execute\_power\_sum\_max\_out 

_Powers_ [_**dp\_corr2d\_execute\_power\_sum()**_](corr2d__core_8h.md#function-dp_corr2d_execute_power_sum) _writes: n\_out._
```C++
size_t dp_corr2d_execute_power_sum_max_out (
    const dp_corr2d_state_t * state
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



### function dp\_corr2d\_get\_state 

_Serialize the frame count and spectrum accumulator into_ `blob` _._
```C++
void dp_corr2d_get_state (
    const dp_corr2d_state_t * state,
    void * blob
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `blob` Destination; at least [**dp\_corr2d\_state\_bytes()**](corr2d__core_8h.md#function-dp_corr2d_state_bytes) bytes. 




        

<hr>



### function dp\_corr2d\_reset 

_Zero the accumulator and reset the integration counter to 0. Equivalent to starting a fresh dwell cycle without rebuilding FFT plans or recomputing ref\_spec._ 
```C++
void dp_corr2d_reset (
    dp_corr2d_state_t * state
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



### function dp\_corr2d\_set\_ref 

_Replace the reference and recompute its spectrum._ 
```C++
int dp_corr2d_set_ref (
    dp_corr2d_state_t * state,
    const float _Complex * ref
) 
```



Also resets accumulator and counter on success. The object's fast-path mode (see the file doc comment) is fixed at create() time and never changes here: on a fast-path object, `ref` must still be nonzero only in row 0, or the call is rejected and the object's existing reference and spectrum are left completely untouched (never a silent partial update) — a caller that needs a genuinely different reference shape must build a new [**dp\_corr2d\_state\_t**](structdp__corr2d__state__t.md). A general-path object accepts any (ny,nx) `ref` and always succeeds.




**Parameters:**


* `state` Must be non-NULL. 
* `ref` New reference, flat row-major CF32, length ny\*nx. 



**Returns:**

0 on success, -1 if `state` is fast-path and `ref` is no longer single-row. 





        

<hr>



### function dp\_corr2d\_set\_state 

_Restore a mid-dwell correlator from_ [_**dp\_corr2d\_get\_state()**_](corr2d__core_8h.md#function-dp_corr2d_get_state) _'s blob._
```C++
int dp_corr2d_set_state (
    dp_corr2d_state_t * state,
    const void * blob
) 
```





**Parameters:**


* `state` Must be non-NULL, created with the same (ny, nx) and reference (the reference is not in the blob). 
* `blob` Source blob of [**dp\_corr2d\_state\_bytes()**](corr2d__core_8h.md#function-dp_corr2d_state_bytes) bytes. 



**Returns:**

`DP_OK`, or `DP_ERR_INVALID` (state untouched) when the header's magic, version, endianness or size does not match  including a blob of the other flavor  or, for the delay-Doppler flavor, when the blob's ny or ny\_out differs from `state's` or its frame count is not below `state's` dwell. 





        

<hr>



### function dp\_corr2d\_state\_bytes 

_Size in bytes of this correlator's state blob._ 
```C++
size_t dp_corr2d_state_bytes (
    const dp_corr2d_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Header + frame count (u64) + ny\*nx complex-float accumulator; the delay-Doppler flavor adds its ny and ny\_out (u64 each). 





        

<hr>
## Macro Definition Documentation





### define CORR2D\_STATE\_MAGIC 

_State-blob magic ('CR2D'); set\_state() rejects any other._ 
```C++
#define CORR2D_STATE_MAGIC `DP_FOURCC ('C','R','2','D')`
```




<hr>



### define CORR2D\_STATE\_VERSION 

_State-blob layout version; set\_state() rejects any other._ 
```C++
#define CORR2D_STATE_VERSION `1u`
```




<hr>



### define DELAY\_DOPPLER\_STATE\_MAGIC 

_The delay-Doppler flavor's state-blob magic ('DDMP'): a Corr2D blob and a delay-Doppler blob each refuse to restore into the other._ 
```C++
#define DELAY_DOPPLER_STATE_MAGIC `DP_FOURCC ('D','D','M','P')`
```




<hr>



### define DELAY\_DOPPLER\_STATE\_VERSION 

_The delay-Doppler flavor's blob layout version._ 
```C++
#define DELAY_DOPPLER_STATE_VERSION `1u`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/corr2d/corr2d_core.h`

