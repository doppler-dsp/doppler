

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
|  [**dp\_corr2d\_state\_t**](structdp__corr2d__state__t.md) \* | [**dp\_corr2d\_create**](#function-dp_corr2d_create) (const float \_Complex \* ref, size\_t ny, size\_t nx, size\_t dwell, int nthreads, size\_t ny\_out, size\_t nx\_out, int col\_out) <br>_Allocate a 2-D FFT correlator with coherent integrate-and-dump. Two-dimensional extension of_ [_**dp\_corr\_create()**_](corr__core_8h.md#function-dp_corr_create) _. The reference is a flat row-major ny×nx CF32 array; its conjugate spectrum is pre-computed once so each execute() call costs two 2-D FFTs plus ny\*nx complex multiplies. The Python wrapper requires_`ref` _to be a 2-D ndarray with shape (ny, nx); it passes a flat view to C._ |
|  [**dp\_corr2d\_state\_t**](structdp__corr2d__state__t.md) \* | [**dp\_corr2d\_create\_delay\_doppler**](#function-dp_corr2d_create_delay_doppler) (const float \_Complex \* code, size\_t code\_len, size\_t ny, size\_t dwell, int nthreads, size\_t ny\_out) <br>_Allocate the delay-Doppler flavor: a code correlator that maps a raw stream of_ `ny` _code periods onto (Doppler, delay)._ |
|  void | [**dp\_corr2d\_destroy**](#function-dp_corr2d_destroy) ([**dp\_corr2d\_state\_t**](structdp__corr2d__state__t.md) \* state) <br>_Destroy and free a corr2d instance._  |
|  size\_t | [**dp\_corr2d\_execute**](#function-dp_corr2d_execute) ([**dp\_corr2d\_state\_t**](structdp__corr2d__state__t.md) \* state, const float \_Complex \* in, size\_t n\_in, float \_Complex \* out, size\_t max\_out) <br>_Correlate one 2-D frame and optionally dump the coherent accumulator. Runs the 2-D pipeline: FFT2 → pointwise multiply with ref\_spec → accumulate the cross-spectrum; on dump, IFFT2 → normalise (÷ ny\*nx). Accumulating in the frequency domain and inverting once is exactly the per-frame inverse summed, by linearity of the IFFT — valid because the dwell is_ **coherent** _(a complex sum); a non-coherent (magnitude) integration could not defer the inverse. The Python wrapper accepts a (ny, nx) CF32 ndarray; a dump returns a flat length-ny\*nx ndarray, a no-dump returns None._ |
|  size\_t | [**dp\_corr2d\_execute\_all**](#function-dp_corr2d_execute_all) ([**dp\_corr2d\_state\_t**](structdp__corr2d__state__t.md) \* state, const float \_Complex \* in, size\_t n\_in, float \_Complex \* out, size\_t max\_out) <br>_Every map of a batch, unreduced: map h at_ `out + h * n_out` _._ |
|  size\_t | [**dp\_corr2d\_execute\_all\_max\_out**](#function-dp_corr2d_execute_all_max_out) (const [**dp\_corr2d\_state\_t**](structdp__corr2d__state__t.md) \* state, size\_t n\_in) <br>_Samples_ [_**dp\_corr2d\_execute\_all()**_](corr2d__core_8h.md#function-dp_corr2d_execute_all) _writes for_`n_in` _input samples: H \* n\_out, H = n\_in / (ny \* nx)._ |
|  size\_t | [**dp\_corr2d\_execute\_coherent**](#function-dp_corr2d_execute_coherent) ([**dp\_corr2d\_state\_t**](structdp__corr2d__state__t.md) \* state, const float \_Complex \* in, size\_t n\_in, float \_Complex \* out, size\_t max\_out) <br>_The coherent sum of a batch's maps, inverted once._  |
|  size\_t | [**dp\_corr2d\_execute\_fold**](#function-dp_corr2d_execute_fold) ([**dp\_corr2d\_state\_t**](structdp__corr2d__state__t.md) \* state, const float \_Complex \* in, size\_t n\_in, const [**dp\_corr2d\_fold\_t**](structdp__corr2d__fold__t.md) \* fold) <br>_Make the map of every frame in a batch and hand each to a reducer._  |
|  size\_t | [**dp\_corr2d\_execute\_max\_out**](#function-dp_corr2d_execute_max_out) ([**dp\_corr2d\_state\_t**](structdp__corr2d__state__t.md) \* state) <br>_Maximum output samples per execute call (_ [_**dp\_corr2d\_state\_t::n\_out**_](structdp__corr2d__state__t.md#variable-n_out) __ _ny\_out\*nx\_out normally, or ny when a single_[_**dp\_corr2d\_state\_t::col\_out**_](structdp__corr2d__state__t.md#variable-col_out) _was selected)._ |
|  size\_t | [**dp\_corr2d\_execute\_peaks**](#function-dp_corr2d_execute_peaks) ([**dp\_corr2d\_state\_t**](structdp__corr2d__state__t.md) \* state, const float \_Complex \* in, size\_t n\_in, float \* out, uint32\_t \* out1, uint32\_t \* out2, float \* out3, size\_t max\_out) <br>_One record per map of a batch: its peak power, where the peak is, and its mean power._  |
|  size\_t | [**dp\_corr2d\_execute\_peaks\_max\_out**](#function-dp_corr2d_execute_peaks_max_out) (const [**dp\_corr2d\_state\_t**](structdp__corr2d__state__t.md) \* state, size\_t n\_in) <br>_Records_ [_**dp\_corr2d\_execute\_peaks()**_](corr2d__core_8h.md#function-dp_corr2d_execute_peaks) _writes for_`n_in` _samples: H._ |
|  size\_t | [**dp\_corr2d\_execute\_power\_max**](#function-dp_corr2d_execute_power_max) ([**dp\_corr2d\_state\_t**](structdp__corr2d__state__t.md) \* state, const float \_Complex \* in, size\_t n\_in, float \* out, uint32\_t \* out1, size\_t max\_out) <br>_The largest power over a batch's maps, cell by cell, and the map it came from._  |
|  size\_t | [**dp\_corr2d\_execute\_power\_sum**](#function-dp_corr2d_execute_power_sum) ([**dp\_corr2d\_state\_t**](structdp__corr2d__state__t.md) \* state, const float \_Complex \* in, size\_t n\_in, float \* out, size\_t max\_out) <br>_The power of a batch's maps, summed cell by cell:_ `out[k] = sum_h |map_h[k]|^2` _, summed in h order in float._ |
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

H \* n\_out, or SIZE\_MAX (nothing written) for a refused batch. 





        

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
// The energy of each map, one value per frame.
static void
energy (void *ctx, size_t h, const float _Complex *m, size_t n_out)
{
  double e = 0.0;
  for (size_t k = 0; k < n_out; k++)
    e += crealf (m[k]) * crealf (m[k]) + cimagf (m[k]) * cimagf (m[k]);
  ((double *)ctx)[h] = e;
}

double           e[NFRAMES];
dp_corr2d_fold_t f = { energy, e };
size_t H = dp_corr2d_execute_fold (c, frames, NFRAMES * NY * NX, &f);
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



For map h: `out[h]` is its largest `|map[k]|^2`, `out1[h]` and `out2[h]` the row and column of that cell (the first in row-major order on a tie), and `out3[h]` the mean of `|map[k]|^2` over its n\_out cells. A map is ny\_out x nx\_out; with col\_out it is ny x 1, and the column reported is col\_out. The peak-to-mean of each map is `out[h] / out3[h]`, a statistic of each map, which the same statistic of a pointwise maximum over maps does not give.




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

H, or SIZE\_MAX (nothing written) for a refused batch. 





        

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



`out[k] = max_h |map_h[k]|^2` and `out1[k]` is that h. A tie goes to the lowest h: a later map replaces the running maximum only when it is strictly greater.




**Parameters:**


* `state` Must be non-NULL. 
* `in` H frames back to back (CF32); H at most 2^32. 
* `n_in` A nonzero multiple of ny \* nx. 
* `out` n\_out powers (float). 
* `out1` n\_out map indices (uint32). 
* `max_out` Capacity of each of `out` and `out1`; at least n\_out. 



**Returns:**

n\_out, or SIZE\_MAX (nothing written) for a refused batch or H above 2^32. 





        

<hr>



### function dp\_corr2d\_execute\_power\_sum 

_The power of a batch's maps, summed cell by cell:_ `out[k] = sum_h |map_h[k]|^2` _, summed in h order in float._
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

