

# Struct acq\_state\_t



[**ClassList**](annotated.md) **>** [**acq\_state\_t**](structacq__state__t.md)



_Streaming acquisition-engine state._ [More...](#detailed-description)

* `#include <acq_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  uint8\_t \* | [**band\_mask**](#variable-band_mask)  <br> |
|  float \_Complex \* | [**blk**](#variable-blk)  <br> |
|  size\_t | [**blk\_epoch**](#variable-blk_epoch)  <br> |
|  double | [**chip\_rate**](#variable-chip_rate)  <br> |
|  double | [**cn0\_dbhz**](#variable-cn0_dbhz)  <br> |
|  size\_t | [**code\_bins**](#variable-code_bins)  <br> |
|  size\_t | [**code\_only\_epochs**](#variable-code_only_epochs)  <br> |
|  size\_t | [**coherent\_bins**](#variable-coherent_bins)  <br> |
|  float \_Complex \* | [**colbuf**](#variable-colbuf)  <br> |
|  float \_Complex \* | [**colout**](#variable-colout)  <br> |
|  [**corr2d\_state\_t**](structcorr2d__state__t.md) \* | [**corr**](#variable-corr)  <br> |
|  double | [**doppler\_rate**](#variable-doppler_rate)  <br> |
|  double | [**doppler\_res\_hz**](#variable-doppler_res_hz)  <br> |
|  double | [**doppler\_span\_hz**](#variable-doppler_span_hz)  <br> |
|  double | [**doppler\_uncertainty**](#variable-doppler_uncertainty)  <br> |
|  uint64\_t | [**dwells**](#variable-dwells)  <br> |
|  double | [**epochs\_per\_symbol**](#variable-epochs_per_symbol)  <br> |
|  float | [**eta**](#variable-eta)  <br> |
|  float | [**eta\_nc**](#variable-eta_nc)  <br> |
|  size\_t | [**frame\_n**](#variable-frame_n)  <br> |
|  double | [**fs**](#variable-fs)  <br> |
|  size\_t | [**interp**](#variable-interp)  <br> |
|  int | [**keep\_surface**](#variable-keep_surface)  <br> |
|  float \* | [**mag\_buf**](#variable-mag_buf)  <br> |
|  size\_t | [**max\_peaks**](#variable-max_peaks)  <br> |
|  size\_t | [**n**](#variable-n)  <br> |
|  size\_t | [**n\_held**](#variable-n_held)  <br> |
|  size\_t | [**n\_noncoh**](#variable-n_noncoh)  <br> |
|  size\_t | [**n\_peaks**](#variable-n_peaks)  <br> |
|  size\_t | [**n\_surf**](#variable-n_surf)  <br> |
|  size\_t | [**n\_twins**](#variable-n_twins)  <br> |
|  size\_t | [**nc\_count**](#variable-nc_count)  <br> |
|  float \* | [**nc\_surface**](#variable-nc_surface)  <br> |
|  float | [**noise\_est**](#variable-noise_est)  <br> |
|  size\_t | [**noise\_hi**](#variable-noise_hi)  <br> |
|  size\_t | [**noise\_lo**](#variable-noise_lo)  <br> |
|  [**det\_noise\_mode\_t**](detector__core_8h.md#enum-det_noise_mode_t) | [**noise\_mode**](#variable-noise_mode)  <br> |
|  float \* | [**noise\_scratch**](#variable-noise_scratch)  <br> |
|  float \_Complex \* | [**out\_buf**](#variable-out_buf)  <br> |
|  double | [**pd**](#variable-pd)  <br> |
|  double | [**pd\_predicted**](#variable-pd_predicted)  <br> |
|  size\_t | [**peak\_col**](#variable-peak_col)  <br> |
|  float | [**peak\_conc**](#variable-peak_conc)  <br> |
|  float | [**peak\_mag**](#variable-peak_mag)  <br> |
|  uint8\_t \* | [**peak\_mask**](#variable-peak_mask)  <br> |
|  size\_t | [**peak\_row**](#variable-peak_row)  <br> |
|  [**det\_peak\_t**](structdet__peak__t.md) \* | [**peaks**](#variable-peaks)  <br> |
|  double | [**pfa**](#variable-pfa)  <br> |
|  double | [**pfa\_cell**](#variable-pfa_cell)  <br> |
|  [**dp\_pool\_t**](structdp__pool__t.md) \* | [**pool**](#variable-pool)  <br> |
|  float \_Complex \* | [**ref**](#variable-ref)  <br> |
|  size\_t | [**reps**](#variable-reps)  <br> |
|  dp\_f32\_t \* | [**ring**](#variable-ring)  <br> |
|  size\_t | [**ring\_cap**](#variable-ring_cap)  <br> |
|  uint64\_t | [**samples\_consumed**](#variable-samples_consumed)  <br> |
|  size\_t | [**searched\_bins**](#variable-searched_bins)  <br> |
|  size\_t | [**sf**](#variable-sf)  <br> |
|  [**acq\_surface\_sink\_fn**](acq__core_8h.md#typedef-acq_surface_sink_fn) | [**sink**](#variable-sink)  <br> |
|  void \* | [**sink\_ctx**](#variable-sink_ctx)  <br> |
|  uint32\_t | [**sink\_decim**](#variable-sink_decim)  <br> |
|  [**fft\_state\_t**](structfft__state__t.md) \* | [**slow\_fft**](#variable-slow_fft)  <br> |
|  size\_t | [**spc**](#variable-spc)  <br> |
|  float \* | [**stat\_surface**](#variable-stat_surface)  <br> |
|  double | [**straddle\_loss**](#variable-straddle_loss)  <br> |
|  uint64\_t | [**surface\_at**](#variable-surface_at)  <br> |
|  double | [**symbol\_rate**](#variable-symbol_rate)  <br> |
|  float | [**test\_stat**](#variable-test_stat)  <br> |
|  int | [**threads**](#variable-threads)  <br> |
|  float | [**threshold**](#variable-threshold)  <br> |
|  float \_Complex \*\* | [**tile\_col**](#variable-tile_col)  <br> |
|  [**fft\_state\_t**](structfft__state__t.md) \*\* | [**tile\_inv**](#variable-tile_inv)  <br> |
|  float \_Complex \*\* | [**tile\_prod**](#variable-tile_prod)  <br> |
|  [**fft\_state\_t**](structfft__state__t.md) \*\* | [**tile\_slow**](#variable-tile_slow)  <br> |
|  [**acq\_tlm\_t**](structacq__tlm__t.md) | [**tlm**](#variable-tlm)  <br> |
|  uint32\_t \* | [**twin\_col**](#variable-twin_col)  <br> |
|  uint32\_t \* | [**twin\_row**](#variable-twin_row)  <br> |
|  uint8\_t | [**underpowered**](#variable-underpowered)  <br> |
|  [**fft\_state\_t**](structfft__state__t.md) \* | [**wide\_fwd**](#variable-wide_fwd)  <br> |
|  [**fft\_state\_t**](structfft__state__t.md) \* | [**wide\_inv**](#variable-wide_inv)  <br> |
|  float \_Complex \* | [**wide\_prod**](#variable-wide_prod)  <br> |
|  float \_Complex \* | [**wide\_ref\_spec**](#variable-wide_ref_spec)  <br> |
|  float \_Complex \* | [**wide\_spec**](#variable-wide_spec)  <br> |
|  size\_t | [**window\_bins**](#variable-window_bins)  <br> |
|  float \_Complex \* | [**yframe**](#variable-yframe)  <br> |












































## Detailed Description


Allocate with [**acq\_create\_burst()**](acq__core_8h.md#function-acq_create_burst) or [**acq\_create\_continuous()**](acq__core_8h.md#function-acq_create_continuous); never stack-allocate. 


    
## Public Attributes Documentation




### variable band\_mask 

```C++
uint8_t* acq_state_t::band_mask;
```



n\_surf; 1 = outside the searched band 
 


        

<hr>



### variable blk 

```C++
float _Complex* acq_state_t::blk;
```



window\_bins \* coherent\_bins \* code\_bins: the block's per-tile epoch correlations; NULL unless both exceed 1. 
 


        

<hr>



### variable blk\_epoch 

```C++
size_t acq_state_t::blk_epoch;
```



Epochs gathered in the current block (0 … coherent\_bins-1). 
 


        

<hr>



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



Design C/N0 the search is sized for (dB-Hz); 0 on a burst engine means none was given. 
 


        

<hr>



### variable code\_bins 

```C++
size_t acq_state_t::code_bins;
```



One segment in samples = sf\*spc. 
 


        

<hr>



### variable code\_only\_epochs 

```C++
size_t acq_state_t::code_only_epochs;
```



Whole code-only epochs a waveform's code-only window holds at any chip phase (§2.1); 1 = no window, D = 1. 
 


        

<hr>



### variable coherent\_bins 

```C++
size_t acq_state_t::coherent_bins;
```



Coherent depth = slow-time FFT length (&lt;= reps on a burst engine). In wideband mode the block depth D inside every tile (file doc); 1 = one epoch, the continuous engine's default. 
 


        

<hr>



### variable colbuf 

```C++
float _Complex* acq_state_t::colbuf;
```



Gathered column scratch (coherent\_bins). 


        

<hr>



### variable colout 

```C++
float _Complex* acq_state_t::colout;
```



FFT'd column scratch (coherent\_bins). 
 


        

<hr>



### variable corr 

```C++
corr2d_state_t* acq_state_t::corr;
```



Single-row-ref correlator (dwell=1). 
 


        

<hr>



### variable doppler\_rate 

```C++
double acq_state_t::doppler_rate;
```



Doppler rate the depth is bounded against (Hz/s); 0 = no bound from the rate. 
 


        

<hr>



### variable doppler\_res\_hz 

```C++
double acq_state_t::doppler_res_hz;
```



Doppler bin width = chip\_rate/(sf\*coherent\_bins). 


        

<hr>



### variable doppler\_span\_hz 

```C++
double acq_state_t::doppler_span_hz;
```



Native Doppler half-range = chip\_rate/(2\*sf). 


        

<hr>



### variable doppler\_uncertainty 

```C++
double acq_state_t::doppler_uncertainty;
```



One-sided Doppler search half-range (Hz); 0 = full native span. 
 


        

<hr>



### variable dwells 

```C++
uint64_t acq_state_t::dwells;
```



dwells decided since create/reset 
 


        

<hr>



### variable epochs\_per\_symbol 

```C++
double acq_state_t::epochs_per_symbol;
```



(chip\_rate/sf)/symbol\_rate; 0 when symbol\_rate &lt;= 0. 
 


        

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



### variable frame\_n 

```C++
size_t acq_state_t::frame_n;
```



Raw samples consumed from the ring per iteration: == n natively; == code\_bins in wideband mode (one epoch's worth — window\_bins hypotheses come from ONE shared epoch, not from consuming more input). 
 


        

<hr>



### variable fs 

```C++
double acq_state_t::fs;
```



Sample rate (Hz) = chip\_rate \* spc. 
 


        

<hr>



### variable interp 

```C++
size_t acq_state_t::interp;
```



Doppler-axis interpolation factor of the inverse (1 = none). Zero-padding the product in frequency is exact band-limited interpolation, so a peak landing between two slow-time bins is no longer attenuated by the scalloping loss that made a half-bin burst invisible at any SNR (gh-1002). It adds resolution to the SURFACE only: the reported bin is mapped back to the native grid, so no consumer's arithmetic changes. 
 


        

<hr>



### variable keep\_surface 

```C++
int acq_state_t::keep_surface;
```



1 = normalise every decided dwell into `stat_surface` for [**acq\_surface()**](acq__core_8h.md#function-acq_surface); set by a caller or by [**acq\_set\_surface\_sink()**](acq__core_8h.md#function-acq_set_surface_sink) 
 


        

<hr>



### variable mag\_buf 

```C++
float* acq_state_t::mag_buf;
```



\|out\_buf\| (n). 
 


        

<hr>



### variable max\_peaks 

```C++
size_t acq_state_t::max_peaks;
```



list capacity per dwell (1 = the maximum) 


        

<hr>



### variable n 

```C++
size_t acq_state_t::n;
```



NATIVE grid size in samples: coherent\_bins \* window\_bins \* code\_bins (both may exceed 1: D rows inside each tile). This is the INPUT frame and the count of statistically independent cells  it is what the threshold ladder is sized from, and it is what `doppler_bin` is reported on. 
 


        

<hr>



### variable n\_held 

```C++
size_t acq_state_t::n_held;
```



the last dwell's picks held as twins (§7.1) 
 


        

<hr>



### variable n\_noncoh 

```C++
size_t acq_state_t::n_noncoh;
```



Non-coherent looks per detection (1 = coherent). 


        

<hr>



### variable n\_peaks 

```C++
size_t acq_state_t::n_peaks;
```



picks in the last dwell, held ones too 
 


        

<hr>



### variable n\_surf 

```C++
size_t acq_state_t::n_surf;
```



Cells in the correlation SURFACE = `interp` \* n. The inverse transform is evaluated on a finer Doppler grid, so the buffers, the peak search and the CFAR reference span this and not `n`. 
 


        

<hr>



### variable n\_twins 

```C++
size_t acq_state_t::n_twins;
```



picks carried over from the last dwell 
 


        

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
float _Complex* acq_state_t::out_buf;
```



corr2d dump output (n) — also the wideband mode's (window\_bins, code\_bins) grid. 
 


        

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



Predicted Pd at cn0\_dbhz and the chosen grid: the AVERAGE Pd over the straddle priors (slow-time scalloping over the INTERPOLATED bin the peak search samples, intra-segment rotation, code sample offset — quadrature over uniform priors), not the on-grid best case, and not Pd at the mean amplitude (which Jensen makes optimistic). Conservative by construction: the engine takes the maximum over an interpolated surface, which the Marcum form does not credit (doppler#1183, #1064). NAN when no design C/N0 was given. 


        

<hr>



### variable peak\_col 

```C++
size_t acq_state_t::peak_col;
```




<hr>



### variable peak\_conc 

```C++
float acq_state_t::peak_conc;
```



the strongest pick's column concentration 
 


        

<hr>



### variable peak\_mag 

```C++
float acq_state_t::peak_mag;
```




<hr>



### variable peak\_mask 

```C++
uint8_t* acq_state_t::peak_mask;
```



n\_surf; the working mask 
 


        

<hr>



### variable peak\_row 

```C++
size_t acq_state_t::peak_row;
```




<hr>



### variable peaks 

```C++
det_peak_t* acq_state_t::peaks;
```



max\_peaks: the last dwell's picks 
 


        

<hr>



### variable pfa 

```C++
double acq_state_t::pfa;
```



Target system false-alarm probability (stored for configure\_search\_raw's threshold re-derivation). 
 


        

<hr>



### variable pfa\_cell 

```C++
double acq_state_t::pfa_cell;
```



Bonferroni per-cell false-alarm probability. 
 


        

<hr>



### variable pool 

```C++
dp_pool_t* acq_state_t::pool;
```



NULL or one thread = serial 
 


        

<hr>



### variable ref 

```C++
float _Complex* acq_state_t::ref;
```



Single-row reference (n), owned. 
 


        

<hr>



### variable reps 

```C++
size_t acq_state_t::reps;
```



Max coherent code repetitions (the ceiling); always 1 for an engine built via [**acq\_create\_continuous()**](acq__core_8h.md#function-acq_create_continuous). 


        

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



Doppler bins scanned (&lt;= coherent\_bins; du prior). 


        

<hr>



### variable sf 

```C++
size_t acq_state_t::sf;
```



Chips per PN segment (= len(code)). 
 


        

<hr>



### variable sink 

```C++
acq_surface_sink_fn acq_state_t::sink;
```



NULL = none 
 


        

<hr>



### variable sink\_ctx 

```C++
void* acq_state_t::sink_ctx;
```




<hr>



### variable sink\_decim 

```C++
uint32_t acq_state_t::sink_decim;
```



the sink sees every sink\_decim-th dwell 
 


        

<hr>



### variable slow\_fft 

```C++
fft_state_t* acq_state_t::slow_fft;
```



Length-coherent\_bins forward FFT (slow time). 


        

<hr>



### variable spc 

```C++
size_t acq_state_t::spc;
```



Samples per chip (chip-rate oversample factor). 
 


        

<hr>



### variable stat\_surface 

```C++
float* acq_state_t::stat_surface;
```



n\_surf: the last decided dwell in the gate's units; allocated on the first dwell decided with keep\_surface set 
 


        

<hr>



### variable straddle\_loss 

```C++
double acq_state_t::straddle_loss;
```



Mean AMPLITUDE derating from grid straddle — a diagnostic summary (~20\*log10 of it in dB); sizing and pd\_predicted average Pd itself over the priors. Derived config, recomputed by create(). 


        

<hr>



### variable surface\_at 

```C++
uint64_t acq_state_t::surface_at;
```



samples\_consumed of that dwell; 0 = none 
 


        

<hr>



### variable symbol\_rate 

```C++
double acq_state_t::symbol_rate;
```



Continuous data-symbol rate (Hz); 0 = no known data-modulation clock. Diagnostic only on an engine built via [**acq\_create\_continuous()**](acq__core_8h.md#function-acq_create_continuous) (which always forces coherent\_bins=1 regardless) — informational, doesn't feed sizing. 
 


        

<hr>



### variable test\_stat 

```C++
float acq_state_t::test_stat;
```




<hr>



### variable threads 

```C++
int acq_state_t::threads;
```



workers the pool runs on, the caller included; 1 without a pool 
 


        

<hr>



### variable threshold 

```C++
float acq_state_t::threshold;
```



CFAR gate on test\_stat (theta); coherent path. 
 


        

<hr>



### variable tile\_col 

```C++
float _Complex** acq_state_t::tile_col;
```



window\_bins column scratch, 2\*D\*interp (in, then out), NULL at D == 1 
 


        

<hr>



### variable tile\_inv 

```C++
fft_state_t** acq_state_t::tile_inv;
```



window\_bins inverse plans (code\_bins) 


        

<hr>



### variable tile\_prod 

```C++
float _Complex** acq_state_t::tile_prod;
```



window\_bins product buffers 
 


        

<hr>



### variable tile\_slow 

```C++
fft_state_t** acq_state_t::tile_slow;
```



window\_bins slow-time plans (D\*interp), NULL at D == 1 
 


        

<hr>



### variable tlm 

```C++
acq_tlm_t acq_state_t::tlm;
```



telemetry attachment; ctx NULL = detached 
 


        

<hr>



### variable twin\_col 

```C++
uint32_t* acq_state_t::twin_col;
```



max\_peaks: last dwell's picks, code phases 


        

<hr>



### variable twin\_row 

```C++
uint32_t* acq_state_t::twin_row;
```



max\_peaks: last dwell's picks, native rows 


        

<hr>



### variable underpowered 

```C++
uint8_t acq_state_t::underpowered;
```



1 when pd\_predicted &lt; pd; never without a design C/N0  there is no target to be under. 


        

<hr>



### variable wide\_fwd 

```C++
fft_state_t* acq_state_t::wide_fwd;
```



Forward FFT, length code\_bins. 
 


        

<hr>



### variable wide\_inv 

```C++
fft_state_t* acq_state_t::wide_inv;
```



Inverse FFT, length code\_bins. 
 


        

<hr>



### variable wide\_prod 

```C++
float _Complex* acq_state_t::wide_prod;
```



Rolled-spectrum \* wide\_ref\_spec product, length code\_bins; reused per hypothesis. 


        

<hr>



### variable wide\_ref\_spec 

```C++
float _Complex* acq_state_t::wide_ref_spec;
```



conj(FFT(replica row)), length code\_bins. 
 


        

<hr>



### variable wide\_spec 

```C++
float _Complex* acq_state_t::wide_spec;
```



FFT(raw epoch), length code\_bins; once/epoch. 
 


        

<hr>



### variable window\_bins 

```C++
size_t acq_state_t::window_bins;
```



Wideband frequency-window hypotheses (1 = disabled/native — see the file doc comment). 
 


        

<hr>



### variable yframe 

```C++
float _Complex* acq_state_t::yframe;
```



Slow-time-FFT'd frame (n) fed to corr. 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/acq/acq_core.h`

