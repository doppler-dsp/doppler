

# Struct dp\_correlator\_state\_t



[**ClassList**](annotated.md) **>** [**dp\_correlator\_state\_t**](structdp__correlator__state__t.md)



_Correlator state, every flavor._ [More...](#detailed-description)

* `#include <correlator_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  unsigned char | [**\_hot\_pad**](#variable-_hot_pad)  <br> |
|  int | [**\_last\_corr\_valid**](#variable-_last_corr_valid)  <br> |
|  float \_Complex \* | [**accum**](#variable-accum)  <br> |
|  uint8\_t \* | [**band\_mask**](#variable-band_mask)  <br> |
|  float \_Complex \* | [**bat\_acc**](#variable-bat_acc)  <br> |
|  float \_Complex \* | [**bat\_fine**](#variable-bat_fine)  <br> |
|  float \_Complex \* | [**bat\_map**](#variable-bat_map)  <br> |
|  float \_Complex \* | [**bat\_pad**](#variable-bat_pad)  <br> |
|  float \_Complex \* | [**bat\_work**](#variable-bat_work)  <br> |
|  int | [**col\_out**](#variable-col_out)  <br> |
|  float \_Complex \* | [**col\_ref**](#variable-col_ref)  <br> |
|  size\_t | [**consumed**](#variable-consumed)  <br> |
|  size\_t | [**count**](#variable-count)  <br> |
|  float \_Complex \* | [**dd\_fine**](#variable-dd_fine)  <br> |
|  int | [**dd\_fused**](#variable-dd_fused)  <br> |
|  struct dp\_\_dd\_map \* | [**dd\_map**](#variable-dd_map)  <br> |
|  size\_t \* | [**dd\_rows**](#variable-dd_rows)  <br> |
|  int | [**delay\_doppler**](#variable-delay_doppler)  <br> |
|  float \_Complex \* | [**det\_out**](#variable-det_out)  <br> |
|  double | [**doppler\_step**](#variable-doppler_step)  <br> |
|  size\_t | [**dwell**](#variable-dwell)  <br> |
|  int | [**fast\_path**](#variable-fast_path)  <br> |
|  [**dp\_correlator\_flavor\_t**](correlator__core_8h.md#enum-dp_correlator_flavor_t) | [**flavor**](#variable-flavor)  <br> |
|  dp\_f32\_framer\_t | [**framer**](#variable-framer)  <br> |
|  [**dp\_fft2d\_state\_t**](structdp__fft2d__state__t.md) \* | [**fwd**](#variable-fwd)  <br> |
|  [**dp\_fft\_state\_t**](structdp__fft__state__t.md) \* | [**fwd1d**](#variable-fwd1d)  <br> |
|  [**dp\_fft2d\_state\_t**](structdp__fft2d__state__t.md) \* | [**inv**](#variable-inv)  <br> |
|  [**dp\_fft\_state\_t**](structdp__fft__state__t.md) \* | [**inv1d**](#variable-inv1d)  <br> |
|  float \* | [**mag\_buf**](#variable-mag_buf)  <br> |
|  size\_t | [**max\_peaks**](#variable-max_peaks)  <br> |
|  size\_t | [**n**](#variable-n)  <br> |
|  size\_t | [**n\_out**](#variable-n_out)  <br> |
|  size\_t | [**n\_peaks**](#variable-n_peaks)  <br> |
|  size\_t | [**n\_twins**](#variable-n_twins)  <br> |
|  float | [**noise\_est**](#variable-noise_est)  <br> |
|  size\_t | [**noise\_hi**](#variable-noise_hi)  <br> |
|  size\_t | [**noise\_lo**](#variable-noise_lo)  <br> |
|  [**det\_noise\_mode\_t**](correlator__core_8h.md#enum-det_noise_mode_t) | [**noise\_mode**](#variable-noise_mode)  <br> |
|  float \* | [**noise\_scratch**](#variable-noise_scratch)  <br> |
|  int | [**nworkers**](#variable-nworkers)  <br> |
|  size\_t | [**nx**](#variable-nx)  <br> |
|  size\_t | [**nx\_out**](#variable-nx_out)  <br> |
|  size\_t | [**ny**](#variable-ny)  <br> |
|  size\_t | [**ny\_out**](#variable-ny_out)  <br> |
|  size\_t | [**peak\_col**](#variable-peak_col)  <br> |
|  float | [**peak\_mag**](#variable-peak_mag)  <br> |
|  uint8\_t \* | [**peak\_mask**](#variable-peak_mask)  <br> |
|  size\_t | [**peak\_row**](#variable-peak_row)  <br> |
|  [**det\_peak\_t**](structdet__peak__t.md) \* | [**peaks**](#variable-peaks)  <br> |
|  [**dp\_pool\_t**](structdp__pool__t.md) \* | [**pool**](#variable-pool)  <br> |
|  float \_Complex \* | [**ref\_spec**](#variable-ref_spec)  <br> |
|  dp\_f32\_t \* | [**ring**](#variable-ring)  <br> |
|  size\_t | [**ring\_cap**](#variable-ring_cap)  <br> |
|  [**dp\_fft\_state\_t**](structdp__fft__state__t.md) \*\* | [**row\_fwd**](#variable-row_fwd)  <br> |
|  [**dp\_fft\_state\_t**](structdp__fft__state__t.md) \*\* | [**row\_inv**](#variable-row_inv)  <br> |
|  float \_Complex \* | [**row\_ref\_spec**](#variable-row_ref_spec)  <br> |
|  [**dp\_correlator\_search\_t**](correlator__core_8h.md#typedef-dp_correlator_search_t) \* | [**search**](#variable-search)  <br> |
|  float | [**test\_stat**](#variable-test_stat)  <br> |
|  float | [**threshold**](#variable-threshold)  <br> |
|  uint32\_t \* | [**twin\_col**](#variable-twin_col)  <br> |
|  uint32\_t \* | [**twin\_row**](#variable-twin_row)  <br> |
|  float \_Complex \* | [**work\_fft**](#variable-work_fft)  <br> |
|  float \_Complex \* | [**work\_pad**](#variable-work_pad)  <br> |
|  float \_Complex \* | [**work\_trunc**](#variable-work_trunc)  <br> |
|  float \_Complex \* | [**zcol**](#variable-zcol)  <br> |
|  float \_Complex \* | [**zcolout**](#variable-zcolout)  <br> |
|  float \_Complex \* | [**ztmp**](#variable-ztmp)  <br> |












































## Detailed Description


Allocate with a dp\_correlator\_create\_\*() constructor; never stack-allocate. All heap buffers are `ny * nx` complex floats stored in row-major order. 


    
## Public Attributes Documentation




### variable \_hot\_pad 

```C++
unsigned char dp_correlator_state_t::_hot_pad[64];
```



Keeps [**count**](structdp__correlator__state__t.md#variable-count) off the read fields' cache lines. 
 


        

<hr>



### variable \_last\_corr\_valid 

```C++
int dp_correlator_state_t::_last_corr_valid;
```



1 once a dump has filled det\_out; 0 after create, reset and set\_state. 
 


        

<hr>



### variable accum 

```C++
float _Complex* dp_correlator_state_t::accum;
```



Coherent product-spectrum accumulator, same (ny,nx)/reinterpretation rule as work\_fft. 
 


        

<hr>



### variable band\_mask 

```C++
uint8_t* dp_correlator_state_t::band_mask;
```



n\_surf; 1 = outside the searched band. 
 


        

<hr>



### variable bat\_acc 

```C++
float _Complex* dp_correlator_state_t::bat_acc;
```



Per-worker accumulators, nworkers x n; zero between maps. 
 


        

<hr>



### variable bat\_fine 

```C++
float _Complex* dp_correlator_state_t::bat_fine;
```



Per-worker [**dd\_fine**](structdp__correlator__state__t.md#variable-dd_fine) for batches; NULL when the map runs rows first. 
 


        

<hr>



### variable bat\_map 

```C++
float _Complex* dp_correlator_state_t::bat_map;
```



Per-worker maps, nworkers x n\_out. 
 


        

<hr>



### variable bat\_pad 

```C++
float _Complex* dp_correlator_state_t::bat_pad;
```



Per-worker row zero-pad, nworkers x ny x nx\_out; fast path with nx\_out &gt; nx only. 


        

<hr>



### variable bat\_work 

```C++
float _Complex* dp_correlator_state_t::bat_work;
```



Per-worker forward spectra, nworkers x n. 


        

<hr>



### variable col\_out 

```C++
int dp_correlator_state_t::col_out;
```



Output column, or &lt; 0 for the full map. 


        

<hr>



### variable col\_ref 

```C++
float _Complex* dp_correlator_state_t::col_ref;
```



conj(ref row 0) in the TIME domain, length nx; NULL when col\_out &lt; 0. 
 


        

<hr>



### variable consumed 

```C++
size_t dp_correlator_state_t::consumed;
```



Input samples the last push took. 
 


        

<hr>



### variable count 

```C++
size_t dp_correlator_state_t::count;
```



Frames accumulated (0 … dwell-1). 
 


        

<hr>



### variable dd\_fine 

```C++
float _Complex* dp_correlator_state_t::dd_fine;
```



The map's scratch when it runs whole (slow first, or one slow row); NULL when it runs rows first, fused into the row stage. Delay-Doppler. 
 


        

<hr>



### variable dd\_fused 

```C++
int dp_correlator_state_t::dd_fused;
```



1 when the map runs rows first (its rule at one tile, one rate): the dump frame's rows are inverted as they are accumulated. 
 


        

<hr>



### variable dd\_map 

```C++
struct dp__dd_map* dp_correlator_state_t::dd_map;
```



The one delay-Doppler block map (docs/design/acq-surface.md §18.1), one slot per worker. Delay-Doppler only; internal. 
 


        

<hr>



### variable dd\_rows 

```C++
size_t* dp_correlator_state_t::dd_rows;
```



Output row of slow row i: the centered (i + ny\_out/2) mod ny\_out. 
 


        

<hr>



### variable delay\_doppler 

```C++
int dp_correlator_state_t::delay_doppler;
```



1 for the delay-Doppler flavor. 
 


        

<hr>



### variable det\_out 

```C++
float _Complex* dp_correlator_state_t::det_out;
```



The last dump's correlation map, n complex samples. 
 


        

<hr>



### variable doppler\_step 

```C++
double dp_correlator_state_t::doppler_step;
```



Doppler spacing between adjacent output rows, cycles/sample: 1 / (ny\_out \* nx). 0 for Corr2D. 
 


        

<hr>



### variable dwell 

```C++
size_t dp_correlator_state_t::dwell;
```



Integration depth. 
 


        

<hr>



### variable fast\_path 

```C++
int dp_correlator_state_t::fast_path;
```



1 if using the 1-D-per-row fast path. 
 


        

<hr>



### variable flavor 

```C++
dp_correlator_flavor_t dp_correlator_state_t::flavor;
```



The constructor that built it. 
 


        

<hr>



### variable framer 

```C++
dp_f32_framer_t dp_correlator_state_t::framer;
```



Any chunk in, n-sample frames out. 
 


        

<hr>



### variable fwd 

```C++
dp_fft2d_state_t* dp_correlator_state_t::fwd;
```



Forward 2-D plan (sign = -1) at (ny, nx). NULL when [**fast\_path**](structdp__correlator__state__t.md#variable-fast_path). 
 


        

<hr>



### variable fwd1d 

```C++
dp_fft_state_t* dp_correlator_state_t::fwd1d;
```



Forward 1-D plan, length nx. Fast only. 


        

<hr>



### variable inv 

```C++
dp_fft2d_state_t* dp_correlator_state_t::inv;
```



Inverse 2-D plan (sign = +1) at (ny\_out,…). NULL when [**fast\_path**](structdp__correlator__state__t.md#variable-fast_path). 
 


        

<hr>



### variable inv1d 

```C++
dp_fft_state_t* dp_correlator_state_t::inv1d;
```



Inverse 1-D plan, length nx\_out. Fast only. 
 


        

<hr>



### variable mag\_buf 

```C++
float* dp_correlator_state_t::mag_buf;
```



\|det\_out\|, n floats. 
 


        

<hr>



### variable max\_peaks 

```C++
size_t dp_correlator_state_t::max_peaks;
```



Peak-list capacity per dwell (1 = the maximum only); [**dp\_correlator\_set\_max\_peaks()**](correlator__core_8h.md#function-dp_correlator_set_max_peaks). 
 


        

<hr>



### variable n 

```C++
size_t dp_correlator_state_t::n;
```



ny \* nx — total element count. 
 


        

<hr>



### variable n\_out 

```C++
size_t dp_correlator_state_t::n_out;
```



ny\_out \* nx\_out — output element count. 
 


        

<hr>



### variable n\_peaks 

```C++
size_t dp_correlator_state_t::n_peaks;
```



Search flavors: picks in the last dwell, held ones too. 
 


        

<hr>



### variable n\_twins 

```C++
size_t dp_correlator_state_t::n_twins;
```



Search flavors: picks carried over from the last dwell. 
 


        

<hr>



### variable noise\_est 

```C++
float dp_correlator_state_t::noise_est;
```



Last dump: the noise floor. 
 


        

<hr>



### variable noise\_hi 

```C++
size_t dp_correlator_state_t::noise_hi;
```



Noise bin range, last (inclusive). 
 


        

<hr>



### variable noise\_lo 

```C++
size_t dp_correlator_state_t::noise_lo;
```



Noise bin range, first (inclusive). 


        

<hr>



### variable noise\_mode 

```C++
det_noise_mode_t dp_correlator_state_t::noise_mode;
```



Noise aggregation. 
 


        

<hr>



### variable noise\_scratch 

```C++
float* dp_correlator_state_t::noise_scratch;
```



Scratch for the median's sort. 
 


        

<hr>



### variable nworkers 

```C++
int dp_correlator_state_t::nworkers;
```



Chunks per stage (1 = serial). 
 


        

<hr>



### variable nx 

```C++
size_t dp_correlator_state_t::nx;
```



Column count. 
 


        

<hr>



### variable nx\_out 

```C++
size_t dp_correlator_state_t::nx_out;
```



Output columns (== nx unless decoupled). 


        

<hr>



### variable ny 

```C++
size_t dp_correlator_state_t::ny;
```



Row count. 
 


        

<hr>



### variable ny\_out 

```C++
size_t dp_correlator_state_t::ny_out;
```



Output rows (== ny unless decoupled). 
 


        

<hr>



### variable peak\_col 

```C++
size_t dp_correlator_state_t::peak_col;
```



Last dump: peak column (the lag at one row). 
 


        

<hr>



### variable peak\_mag 

```C++
float dp_correlator_state_t::peak_mag;
```



Last dump: peak \|R\|. 
 


        

<hr>



### variable peak\_mask 

```C++
uint8_t* dp_correlator_state_t::peak_mask;
```



n\_surf; the working mask. 
 


        

<hr>



### variable peak\_row 

```C++
size_t dp_correlator_state_t::peak_row;
```



Last dump: peak row (0 before one). 
 


        

<hr>



### variable peaks 

```C++
det_peak_t* dp_correlator_state_t::peaks;
```



max\_peaks: the last dwell's picks. 
 


        

<hr>



### variable pool 

```C++
dp_pool_t* dp_correlator_state_t::pool;
```



Persistent workers; NULL when serial. 
 


        

<hr>



### variable ref\_spec 

```C++
float _Complex* dp_correlator_state_t::ref_spec;
```



conj(FFT2(ref)), pre-computed. (ny, nx). NULL when [**fast\_path**](structdp__correlator__state__t.md#variable-fast_path) (see row\_ref\_spec). 


        

<hr>



### variable ring 

```C++
dp_f32_t* dp_correlator_state_t::ring;
```



The carry's storage: bound to [**framer**](structdp__correlator__state__t.md#variable-framer), freed by destroy. 
 


        

<hr>



### variable ring\_cap 

```C++
size_t dp_correlator_state_t::ring_cap;
```



Ring capacity, complex samples. 
 


        

<hr>



### variable row\_fwd 

```C++
dp_fft_state_t** dp_correlator_state_t::row_fwd;
```



Per-worker forward row plans, length nx; worker 0's is [**fwd1d**](structdp__correlator__state__t.md#variable-fwd1d). Fast path only. 
 


        

<hr>



### variable row\_inv 

```C++
dp_fft_state_t** dp_correlator_state_t::row_inv;
```



Per-worker inverse row plans, length nx\_out; worker 0's is [**inv1d**](structdp__correlator__state__t.md#variable-inv1d). Fast path only. 
 


        

<hr>



### variable row\_ref\_spec 

```C++
float _Complex* dp_correlator_state_t::row_ref_spec;
```



conj(FFT\_nx(ref row 0)), length nx. Fast-path replacement for ref\_spec. 
 


        

<hr>



### variable search 

```C++
dp_correlator_search_t* dp_correlator_state_t::search;
```



The search policy, allocated once at create; NULL unless a search flavor. 
 


        

<hr>



### variable test\_stat 

```C++
float dp_correlator_state_t::test_stat;
```



Last dump: peak\_mag / noise\_est, or 0. 


        

<hr>



### variable threshold 

```C++
float dp_correlator_state_t::threshold;
```



0 = always fire; &gt; 0 gates test\_stat. 
 


        

<hr>



### variable twin\_col 

```C++
uint32_t* dp_correlator_state_t::twin_col;
```



max\_peaks: last dwell's picks, code phases. 


        

<hr>



### variable twin\_row 

```C++
uint32_t* dp_correlator_state_t::twin_row;
```



max\_peaks: last dwell's picks, native rows. 


        

<hr>



### variable work\_fft 

```C++
float _Complex* dp_correlator_state_t::work_fft;
```



Scratch: FFT(in)·ref\_spec product. (ny,nx) either path — fast path reinterprets this as ny independent length-nx row spectra. 
 


        

<hr>



### variable work\_pad 

```C++
float _Complex* dp_correlator_state_t::work_pad;
```



Zero-padded product, (ny\_out, nx\_out) or, fast path, (ny, nx\_out). 
 


        

<hr>



### variable work\_trunc 

```C++
float _Complex* dp_correlator_state_t::work_trunc;
```



Bounce buffer for a dump into an under-sized `out` (jm gh-138). Both execute paths  the 2-D inverse and the per-row fast path  write the full n\_out surface, so a short `out` is served by writing here and copying the prefix. Allocated lazily; the sized path never touches it. 


        

<hr>



### variable zcol 

```C++
float _Complex* dp_correlator_state_t::zcol;
```



Column gather scratch, (ny). General path only. 
 


        

<hr>



### variable zcolout 

```C++
float _Complex* dp_correlator_state_t::zcolout;
```



Column-padded scratch, (ny\_out). General path only. 
 


        

<hr>



### variable ztmp 

```C++
float _Complex* dp_correlator_state_t::ztmp;
```



Row-padded intermediate, (ny, nx\_out). General path only. 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/correlator/correlator_core.h`

