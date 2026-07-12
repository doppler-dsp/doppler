

# Struct acq\_state\_t



[**ClassList**](annotated.md) **>** [**acq\_state\_t**](structacq__state__t.md)



_Streaming acquisition-engine state._ [More...](#detailed-description)

* `#include <acq_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  double | [**chip\_rate**](#variable-chip_rate)  <br> |
|  double | [**cn0\_dbhz**](#variable-cn0_dbhz)  <br> |
|  size\_t | [**code\_bins**](#variable-code_bins)  <br> |
|  float complex \* | [**colbuf**](#variable-colbuf)  <br> |
|  float complex \* | [**colout**](#variable-colout)  <br> |
|  [**corr2d\_state\_t**](structcorr2d__state__t.md) \* | [**corr**](#variable-corr)  <br> |
|  size\_t | [**doppler\_bins**](#variable-doppler_bins)  <br> |
|  double | [**doppler\_res\_hz**](#variable-doppler_res_hz)  <br> |
|  double | [**doppler\_span\_hz**](#variable-doppler_span_hz)  <br> |
|  float | [**eta**](#variable-eta)  <br> |
|  float | [**eta\_nc**](#variable-eta_nc)  <br> |
|  double | [**fs**](#variable-fs)  <br> |
|  float \* | [**mag\_buf**](#variable-mag_buf)  <br> |
|  size\_t | [**max\_noncoh**](#variable-max_noncoh)  <br> |
|  size\_t | [**n**](#variable-n)  <br> |
|  size\_t | [**n\_noncoh**](#variable-n_noncoh)  <br> |
|  size\_t | [**nc\_count**](#variable-nc_count)  <br> |
|  float \* | [**nc\_surface**](#variable-nc_surface)  <br> |
|  float | [**noise\_est**](#variable-noise_est)  <br> |
|  size\_t | [**noise\_hi**](#variable-noise_hi)  <br> |
|  size\_t | [**noise\_lo**](#variable-noise_lo)  <br> |
|  [**det\_noise\_mode\_t**](detector__core_8h.md#enum-det_noise_mode_t) | [**noise\_mode**](#variable-noise_mode)  <br> |
|  float \* | [**noise\_scratch**](#variable-noise_scratch)  <br> |
|  float complex \* | [**out\_buf**](#variable-out_buf)  <br> |
|  double | [**pd**](#variable-pd)  <br> |
|  double | [**pd\_predicted**](#variable-pd_predicted)  <br> |
|  size\_t | [**peak\_col**](#variable-peak_col)  <br> |
|  float | [**peak\_mag**](#variable-peak_mag)  <br> |
|  size\_t | [**peak\_row**](#variable-peak_row)  <br> |
|  double | [**pfa\_cell**](#variable-pfa_cell)  <br> |
|  float complex \* | [**ref**](#variable-ref)  <br> |
|  size\_t | [**reps**](#variable-reps)  <br> |
|  dp\_f32\_t \* | [**ring**](#variable-ring)  <br> |
|  size\_t | [**ring\_cap**](#variable-ring_cap)  <br> |
|  uint64\_t | [**samples\_consumed**](#variable-samples_consumed)  <br> |
|  size\_t | [**searched\_bins**](#variable-searched_bins)  <br> |
|  size\_t | [**sf**](#variable-sf)  <br> |
|  [**fft\_state\_t**](structfft__state__t.md) \* | [**slow\_fft**](#variable-slow_fft)  <br> |
|  size\_t | [**spc**](#variable-spc)  <br> |
|  double | [**straddle\_loss**](#variable-straddle_loss)  <br> |
|  float | [**test\_stat**](#variable-test_stat)  <br> |
|  float | [**threshold**](#variable-threshold)  <br> |
|  uint8\_t | [**underpowered**](#variable-underpowered)  <br> |
|  float complex \* | [**yframe**](#variable-yframe)  <br> |












































## Detailed Description


Allocate with [**acq\_create()**](acq__core_8h.md#function-acq_create); never stack-allocate. 


    
## Public Attributes Documentation




### variable chip\_rate 

```C++
double acq_state_t::chip_rate;
```



Chip rate (Hz). 


        

<hr>



### variable cn0\_dbhz 

```C++
double acq_state_t::cn0_dbhz;
```



Sensitivity used to size the search (dB-Hz). 


        

<hr>



### variable code\_bins 

```C++
size_t acq_state_t::code_bins;
```



One segment in samples = sf\*spc. 


        

<hr>



### variable colbuf 

```C++
float complex* acq_state_t::colbuf;
```



Gathered column scratch (doppler\_bins). 


        

<hr>



### variable colout 

```C++
float complex* acq_state_t::colout;
```



FFT'd column scratch (doppler\_bins). 


        

<hr>



### variable corr 

```C++
corr2d_state_t* acq_state_t::corr;
```



Single-row-ref correlator (dwell=1). 


        

<hr>



### variable doppler\_bins 

```C++
size_t acq_state_t::doppler_bins;
```



Coherent depth = slow-time FFT length (&lt;= reps). 


        

<hr>



### variable doppler\_res\_hz 

```C++
double acq_state_t::doppler_res_hz;
```



Doppler bin width = chip\_rate/(sf\*doppler\_bins). 


        

<hr>



### variable doppler\_span\_hz 

```C++
double acq_state_t::doppler_span_hz;
```



Native Doppler half-range = chip\_rate/(2\*sf). 


        

<hr>



### variable eta 

```C++
float acq_state_t::eta;
```



Raw per-cell Rayleigh amplitude threshold. 


        

<hr>



### variable eta\_nc 

```C++
float acq_state_t::eta_nc;
```



Non-coherent CFAR threshold (order-N\_nc Marcum). 


        

<hr>



### variable fs 

```C++
double acq_state_t::fs;
```



Sample rate (Hz) = chip\_rate \* spc. 


        

<hr>



### variable mag\_buf 

```C++
float* acq_state_t::mag_buf;
```



\|out\_buf\| (n). 


        

<hr>



### variable max\_noncoh 

```C++
size_t acq_state_t::max_noncoh;
```



Cap on the auto-split non-coherent look count. 


        

<hr>



### variable n 

```C++
size_t acq_state_t::n;
```



doppler\_bins \* code\_bins — frame size in samples. 


        

<hr>



### variable n\_noncoh 

```C++
size_t acq_state_t::n_noncoh;
```



Non-coherent looks per detection (1 = coherent). 


        

<hr>



### variable nc\_count 

```C++
size_t acq_state_t::nc_count;
```



Coherent dumps in the current look (0…n\_noncoh-1). 


        

<hr>



### variable nc\_surface 

```C++
float* acq_state_t::nc_surface;
```



Non-coherent \|·\|² accumulator (n); NULL unless n\_noncoh &gt; 1. 


        

<hr>



### variable noise\_est 

```C++
float acq_state_t::noise_est;
```




<hr>



### variable noise\_hi 

```C++
size_t acq_state_t::noise_hi;
```



Last CFAR reference bin (inclusive). 


        

<hr>



### variable noise\_lo 

```C++
size_t acq_state_t::noise_lo;
```



First CFAR reference bin (inclusive). 


        

<hr>



### variable noise\_mode 

```C++
det_noise_mode_t acq_state_t::noise_mode;
```



CFAR aggregation mode. 


        

<hr>



### variable noise\_scratch 

```C++
float* acq_state_t::noise_scratch;
```



Scratch for the median sort (n). 


        

<hr>



### variable out\_buf 

```C++
float complex* acq_state_t::out_buf;
```



corr2d dump output (n). 


        

<hr>



### variable pd 

```C++
double acq_state_t::pd;
```



Target detection probability. 


        

<hr>



### variable pd\_predicted 

```C++
double acq_state_t::pd_predicted;
```



Predicted Pd at cn0\_dbhz and the chosen grid: the AVERAGE Pd over the straddle priors (slow-time scalloping, intra-segment rotation, code sample offset — quadrature over uniform priors), not the on-grid best case, and not Pd at the mean amplitude (which Jensen makes optimistic). 


        

<hr>



### variable peak\_col 

```C++
size_t acq_state_t::peak_col;
```




<hr>



### variable peak\_mag 

```C++
float acq_state_t::peak_mag;
```




<hr>



### variable peak\_row 

```C++
size_t acq_state_t::peak_row;
```




<hr>



### variable pfa\_cell 

```C++
double acq_state_t::pfa_cell;
```



Bonferroni per-cell false-alarm probability. 


        

<hr>



### variable ref 

```C++
float complex* acq_state_t::ref;
```



Single-row reference (n), owned. 


        

<hr>



### variable reps 

```C++
size_t acq_state_t::reps;
```



Max coherent code repetitions (the ceiling). 


        

<hr>



### variable ring 

```C++
dp_f32_t* acq_state_t::ring;
```



Raw cf32 input ring (the only ring). 


        

<hr>



### variable ring\_cap 

```C++
size_t acq_state_t::ring_cap;
```



Ring capacity in complex samples. 


        

<hr>



### variable samples\_consumed 

```C++
uint64_t acq_state_t::samples_consumed;
```



Total framed samples (the state's offset). 


        

<hr>



### variable searched\_bins 

```C++
size_t acq_state_t::searched_bins;
```



Doppler bins scanned (&lt;= doppler\_bins; du prior). 


        

<hr>



### variable sf 

```C++
size_t acq_state_t::sf;
```



Chips per PN segment (= len(code)). 


        

<hr>



### variable slow\_fft 

```C++
fft_state_t* acq_state_t::slow_fft;
```



Length-doppler\_bins forward FFT (slow time). 


        

<hr>



### variable spc 

```C++
size_t acq_state_t::spc;
```



Samples per chip (chip-rate oversample factor). 


        

<hr>



### variable straddle\_loss 

```C++
double acq_state_t::straddle_loss;
```



Mean AMPLITUDE derating from grid straddle — a diagnostic summary (~20\*log10 of it in dB); sizing and pd\_predicted average Pd itself over the priors. Derived config, recomputed by create(). 


        

<hr>



### variable test\_stat 

```C++
float acq_state_t::test_stat;
```




<hr>



### variable threshold 

```C++
float acq_state_t::threshold;
```



CFAR gate on test\_stat (theta); coherent path. 


        

<hr>



### variable underpowered 

```C++
uint8_t acq_state_t::underpowered;
```



1 when pd\_predicted &lt; pd. 


        

<hr>



### variable yframe 

```C++
float complex* acq_state_t::yframe;
```



Slow-time-FFT'd frame (n) fed to corr. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/acq/acq_core.h`

