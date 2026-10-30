

# Struct dp\_corr2d\_state\_t



[**ClassList**](annotated.md) **>** [**dp\_corr2d\_state\_t**](structdp__corr2d__state__t.md)



_2-D FFT correlator state._ [More...](#detailed-description)

* `#include <corr2d_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  float \_Complex \* | [**accum**](#variable-accum)  <br> |
|  float \_Complex \* | [**bat\_acc**](#variable-bat_acc)  <br> |
|  float \_Complex \* | [**bat\_map**](#variable-bat_map)  <br> |
|  float \_Complex \* | [**bat\_pad**](#variable-bat_pad)  <br> |
|  float \_Complex \* | [**bat\_work**](#variable-bat_work)  <br> |
|  float \_Complex \* | [**col\_gather**](#variable-col_gather)  <br> |
|  int | [**col\_out**](#variable-col_out)  <br> |
|  float \_Complex \* | [**col\_ref**](#variable-col_ref)  <br> |
|  float \_Complex \* | [**col\_spec**](#variable-col_spec)  <br> |
|  size\_t | [**count**](#variable-count)  <br> |
|  int | [**delay\_doppler**](#variable-delay_doppler)  <br> |
|  double | [**doppler\_step**](#variable-doppler_step)  <br> |
|  size\_t | [**dwell**](#variable-dwell)  <br> |
|  int | [**fast\_path**](#variable-fast_path)  <br> |
|  [**dp\_fft2d\_state\_t**](structdp__fft2d__state__t.md) \* | [**fwd**](#variable-fwd)  <br> |
|  [**dp\_fft\_state\_t**](structdp__fft__state__t.md) \* | [**fwd1d**](#variable-fwd1d)  <br> |
|  [**dp\_fft2d\_state\_t**](structdp__fft2d__state__t.md) \* | [**inv**](#variable-inv)  <br> |
|  [**dp\_fft\_state\_t**](structdp__fft__state__t.md) \* | [**inv1d**](#variable-inv1d)  <br> |
|  size\_t | [**n**](#variable-n)  <br> |
|  size\_t | [**n\_out**](#variable-n_out)  <br> |
|  int | [**nworkers**](#variable-nworkers)  <br> |
|  size\_t | [**nx**](#variable-nx)  <br> |
|  size\_t | [**nx\_out**](#variable-nx_out)  <br> |
|  size\_t | [**ny**](#variable-ny)  <br> |
|  size\_t | [**ny\_out**](#variable-ny_out)  <br> |
|  [**dp\_pool\_t**](structdp__pool__t.md) \* | [**pool**](#variable-pool)  <br> |
|  float \_Complex \* | [**ref\_spec**](#variable-ref_spec)  <br> |
|  [**dp\_fft\_state\_t**](structdp__fft__state__t.md) \*\* | [**row\_fwd**](#variable-row_fwd)  <br> |
|  [**dp\_fft\_state\_t**](structdp__fft__state__t.md) \*\* | [**row\_inv**](#variable-row_inv)  <br> |
|  float \_Complex \* | [**row\_ref\_spec**](#variable-row_ref_spec)  <br> |
|  [**dp\_fft\_state\_t**](structdp__fft__state__t.md) \*\* | [**slow\_fft**](#variable-slow_fft)  <br> |
|  float \_Complex \* | [**work\_fft**](#variable-work_fft)  <br> |
|  float \_Complex \* | [**work\_pad**](#variable-work_pad)  <br> |
|  float \_Complex \* | [**work\_trunc**](#variable-work_trunc)  <br> |
|  float \_Complex \* | [**zcol**](#variable-zcol)  <br> |
|  float \_Complex \* | [**zcolout**](#variable-zcolout)  <br> |
|  float \_Complex \* | [**ztmp**](#variable-ztmp)  <br> |












































## Detailed Description


Allocate with [**dp\_corr2d\_create()**](corr2d__core_8h.md#function-dp_corr2d_create); never stack-allocate. All heap buffers are `ny * nx` complex floats stored in row-major order. 


    
## Public Attributes Documentation




### variable accum 

```C++
float _Complex* dp_corr2d_state_t::accum;
```



Coherent product-spectrum accumulator, same (ny,nx)/reinterpretation rule as work\_fft. 
 


        

<hr>



### variable bat\_acc 

```C++
float _Complex* dp_corr2d_state_t::bat_acc;
```



Per-worker accumulators, nworkers x n; zero between maps. 
 


        

<hr>



### variable bat\_map 

```C++
float _Complex* dp_corr2d_state_t::bat_map;
```



Per-worker maps, nworkers x n\_out. 
 


        

<hr>



### variable bat\_pad 

```C++
float _Complex* dp_corr2d_state_t::bat_pad;
```



Per-worker row zero-pad, nworkers x ny x nx\_out; fast path with nx\_out &gt; nx only. 


        

<hr>



### variable bat\_work 

```C++
float _Complex* dp_corr2d_state_t::bat_work;
```



Per-worker forward spectra, nworkers x n. 


        

<hr>



### variable col\_gather 

```C++
float _Complex* dp_corr2d_state_t::col_gather;
```



Per-worker column tiles, nworkers x CORR2D\_COL\_TILE x ny\_out; rows ny.. ny\_out-1 of each column stay zero (the slow-time zero-pad). Delay-Doppler. 
 


        

<hr>



### variable col\_out 

```C++
int dp_corr2d_state_t::col_out;
```



Output column, or &lt; 0 for the full map. 


        

<hr>



### variable col\_ref 

```C++
float _Complex* dp_corr2d_state_t::col_ref;
```



conj(ref row 0) in the TIME domain, length nx; NULL when col\_out &lt; 0. 
 


        

<hr>



### variable col\_spec 

```C++
float _Complex* dp_corr2d_state_t::col_spec;
```



Per-worker transformed tiles, same shape as [**col\_gather**](structdp__corr2d__state__t.md#variable-col_gather). 
 


        

<hr>



### variable count 

```C++
size_t dp_corr2d_state_t::count;
```



Frames accumulated (0 … dwell-1). 
 


        

<hr>



### variable delay\_doppler 

```C++
int dp_corr2d_state_t::delay_doppler;
```



1 for the delay-Doppler flavor. 
 


        

<hr>



### variable doppler\_step 

```C++
double dp_corr2d_state_t::doppler_step;
```



Doppler spacing between adjacent output rows, cycles/sample: 1 / (ny\_out \* nx). 0 for Corr2D. 
 


        

<hr>



### variable dwell 

```C++
size_t dp_corr2d_state_t::dwell;
```



Integration depth. 
 


        

<hr>



### variable fast\_path 

```C++
int dp_corr2d_state_t::fast_path;
```



1 if using the 1-D-per-row fast path. 
 


        

<hr>



### variable fwd 

```C++
dp_fft2d_state_t* dp_corr2d_state_t::fwd;
```



Forward 2-D plan (sign = -1) at (ny, nx). NULL when [**fast\_path**](structdp__corr2d__state__t.md#variable-fast_path). 
 


        

<hr>



### variable fwd1d 

```C++
dp_fft_state_t* dp_corr2d_state_t::fwd1d;
```



Forward 1-D plan, length nx. Fast only. 


        

<hr>



### variable inv 

```C++
dp_fft2d_state_t* dp_corr2d_state_t::inv;
```



Inverse 2-D plan (sign = +1) at (ny\_out,…). NULL when [**fast\_path**](structdp__corr2d__state__t.md#variable-fast_path). 
 


        

<hr>



### variable inv1d 

```C++
dp_fft_state_t* dp_corr2d_state_t::inv1d;
```



Inverse 1-D plan, length nx\_out. Fast only. 
 


        

<hr>



### variable n 

```C++
size_t dp_corr2d_state_t::n;
```



ny \* nx — total element count. 
 


        

<hr>



### variable n\_out 

```C++
size_t dp_corr2d_state_t::n_out;
```



ny\_out \* nx\_out — output element count. 
 


        

<hr>



### variable nworkers 

```C++
int dp_corr2d_state_t::nworkers;
```



Chunks per stage (1 = serial). 
 


        

<hr>



### variable nx 

```C++
size_t dp_corr2d_state_t::nx;
```



Column count. 
 


        

<hr>



### variable nx\_out 

```C++
size_t dp_corr2d_state_t::nx_out;
```



Output columns (== nx unless decoupled). 


        

<hr>



### variable ny 

```C++
size_t dp_corr2d_state_t::ny;
```



Row count. 
 


        

<hr>



### variable ny\_out 

```C++
size_t dp_corr2d_state_t::ny_out;
```



Output rows (== ny unless decoupled). 
 


        

<hr>



### variable pool 

```C++
dp_pool_t* dp_corr2d_state_t::pool;
```



Persistent workers; NULL when serial. 
 


        

<hr>



### variable ref\_spec 

```C++
float _Complex* dp_corr2d_state_t::ref_spec;
```



conj(FFT2(ref)), pre-computed. (ny, nx). NULL when [**fast\_path**](structdp__corr2d__state__t.md#variable-fast_path) (see row\_ref\_spec). 


        

<hr>



### variable row\_fwd 

```C++
dp_fft_state_t** dp_corr2d_state_t::row_fwd;
```



Per-worker forward row plans, length nx; worker 0's is [**fwd1d**](structdp__corr2d__state__t.md#variable-fwd1d). Fast path only. 
 


        

<hr>



### variable row\_inv 

```C++
dp_fft_state_t** dp_corr2d_state_t::row_inv;
```



Per-worker inverse row plans, length nx\_out; worker 0's is [**inv1d**](structdp__corr2d__state__t.md#variable-inv1d). Fast path only. 
 


        

<hr>



### variable row\_ref\_spec 

```C++
float _Complex* dp_corr2d_state_t::row_ref_spec;
```



conj(FFT\_nx(ref row 0)), length nx. Fast-path replacement for ref\_spec. 
 


        

<hr>



### variable slow\_fft 

```C++
dp_fft_state_t** dp_corr2d_state_t::slow_fft;
```



Per-worker slow-time plans, length ny\_out. Delay-Doppler only. 
 


        

<hr>



### variable work\_fft 

```C++
float _Complex* dp_corr2d_state_t::work_fft;
```



Scratch: FFT(in)·ref\_spec product. (ny,nx) either path — fast path reinterprets this as ny independent length-nx row spectra. 
 


        

<hr>



### variable work\_pad 

```C++
float _Complex* dp_corr2d_state_t::work_pad;
```



Zero-padded product, (ny\_out, nx\_out) or, fast path, (ny, nx\_out). 
 


        

<hr>



### variable work\_trunc 

```C++
float _Complex* dp_corr2d_state_t::work_trunc;
```



Bounce buffer for a dump into an under-sized `out` (jm gh-138). Both execute paths  the 2-D inverse and the per-row fast path  write the full n\_out surface, so a short `out` is served by writing here and copying the prefix. Allocated lazily; the sized path never touches it. 


        

<hr>



### variable zcol 

```C++
float _Complex* dp_corr2d_state_t::zcol;
```



Column gather scratch, (ny). General path only. 
 


        

<hr>



### variable zcolout 

```C++
float _Complex* dp_corr2d_state_t::zcolout;
```



Column-padded scratch, (ny\_out). General path only. 
 


        

<hr>



### variable ztmp 

```C++
float _Complex* dp_corr2d_state_t::ztmp;
```



Row-padded intermediate, (ny, nx\_out). General path only. 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/corr2d/corr2d_core.h`

