

# Struct dp\_acq\_state\_t



[**ClassList**](annotated.md) **>** [**dp\_acq\_state\_t**](structdp__acq__state__t.md)



_Streaming acquisition-engine state._ [More...](#detailed-description)

* `#include <acq_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  uint8\_t \* | [**band\_mask**](#variable-band_mask)  <br> |
|  size\_t | [**blk\_epoch**](#variable-blk_epoch)  <br> |
|  float \_Complex \* | [**blk\_raw**](#variable-blk_raw)  <br> |
|  uint8\_t | [**burst**](#variable-burst)  <br> |
|  double | [**carrier\_freq\_hz**](#variable-carrier_freq_hz)  <br> |
|  double | [**chip\_rate**](#variable-chip_rate)  <br> |
|  double | [**cn0\_dbhz**](#variable-cn0_dbhz)  <br> |
|  size\_t | [**code\_bins**](#variable-code_bins)  <br> |
|  size\_t | [**code\_only\_epochs**](#variable-code_only_epochs)  <br> |
|  size\_t | [**coherent\_bins**](#variable-coherent_bins)  <br> |
|  size\_t | [**consumed**](#variable-consumed)  <br> |
|  double | [**doppler\_rate**](#variable-doppler_rate)  <br> |
|  double | [**doppler\_res\_hz**](#variable-doppler_res_hz)  <br> |
|  double | [**doppler\_span\_hz**](#variable-doppler_span_hz)  <br> |
|  double | [**doppler\_uncertainty**](#variable-doppler_uncertainty)  <br> |
|  int | [**dsl\_ready**](#variable-dsl_ready)  <br> |
|  size\_t | [**dsl\_window\_bins**](#variable-dsl_window_bins)  <br> |
|  uint64\_t | [**dwells**](#variable-dwells)  <br> |
|  int | [**epoch\_fan**](#variable-epoch_fan)  <br> |
|  double | [**epochs\_per\_symbol**](#variable-epochs_per_symbol)  <br> |
|  float | [**eta**](#variable-eta)  <br> |
|  float | [**eta\_nc**](#variable-eta_nc)  <br> |
|  size\_t | [**frame\_n**](#variable-frame_n)  <br> |
|  dp\_f32\_framer\_t | [**framer**](#variable-framer)  <br> |
|  double | [**fs**](#variable-fs)  <br> |
|  size\_t | [**interp**](#variable-interp)  <br> |
|  int | [**keep\_surface**](#variable-keep_surface)  <br> |
|  size\_t | [**lag\_bins**](#variable-lag_bins)  <br> |
|  float \* | [**mag\_buf**](#variable-mag_buf)  <br> |
|  struct dp\_\_dd\_map \* | [**map**](#variable-map)  <br> |
|  size\_t | [**map\_f**](#variable-map_f)  <br> |
|  int32\_t \* | [**map\_off**](#variable-map_off)  <br> |
|  size\_t \* | [**map\_rows**](#variable-map_rows)  <br> |
|  float \_Complex \* | [**map\_spec**](#variable-map_spec)  <br> |
|  struct dp\_\_dd\_tile \* | [**map\_tiles**](#variable-map_tiles)  <br> |
|  float \_Complex \* | [**map\_work**](#variable-map_work)  <br> |
|  float \_Complex \* | [**map\_wt**](#variable-map_wt)  <br> |
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
|  [**det\_noise\_mode\_t**](correlator__core_8h.md#enum-det_noise_mode_t) | [**noise\_mode**](#variable-noise_mode)  <br> |
|  float \* | [**noise\_scratch**](#variable-noise_scratch)  <br> |
|  float \_Complex \* | [**out\_buf**](#variable-out_buf)  <br> |
|  [**acq\_part\_t**](structacq__part__t.md) \* | [**parts**](#variable-parts)  <br> |
|  double | [**pd**](#variable-pd)  <br> |
|  double | [**pd\_burst**](#variable-pd_burst)  <br> |
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
|  float \_Complex \* | [**prompt\_scratch**](#variable-prompt_scratch)  <br> |
|  float \_Complex \* | [**rate\_best**](#variable-rate_best)  <br> |
|  size\_t | [**rate\_best\_cap**](#variable-rate_best_cap)  <br> |
|  size\_t | [**rate\_best\_h**](#variable-rate_best_h)  <br> |
|  float \_Complex \* | [**rate\_chirp**](#variable-rate_chirp)  <br> |
|  size\_t | [**rate\_chirp\_d**](#variable-rate_chirp_d)  <br> |
|  size\_t | [**rate\_hyps**](#variable-rate_hyps)  <br> |
|  float \_Complex \* | [**ref**](#variable-ref)  <br> |
|  float \_Complex \* | [**ref\_spec**](#variable-ref_spec)  <br> |
|  size\_t | [**reps**](#variable-reps)  <br> |
|  dp\_f32\_t \* | [**ring**](#variable-ring)  <br> |
|  size\_t | [**ring\_cap**](#variable-ring_cap)  <br> |
|  uint64\_t | [**samples\_consumed**](#variable-samples_consumed)  <br> |
|  size\_t | [**searched\_bins**](#variable-searched_bins)  <br> |
|  size\_t | [**sf**](#variable-sf)  <br> |
|  [**acq\_shape\_t**](structacq__shape__t.md) | [**shape**](#variable-shape)  <br> |
|  [**acq\_surface\_sink\_fn**](acq__core_8h.md#typedef-acq_surface_sink_fn) | [**sink**](#variable-sink)  <br> |
|  void \* | [**sink\_ctx**](#variable-sink_ctx)  <br> |
|  uint32\_t | [**sink\_decim**](#variable-sink_decim)  <br> |
|  size\_t | [**sl\_ccol**](#variable-sl_ccol)  <br> |
|  double \* | [**sl\_cmax**](#variable-sl_cmax)  <br> |
|  size\_t | [**sl\_cn**](#variable-sl_cn)  <br> |
|  size\_t | [**sl\_cnext**](#variable-sl_cnext)  <br> |
|  size\_t | [**sl\_crow**](#variable-sl_crow)  <br> |
|  double | [**sl\_ctile**](#variable-sl_ctile)  <br> |
|  struct dp\_\_dd\_map \* | [**sl\_map**](#variable-sl_map)  <br> |
|  struct dp\_\_dd\_map \* | [**sl\_map\_cb**](#variable-sl_map_cb)  <br> |
|  float \_Complex \* | [**sl\_scratch**](#variable-sl_scratch)  <br> |
|  float \* | [**stat\_surface**](#variable-stat_surface)  <br> |
|  double | [**straddle\_loss**](#variable-straddle_loss)  <br> |
|  uint64\_t | [**surface\_at**](#variable-surface_at)  <br> |
|  double | [**symbol\_rate**](#variable-symbol_rate)  <br> |
|  float | [**test\_stat**](#variable-test_stat)  <br> |
|  int | [**threads**](#variable-threads)  <br> |
|  int | [**threads\_req**](#variable-threads_req)  <br> |
|  float | [**threshold**](#variable-threshold)  <br> |
|  [**acq\_tlm\_t**](structacq__tlm__t.md) | [**tlm**](#variable-tlm)  <br> |
|  uint32\_t \* | [**twin\_col**](#variable-twin_col)  <br> |
|  uint32\_t \* | [**twin\_row**](#variable-twin_row)  <br> |
|  uint8\_t | [**underpowered**](#variable-underpowered)  <br> |
|  size\_t | [**window\_bins**](#variable-window_bins)  <br> |












































## Detailed Description


Allocate with [**dp\_acq\_create\_burst()**](acq__core_8h.md#function-dp_acq_create_burst) or [**dp\_acq\_create\_continuous()**](acq__core_8h.md#function-dp_acq_create_continuous); never stack-allocate. 


    
## Public Attributes Documentation




### variable band\_mask 

```C++
uint8_t* dp_acq_state_t::band_mask;
```



n\_surf; 1 = outside the searched band 
 


        

<hr>



### variable blk\_epoch 

```C++
size_t dp_acq_state_t::blk_epoch;
```



Epochs gathered in the current block (0 … coherent\_bins-1). 
 


        

<hr>



### variable blk\_raw 

```C++
float _Complex* dp_acq_state_t::blk_raw;
```



coherent\_bins \* code\_bins: the block's raw epochs as pushed, which the map reads at the block's end (and the tile-edge test of acq\_resolve\_tile\_alias()); NULL unless the engine is tiled with D &gt; 1. 
 


        

<hr>



### variable burst 

```C++
uint8_t dp_acq_state_t::burst;
```



1 for an engine built by [**dp\_acq\_create\_burst()**](acq__core_8h.md#function-dp_acq_create_burst), 0 for [**dp\_acq\_create\_continuous()**](acq__core_8h.md#function-dp_acq_create_continuous). Config. 
 


        

<hr>



### variable carrier\_freq\_hz 

```C++
double dp_acq_state_t::carrier_freq_hz;
```



RF carrier the Doppler is physically coupled to, Hz ([**dp\_acq\_set\_carrier\_freq\_hz()**](acq__core_8h.md#function-dp_acq_set_carrier_freq_hz)); 0 = uncoupled: no code-rate hypothesis per tile, no dwell advance in the hand-off. 


        

<hr>



### variable chip\_rate 

```C++
double dp_acq_state_t::chip_rate;
```



Chip rate (Hz). 
 


        

<hr>



### variable cn0\_dbhz 

```C++
double dp_acq_state_t::cn0_dbhz;
```



Design C/N0 the search is sized for (dB-Hz); NaN (ACQ\_CN0\_NONE) means none was given. 
 


        

<hr>



### variable code\_bins 

```C++
size_t dp_acq_state_t::code_bins;
```



One code period in samples, fs \* sf / chip\_rate: the length of every forward fast-time transform and of the raw epoch. 
 


        

<hr>



### variable code\_only\_epochs 

```C++
size_t dp_acq_state_t::code_only_epochs;
```



Whole code-only epochs a waveform's code-only window holds at any chip phase (docs/design/async-dsss-receiver.md §2.1); 1 = no window, D = 1. 
 


        

<hr>



### variable coherent\_bins 

```C++
size_t dp_acq_state_t::coherent_bins;
```



Coherent depth = slow-time FFT length (&lt;= reps on a burst engine). In wideband mode the block depth D inside every tile (file doc); 1 = one epoch, the continuous engine's default. 
 


        

<hr>



### variable consumed 

```C++
size_t dp_acq_state_t::consumed;
```



Input samples the last push took. 
 


        

<hr>



### variable doppler\_rate 

```C++
double dp_acq_state_t::doppler_rate;
```



Doppler rate the depth is bounded against (Hz/s); 0 = no bound from the rate. 
 


        

<hr>



### variable doppler\_res\_hz 

```C++
double dp_acq_state_t::doppler_res_hz;
```



Doppler bin width = chip\_rate/(sf\*coherent\_bins). 


        

<hr>



### variable doppler\_span\_hz 

```C++
double dp_acq_state_t::doppler_span_hz;
```



Native Doppler half-range = chip\_rate/(2\*sf). 


        

<hr>



### variable doppler\_uncertainty 

```C++
double dp_acq_state_t::doppler_uncertainty;
```



One-sided Doppler search half-range (Hz); 0 = full native span. 
 


        

<hr>



### variable dsl\_ready 

```C++
int dp_acq_state_t::dsl_ready;
```



1 once [**acq\_shape\_t::dsl**](structacq__shape__t.md#variable-dsl) holds its value: it is computed on its first read, [**dp\_acq\_dsl\_db()**](acq__core_8h.md#function-dp_acq_dsl_db), not by the constructor  a sweep of the ambiguity that only a peak-list consumer reads. 


        

<hr>



### variable dsl\_window\_bins 

```C++
size_t dp_acq_state_t::dsl_window_bins;
```



The tiles of the grid the engine was BUILT with, which [**acq\_shape\_t::dsl**](structacq__shape__t.md#variable-dsl) is defined on; a grid pinned later does not move it. 


        

<hr>



### variable dwells 

```C++
uint64_t dp_acq_state_t::dwells;
```



dwells decided since create/reset 
 


        

<hr>



### variable epoch\_fan 

```C++
int dp_acq_state_t::epoch_fan;
```



1 (create's default): every tile stage fans on the pool. 0: the stages that run once an epoch run inline, and the per-block ones fan only when a block is more than one epoch. The result is bit-identical either way; only the cost moves. async\_dsss\_pool clears it on its search (doppler#2775): on bare metal the per-epoch fan pays even at one tile a worker, while on a host with a costly wake-up (WSL2) it costs that pool 1.9-3.5x at D = 1, so no tile count decides it. 
 


        

<hr>



### variable epochs\_per\_symbol 

```C++
double dp_acq_state_t::epochs_per_symbol;
```



(chip\_rate/sf)/symbol\_rate; 0 when symbol\_rate &lt;= 0. 
 


        

<hr>



### variable eta 

```C++
float dp_acq_state_t::eta;
```



Raw per-cell Rayleigh amplitude threshold. 
 


        

<hr>



### variable eta\_nc 

```C++
float dp_acq_state_t::eta_nc;
```



Non-coherent CFAR threshold (order-N\_nc Marcum). 
 


        

<hr>



### variable frame\_n 

```C++
size_t dp_acq_state_t::frame_n;
```



Raw samples consumed from the ring per iteration: == n natively; == code\_bins in wideband mode (one epoch's worth — window\_bins hypotheses come from ONE shared epoch, not from consuming more input). 
 


        

<hr>



### variable framer 

```C++
dp_f32_framer_t dp_acq_state_t::framer;
```



Any chunk in, frame\_n-sample frames out. 
 


        

<hr>



### variable fs 

```C++
double dp_acq_state_t::fs;
```



Sample rate (Hz) = chip\_rate \* code\_bins / sf. 


        

<hr>



### variable interp 

```C++
size_t dp_acq_state_t::interp;
```



Doppler-axis interpolation factor of the inverse (1 = none). Zero-padding the product in frequency is exact band-limited interpolation, so a peak landing between two slow-time bins is no longer attenuated by the scalloping loss that made a half-bin burst invisible at any SNR (gh-1002). It adds resolution to the SURFACE only: the reported bin is mapped back to the native grid, so no consumer's arithmetic changes. 
 


        

<hr>



### variable keep\_surface 

```C++
int dp_acq_state_t::keep_surface;
```



1 = normalise every decided dwell into `stat_surface` for acq\_surface(); set by a caller or by [**dp\_acq\_set\_surface\_sink()**](acq__core_8h.md#function-dp_acq_set_surface_sink) 
 


        

<hr>



### variable lag\_bins 

```C++
size_t dp_acq_state_t::lag_bins;
```



Code-phase columns of the surface: the inverse fast-time transform's length. code\_bins, except on a continuous engine whose code\_bins is not 7-smooth ([**dp\_fft\_smooth\_floor()**](fft__core_8h.md#function-dp_fft_smooth_floor)): there the cross-spectrum is zero-padded to the next power of two before the inverse, which interpolates the circular correlation exactly and costs a fraction of a transform of awkward length. Column c is lag c \* code\_bins / lag\_bins samples. 
 


        

<hr>



### variable mag\_buf 

```C++
float* dp_acq_state_t::mag_buf;
```



\|out\_buf\| (n). 
 


        

<hr>



### variable map 

```C++
struct dp__dd_map* dp_acq_state_t::map;
```



The map: its plans, a slot per worker. 
 


        

<hr>



### variable map\_f 

```C++
size_t dp_acq_state_t::map_f;
```



The ramp's fine grid: 4 when a carrier is set, the engine is tiled and D &gt; 1; else 1. 


        

<hr>



### variable map\_off 

```C++
int32_t* dp_acq_state_t::map_off;
```



window\_bins x code\_bins rounded-ramp offsets; NULL when no tile ramps (map\_f == 1). 
 


        

<hr>



### variable map\_rows 

```C++
size_t* dp_acq_state_t::map_rows;
```



window\_bins x D\*interp: each slow row's surface row (acq\_block\_row). 
 


        

<hr>



### variable map\_spec 

```C++
float _Complex* dp_acq_state_t::map_spec;
```



The block's epoch spectra, coherent\_bins x code\_bins. 
 


        

<hr>



### variable map\_tiles 

```C++
struct dp__dd_tile* dp_acq_state_t::map_tiles;
```



window\_bins tiles: roll, ramp offsets, weights, rows. 
 


        

<hr>



### variable map\_work 

```C++
float _Complex* dp_acq_state_t::map_work;
```



The map's scratch for its schedule (dp\_\_dd\_map\_work\_len). 
 


        

<hr>



### variable map\_wt 

```C++
float _Complex* dp_acq_state_t::map_wt;
```



The tiles' weights: window\_bins x code\_bins with a ramp, else one shared code\_bins (ref\_spec / code\_bins). 
 


        

<hr>



### variable max\_peaks 

```C++
size_t dp_acq_state_t::max_peaks;
```



list capacity per dwell (1 = the maximum) 


        

<hr>



### variable n 

```C++
size_t dp_acq_state_t::n;
```



NATIVE grid size in samples: coherent\_bins \* window\_bins \* code\_bins (both may exceed 1: D rows inside each tile). This is the INPUT frame and the count of statistically independent cells  it is what the threshold ladder is sized from, and it is what `doppler_bin` is reported on. 
 


        

<hr>



### variable n\_held 

```C++
size_t dp_acq_state_t::n_held;
```



the last dwell's picks held as twins (§7.1) 
 


        

<hr>



### variable n\_noncoh 

```C++
size_t dp_acq_state_t::n_noncoh;
```



Non-coherent looks per detection (1 = coherent). 


        

<hr>



### variable n\_peaks 

```C++
size_t dp_acq_state_t::n_peaks;
```



picks in the last dwell, held ones too 
 


        

<hr>



### variable n\_surf 

```C++
size_t dp_acq_state_t::n_surf;
```



Cells in the correlation SURFACE = `interp` \* n. The inverse transform is evaluated on a finer Doppler grid, so the buffers, the peak search and the CFAR reference span this and not `n`. 
 


        

<hr>



### variable n\_twins 

```C++
size_t dp_acq_state_t::n_twins;
```



picks carried over from the last dwell 
 


        

<hr>



### variable nc\_count 

```C++
size_t dp_acq_state_t::nc_count;
```



Coherent dumps in the current look (0…n\_noncoh-1). 


        

<hr>



### variable nc\_surface 

```C++
float* dp_acq_state_t::nc_surface;
```



Non-coherent \|·\|² accumulator (n); NULL unless n\_noncoh &gt; 1. 
 


        

<hr>



### variable noise\_est 

```C++
float dp_acq_state_t::noise_est;
```



the dwell's CFAR noise reference, same units as peak\_mag 
 


        

<hr>



### variable noise\_hi 

```C++
size_t dp_acq_state_t::noise_hi;
```



Last CFAR reference bin (inclusive). 
 


        

<hr>



### variable noise\_lo 

```C++
size_t dp_acq_state_t::noise_lo;
```



First CFAR reference bin (inclusive). 
 


        

<hr>



### variable noise\_mode 

```C++
det_noise_mode_t dp_acq_state_t::noise_mode;
```



CFAR aggregation mode. 


        

<hr>



### variable noise\_scratch 

```C++
float* dp_acq_state_t::noise_scratch;
```



Scratch for the median sort (n). 
 


        

<hr>



### variable out\_buf 

```C++
float _Complex* dp_acq_state_t::out_buf;
```



The decided coherent surface (n\_surf): the one delay-Doppler map's output, rows of lag\_bins, scaled 1/code\_bins on every path (docs/design/acq-surface.md §18.1). 
 


        

<hr>



### variable parts 

```C++
acq_part_t* dp_acq_state_t::parts;
```



window\_bins partial slots of the decided surface's per-tile passes 
 


        

<hr>



### variable pd 

```C++
double dp_acq_state_t::pd;
```



Target detection probability. 
 


        

<hr>



### variable pd\_burst 

```C++
double dp_acq_state_t::pd_burst;
```



Predicted Pd of one BURST of `reps` repetitions at cn0\_dbhz: the dwells are aligned to the stream, so the preamble lands at a uniform offset and spans about reps/D of them, whole or partial; the burst is detected when any one is. Averaged over that alignment and the same straddle nodes as pd\_predicted (one node for every dwell of a burst: they share its Doppler and delay). What a burst engine sizes on and sets `underpowered` from (doppler#1498). Assumes the preamble is exactly `reps` periods. NAN on a continuous engine, with n\_noncoh &gt; 1, or with no design C/N0. 


        

<hr>



### variable pd\_predicted 

```C++
double dp_acq_state_t::pd_predicted;
```



Predicted Pd at cn0\_dbhz and the chosen grid of ONE dwell lying wholly inside the signal (a burst engine's is judged on pd\_burst instead): the AVERAGE Pd over the straddle priors (slow-time scalloping over the INTERPOLATED bin the peak search samples, intra-segment rotation, code sample offset — quadrature over uniform priors), not the on-grid best case, and not Pd at the mean amplitude (which Jensen makes optimistic). Conservative by construction: the engine takes the maximum over an interpolated surface, which the Marcum form does not credit (doppler#1183, #1064). NAN when no design C/N0 was given. 


        

<hr>



### variable peak\_col 

```C++
size_t dp_acq_state_t::peak_col;
```



... and its code-phase column 
 


        

<hr>



### variable peak\_conc 

```C++
float dp_acq_state_t::peak_conc;
```



the strongest pick's column concentration 
 


        

<hr>



### variable peak\_mag 

```C++
float dp_acq_state_t::peak_mag;
```



... and its amplitude (linear; the sqrt of the summed power when n\_noncoh &gt; 1) 
 


        

<hr>



### variable peak\_mask 

```C++
uint8_t* dp_acq_state_t::peak_mask;
```



n\_surf; the working mask 
 


        

<hr>



### variable peak\_row 

```C++
size_t dp_acq_state_t::peak_row;
```



last decided dwell's strongest pick: its native Doppler row 
 


        

<hr>



### variable peaks 

```C++
det_peak_t* dp_acq_state_t::peaks;
```



max\_peaks: the last dwell's picks 
 


        

<hr>



### variable pfa 

```C++
double dp_acq_state_t::pfa;
```



Target system false-alarm probability (stored for configure\_search\_raw's threshold re-derivation). 
 


        

<hr>



### variable pfa\_cell 

```C++
double dp_acq_state_t::pfa_cell;
```



Sidak per-cell false-alarm probability, split over the cells a dwell's maximum ranges over: the in-band rows of the interpolated surface, times code\_bins, times the rate hypotheses. 
 


        

<hr>



### variable pool 

```C++
dp_pool_t* dp_acq_state_t::pool;
```



NULL or one thread = serial 
 


        

<hr>



### variable prompt\_scratch 

```C++
float _Complex* dp_acq_state_t::prompt_scratch;
```



[**dp\_acq\_block\_prompt()**](acq__core_8h.md#function-dp_acq_block_prompt)'s: an epoch's spectrum, its weights and a row (2 code\_bins + lag\_bins); made on first use. 


        

<hr>



### variable rate\_best 

```C++
float _Complex* dp_acq_state_t::rate_best;
```



n\_surf cells: the best hypothesis' surface while the others are searched. 
 


        

<hr>



### variable rate\_best\_cap 

```C++
size_t dp_acq_state_t::rate_best_cap;
```



Cells rate\_best holds. 
 


        

<hr>



### variable rate\_best\_h 

```C++
size_t dp_acq_state_t::rate_best_h;
```



Winning hypothesis of the last dwell. 
 


        

<hr>



### variable rate\_chirp 

```C++
float _Complex* dp_acq_state_t::rate_chirp;
```



rate\_hyps \* rate\_chirp\_d slow-time de-chirp phasors; NULL while rate\_hyps == 1. Rebuilt lazily when coherent\_bins moves. 
 


        

<hr>



### variable rate\_chirp\_d 

```C++
size_t dp_acq_state_t::rate_chirp_d;
```



The coherent\_bins rate\_chirp was built for. 


        

<hr>



### variable rate\_hyps 

```C++
size_t dp_acq_state_t::rate_hyps;
```



Doppler-rate hypotheses searched (&gt;= 1); 1 = none: the engine as it ran without them. [**dp\_acq\_create\_burst()**](acq__core_8h.md#function-dp_acq_create_burst)'s rate\_hypotheses. 
 


        

<hr>



### variable ref 

```C++
float _Complex* dp_acq_state_t::ref;
```



Single-row reference (n), owned. 
 


        

<hr>



### variable ref\_spec 

```C++
float _Complex* dp_acq_state_t::ref_spec;
```



conj(FFT(replica)), code\_bins: the template every tile's weights carry. 
 


        

<hr>



### variable reps 

```C++
size_t dp_acq_state_t::reps;
```



Max coherent code repetitions (the ceiling); always 1 for an engine built via [**dp\_acq\_create\_continuous()**](acq__core_8h.md#function-dp_acq_create_continuous). 


        

<hr>



### variable ring 

```C++
dp_f32_t* dp_acq_state_t::ring;
```



The carry's storage: bound to `framer`, freed by destroy. 
 


        

<hr>



### variable ring\_cap 

```C++
size_t dp_acq_state_t::ring_cap;
```



Ring capacity in complex samples. 
 


        

<hr>



### variable samples\_consumed 

```C++
uint64_t dp_acq_state_t::samples_consumed;
```



Total framed samples (the state's offset). 
 


        

<hr>



### variable searched\_bins 

```C++
size_t dp_acq_state_t::searched_bins;
```



Doppler bins scanned (&lt;= coherent\_bins; du prior). 


        

<hr>



### variable sf 

```C++
size_t dp_acq_state_t::sf;
```



Chips per PN segment (= len(code)). 
 


        

<hr>



### variable shape 

```C++
acq_shape_t dp_acq_state_t::shape;
```



The preamble's correlation shape (zone, delay straddle); config, filled by the constructor. 


        

<hr>



### variable sink 

```C++
acq_surface_sink_fn dp_acq_state_t::sink;
```



NULL = none 
 


        

<hr>



### variable sink\_ctx 

```C++
void* dp_acq_state_t::sink_ctx;
```



caller's opaque pointer, passed to sink 
 


        

<hr>



### variable sink\_decim 

```C++
uint32_t dp_acq_state_t::sink_decim;
```



the sink sees every sink\_decim-th dwell 
 


        

<hr>



### variable sl\_ccol 

```C++
size_t dp_acq_state_t::sl_ccol[ACQ_SIDELOBE_SLOTS];
```



its column, 
 


        

<hr>



### variable sl\_cmax 

```C++
double* dp_acq_state_t::sl_cmax;
```



... its cache: ACQ\_SIDELOBE\_SLOTS rows of lag\_bins, each one hit's worst ratio at every column as read in one tile. Derived, never in a blob. 


        

<hr>



### variable sl\_cn 

```C++
size_t dp_acq_state_t::sl_cn;
```



slots filled 
 


        

<hr>



### variable sl\_cnext 

```C++
size_t dp_acq_state_t::sl_cnext;
```



the slot a full cache overwrites next 
 


        

<hr>



### variable sl\_crow 

```C++
size_t dp_acq_state_t::sl_crow[ACQ_SIDELOBE_SLOTS];
```



each slot's hit: its row, 
 


        

<hr>



### variable sl\_ctile 

```C++
double dp_acq_state_t::sl_ctile[ACQ_SIDELOBE_SLOTS];
```



and the tile centre, Hz 
 


        

<hr>



### variable sl\_map 

```C++
struct dp__dd_map* dp_acq_state_t::sl_map;
```



[**dp\_acq\_sidelobe\_ratio()**](acq__core_8h.md#function-dp_acq_sidelobe_ratio)'s transforms: the one delay-Doppler map at one epoch, code\_bins in and lag\_bins out, made on its first call 
 


        

<hr>



### variable sl\_map\_cb 

```C++
struct dp__dd_map* dp_acq_state_t::sl_map_cb;
```



... and one at code\_bins out, for the delayed replica 
 


        

<hr>



### variable sl\_scratch 

```C++
float _Complex* dp_acq_state_t::sl_scratch;
```



its buffers: the replica's spectrum, a mixed signal, its spectrum, the delayed replica, the spectrum's conjugate and ones (code\_bins each), then the row (lag\_bins) 


        

<hr>



### variable stat\_surface 

```C++
float* dp_acq_state_t::stat_surface;
```



n\_surf: the last decided dwell in the gate's units; allocated on the first dwell decided with keep\_surface set 
 


        

<hr>



### variable straddle\_loss 

```C++
double dp_acq_state_t::straddle_loss;
```



Mean AMPLITUDE derating from grid straddle — a diagnostic summary (~20\*log10 of it in dB); sizing and pd\_predicted average Pd itself over the priors. Derived config, recomputed by create(). 


        

<hr>



### variable surface\_at 

```C++
uint64_t dp_acq_state_t::surface_at;
```



samples\_consumed of that dwell; 0 = none 
 


        

<hr>



### variable symbol\_rate 

```C++
double dp_acq_state_t::symbol_rate;
```



Continuous data-symbol rate (Hz); 0 = no known data-modulation clock. Diagnostic only on an engine built via [**dp\_acq\_create\_continuous()**](acq__core_8h.md#function-dp_acq_create_continuous) (which always forces coherent\_bins=1 regardless) — informational, doesn't feed sizing. 
 


        

<hr>



### variable test\_stat 

```C++
float dp_acq_state_t::test_stat;
```



the dwell's gating statistic, compared with [**dp\_acq\_gate\_stat()**](acq__core_8h.md#function-dp_acq_gate_stat); 0 if noise\_est is 0 
 


        

<hr>



### variable threads 

```C++
int dp_acq_state_t::threads;
```



workers the pool runs on, the caller included; 1 without a pool 
 


        

<hr>



### variable threads\_req 

```C++
int dp_acq_state_t::threads_req;
```



what the caller asked for: set\_threads()'s n, 0 = the online cores. `threads` is this capped at one worker a tile; every regrid re-applies it, so narrowing to one tile and widening again comes back to min(threads\_req, tiles), not to the 1 the narrow grid ran on (doppler#2797). 


        

<hr>



### variable threshold 

```C++
float dp_acq_state_t::threshold;
```



CFAR gate on test\_stat (theta); coherent path. 
 


        

<hr>



### variable tlm 

```C++
acq_tlm_t dp_acq_state_t::tlm;
```



telemetry attachment; ctx NULL = detached 
 


        

<hr>



### variable twin\_col 

```C++
uint32_t* dp_acq_state_t::twin_col;
```



max\_peaks: last dwell's picks, code phases 


        

<hr>



### variable twin\_row 

```C++
uint32_t* dp_acq_state_t::twin_row;
```



max\_peaks: last dwell's picks, native rows 


        

<hr>



### variable underpowered 

```C++
uint8_t dp_acq_state_t::underpowered;
```



1 when the Pd the engine is judged on falls short of pd: pd\_burst on a burst engine, pd\_predicted where pd\_burst is NAN. Never without a design C/N0  there is no target to be under. 


        

<hr>



### variable window\_bins 

```C++
size_t dp_acq_state_t::window_bins;
```



Wideband frequency-window hypotheses (1 = disabled/native — see the file doc comment). 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/acq/acq_core.h`

