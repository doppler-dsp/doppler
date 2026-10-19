

# Struct dp\_async\_dsss\_receiver\_state\_t



[**ClassList**](annotated.md) **>** [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md)



_Composed receiver state._ [More...](#detailed-description)

* `#include <async_dsss_receiver_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  [**dp\_acq\_state\_t**](structdp__acq__state__t.md) \* | [**acq**](#variable-acq)  <br> |
|  uint64\_t | [**both\_down\_samples**](#variable-both_down_samples)  <br> |
|  [**dp\_carrier\_acq\_state\_t**](structdp__carrier__acq__state__t.md) \* | [**ca**](#variable-ca)  <br> |
|  [**dp\_costas\_state\_t**](structdp__costas__state__t.md) | [**car**](#variable-car)  <br> |
|  float \_Complex \* | [**car\_carry\_buf**](#variable-car_carry_buf)  <br> |
|  size\_t | [**car\_carry\_len**](#variable-car_carry_len)  <br> |
|  int | [**car\_coasting**](#variable-car_coasting)  <br> |
|  [**dp\_costas\_state\_t**](structdp__costas__state__t.md) | [**car\_frozen**](#variable-car_frozen)  <br> |
|  [**dp\_costas\_state\_t**](structdp__costas__state__t.md) | [**car\_held**](#variable-car_held)  <br> |
|  float \_Complex \* | [**car\_wiped\_buf**](#variable-car_wiped_buf)  <br> |
|  double | [**carrier\_freq\_hz**](#variable-carrier_freq_hz)  <br> |
|  int | [**cell**](#variable-cell)  <br> |
|  double | [**cell\_gain**](#variable-cell_gain)  <br> |
|  double | [**cell\_rate\_bias**](#variable-cell_rate_bias)  <br> |
|  int | [**cell\_refined**](#variable-cell_refined)  <br> |
|  double | [**chip\_rate**](#variable-chip_rate)  <br> |
|  double | [**cn0\_dbhz**](#variable-cn0_dbhz)  <br> |
|  double | [**cn0\_dbhz\_est**](#variable-cn0_dbhz_est)  <br> |
|  uint8\_t \* | [**code**](#variable-code)  <br> |
|  size\_t | [**code\_len**](#variable-code_len)  <br> |
|  size\_t | [**correct\_periods**](#variable-correct_periods)  <br> |
|  int | [**differential**](#variable-differential)  <br> |
|  [**dp\_dll\_state\_t**](structdp__dll__state__t.md) \* | [**dll**](#variable-dll)  <br> |
|  double | [**doppler\_hz\_est**](#variable-doppler_hz_est)  <br> |
|  int | [**had\_lock**](#variable-had_lock)  <br> |
|  double | [**held\_phase**](#variable-held_phase)  <br> |
|  uint64\_t | [**intervals**](#variable-intervals)  <br> |
|  double | [**lock\_alpha**](#variable-lock_alpha)  <br> |
|  double | [**lock\_den**](#variable-lock_den)  <br> |
|  double | [**lock\_metric**](#variable-lock_metric)  <br> |
|  double | [**lock\_num**](#variable-lock_num)  <br> |
|  double | [**lost\_confirm\_s**](#variable-lost_confirm_s)  <br> |
|  uint64\_t | [**lost\_confirm\_samples**](#variable-lost_confirm_samples)  <br> |
|  int | [**m**](#variable-m)  <br> |
|  int | [**n**](#variable-n)  <br> |
|  double | [**pd**](#variable-pd)  <br> |
|  size\_t | [**period\_count**](#variable-period_count)  <br> |
|  double | [**pfa**](#variable-pfa)  <br> |
|  size\_t | [**pullin\_intervals**](#variable-pullin_intervals)  <br> |
|  [**dp\_RateConverter\_state\_t**](structdp__RateConverter__state__t.md) \* | [**rc**](#variable-rc)  <br> |
|  bool | [**reacquire**](#variable-reacquire)  <br> |
|  double | [**refine\_design\_margin\_db**](#variable-refine_design_margin_db)  <br> |
|  [**dp\_dll\_state\_t**](structdp__dll__state__t.md) \* | [**refine\_dll**](#variable-refine_dll)  <br> |
|  float \_Complex \* | [**refine\_dll\_out\_buf**](#variable-refine_dll_out_buf)  <br> |
|  size\_t | [**refine\_dll\_out\_cap**](#variable-refine_dll_out_cap)  <br> |
|  double | [**refine\_max\_error\_db**](#variable-refine_max_error_db)  <br> |
|  size\_t | [**refine\_max\_n\_blocks**](#variable-refine_max_n_blocks)  <br> |
|  size\_t | [**refine\_min\_blocks**](#variable-refine_min_blocks)  <br> |
|  size\_t | [**refine\_n\_fft**](#variable-refine_n_fft)  <br> |
|  uint64\_t | [**refine\_periods**](#variable-refine_periods)  <br> |
|  [**dp\_RateConverter\_state\_t**](structdp__RateConverter__state__t.md) \* | [**refine\_rc**](#variable-refine_rc)  <br> |
|  float \_Complex \* | [**refine\_rc\_out\_buf**](#variable-refine_rc_out_buf)  <br> |
|  size\_t | [**refine\_rc\_out\_cap**](#variable-refine_rc_out_cap)  <br> |
|  uint64\_t | [**refine\_samples\_fed**](#variable-refine_samples_fed)  <br> |
|  size\_t | [**refine\_samples\_per\_symbol**](#variable-refine_samples_per_symbol)  <br> |
|  size\_t | [**refine\_segments**](#variable-refine_segments)  <br> |
|  bool | [**refine\_sequential**](#variable-refine_sequential)  <br> |
|  size\_t | [**refine\_zero\_pad**](#variable-refine_zero_pad)  <br> |
|  uint64\_t | [**refused\_hits**](#variable-refused_hits)  <br> |
|  [**dp\_mpsk\_receiver\_state\_t**](structdp__mpsk__receiver__state__t.md) \* | [**rx**](#variable-rx)  <br> |
|  double | [**seed\_chip\_phase**](#variable-seed_chip_phase)  <br> |
|  double | [**seed\_doppler\_hz\_est**](#variable-seed_doppler_hz_est)  <br> |
|  size\_t | [**segments**](#variable-segments)  <br> |
|  size\_t | [**spc**](#variable-spc)  <br> |
|  size\_t | [**sps**](#variable-sps)  <br> |
|  int | [**state**](#variable-state)  <br> |
|  uint64\_t | [**state\_samples**](#variable-state_samples)  <br> |
|  [**dp\_lockdet\_state\_t**](structdp__lockdet__state__t.md) | [**sym\_lockdet**](#variable-sym_lockdet)  <br> |
|  double | [**symbol\_rate**](#variable-symbol_rate)  <br> |
|  [**dp\_tlm\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_t) \* | [**tlm**](#variable-tlm)  <br> |
|  uint32\_t | [**tlm\_decim**](#variable-tlm_decim)  <br> |
|  char | [**tlm\_prefix**](#variable-tlm_prefix)  <br> |
|  int32\_t | [**tlm\_sym\_lock**](#variable-tlm_sym_lock)  <br> |
|  int32\_t | [**tlm\_sym\_locked**](#variable-tlm_sym_locked)  <br> |
|  float \_Complex \* | [**track\_dll\_out\_buf**](#variable-track_dll_out_buf)  <br> |
|  size\_t | [**track\_dll\_out\_cap**](#variable-track_dll_out_cap)  <br> |
|  float \_Complex \* | [**track\_rc\_out\_buf**](#variable-track_rc_out_buf)  <br> |
|  size\_t | [**track\_rc\_out\_cap**](#variable-track_rc_out_cap)  <br> |
|  size\_t | [**tsamps**](#variable-tsamps)  <br> |












































## Detailed Description


Every child is allocated at create() with a placeholder seed (phase 0, no Doppler) and REBUILT (not freed to NULL) on every real transition: a fixed shape per flavor, independent of `state` (cell mode has no `acq` and no refine collector). Treat all fields as internal (use the getters). 


    
## Public Attributes Documentation




### variable acq 

```C++
dp_acq_state_t* dp_async_dsss_receiver_state_t::acq;
```



The embedded search; NULL in cell mode ([**dp\_async\_dsss\_receiver\_create\_cell()**](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_create_cell)), where the seed comes from outside. 


        

<hr>



### variable both\_down\_samples 

```C++
uint64_t dp_async_dsss_receiver_state_t::both_down_samples;
```



Running: consecutive samples fed while tracking with BOTH lock flags down  the release clock; lost keeps it counting. With reacquire it counts the symbol lock flag alone. 
 


        

<hr>



### variable ca 

```C++
dp_carrier_acq_state_t* dp_async_dsss_receiver_state_t::ca;
```




<hr>



### variable car 

```C++
dp_costas_state_t dp_async_dsss_receiver_state_t::car;
```




<hr>



### variable car\_carry\_buf 

```C++
float _Complex* dp_async_dsss_receiver_state_t::car_carry_buf;
```




<hr>



### variable car\_carry\_len 

```C++
size_t dp_async_dsss_receiver_state_t::car_carry_len;
```




<hr>



### variable car\_coasting 

```C++
int dp_async_dsss_receiver_state_t::car_coasting;
```



Running: the carrier loop is held. 


        

<hr>



### variable car\_frozen 

```C++
dp_costas_state_t dp_async_dsss_receiver_state_t::car_frozen;
```




<hr>



### variable car\_held 

```C++
dp_costas_state_t dp_async_dsss_receiver_state_t::car_held;
```



The carrier as of the last period with both lock flags up, restored on entering the hold. 
 


        

<hr>



### variable car\_wiped\_buf 

```C++
float _Complex* dp_async_dsss_receiver_state_t::car_wiped_buf;
```




<hr>



### variable carrier\_freq\_hz 

```C++
double dp_async_dsss_receiver_state_t::carrier_freq_hz;
```



nominal RF carrier, Hz; &gt; 0 enables the carrier-&gt;code rate aiding, 0 = off (config, not running state  restored by create). 


        

<hr>



### variable cell 

```C++
int dp_async_dsss_receiver_state_t::cell;
```



Config: 1 = the cell mode. 
 


        

<hr>



### variable cell\_gain 

```C++
double dp_async_dsss_receiver_state_t::cell_gain;
```



Config: the correction's gain, chips, (0, 1]. 
 


        

<hr>



### variable cell\_rate\_bias 

```C++
double dp_async_dsss_receiver_state_t::cell_rate_bias;
```



Running: the rate bias steering the Dll onto the held phase over the next period (chips per chip), summed into its aid. 
 


        

<hr>



### variable cell\_refined 

```C++
int dp_async_dsss_receiver_state_t::cell_refined;
```



Running: the seed's carrier residual has been estimated on the live chain and folded into loop 1 (adr\_cell\_refine). 


        

<hr>



### variable chip\_rate 

```C++
double dp_async_dsss_receiver_state_t::chip_rate;
```




<hr>



### variable cn0\_dbhz 

```C++
double dp_async_dsss_receiver_state_t::cn0_dbhz;
```



Design C/N0  feeds both Acquisition's own sizing and (derated) CarrierAcquisition's design\_snr on every refine-chain (re)build. 


        

<hr>



### variable cn0\_dbhz\_est 

```C++
double dp_async_dsss_receiver_state_t::cn0_dbhz_est;
```



The seed's C/N0 estimate (its own hit's, or an outside seed's); nothing refreshes it. 
 


        

<hr>



### variable code 

```C++
uint8_t* dp_async_dsss_receiver_state_t::code;
```




<hr>



### variable code\_len 

```C++
size_t dp_async_dsss_receiver_state_t::code_len;
```




<hr>



### variable correct\_periods 

```C++
size_t dp_async_dsss_receiver_state_t::correct_periods;
```



Config: code periods per correction  the searcher's block depth in a pool. 


        

<hr>



### variable differential 

```C++
int dp_async_dsss_receiver_state_t::differential;
```




<hr>



### variable dll 

```C++
dp_dll_state_t* dp_async_dsss_receiver_state_t::dll;
```




<hr>



### variable doppler\_hz\_est 

```C++
double dp_async_dsss_receiver_state_t::doppler_hz_est;
```



The seed's Doppler while refining; the CarrierAcquisition-refined value once tracking (searching flavor). The cell mode's pull-in moves loop 1 and leaves this at the seed's. A refused hand-over zeroes both, as reset() does. 
 


        

<hr>



### variable had\_lock 

```C++
int dp_async_dsss_receiver_state_t::had_lock;
```



Running: both lock flags have been up since the hand-over. After that both down holds both loops (dp\_dll\_set\_coast, the carrier's own hold) so a departed emitter's receiver cannot free-run onto a neighbour's code (#1271). 
 


        

<hr>



### variable held\_phase 

```C++
double dp_async_dsss_receiver_state_t::held_phase;
```



Running: the held code phase at the next interval's start, chips in the Dll's convention, unwrapped. 
 


        

<hr>



### variable intervals 

```C++
uint64_t dp_async_dsss_receiver_state_t::intervals;
```



Running: intervals since the seed  the pull-in schedule's clock. 
 


        

<hr>



### variable lock\_alpha 

```C++
double dp_async_dsss_receiver_state_t::lock_alpha;
```



EMA coeff = 1/dwell (dwell &gt;= 30). 
 


        

<hr>



### variable lock\_den 

```C++
double dp_async_dsss_receiver_state_t::lock_den;
```




<hr>



### variable lock\_metric 

```C++
double dp_async_dsss_receiver_state_t::lock_metric;
```




<hr>



### variable lock\_num 

```C++
double dp_async_dsss_receiver_state_t::lock_num;
```




<hr>



### variable lost\_confirm\_s 

```C++
double dp_async_dsss_receiver_state_t::lost_confirm_s;
```



Config: both flags down for longer than this is "gone"; 0 = never. 
 


        

<hr>



### variable lost\_confirm\_samples 

```C++
uint64_t dp_async_dsss_receiver_state_t::lost_confirm_samples;
```



lost\_confirm\_s at the input rate. 


        

<hr>



### variable m 

```C++
int dp_async_dsss_receiver_state_t::m;
```




<hr>



### variable n 

```C++
int dp_async_dsss_receiver_state_t::n;
```



MpskReceiver's terminal outputs per symbol (`m_out`: even, 2..8). 
 


        

<hr>



### variable pd 

```C++
double dp_async_dsss_receiver_state_t::pd;
```



Also CarrierAcquisition's own pd. 
 


        

<hr>



### variable period\_count 

```C++
size_t dp_async_dsss_receiver_state_t::period_count;
```



Running: periods into the interval. 
 


        

<hr>



### variable pfa 

```C++
double dp_async_dsss_receiver_state_t::pfa;
```



Also CarrierAcquisition's own pfa. 
 


        

<hr>



### variable pullin\_intervals 

```C++
size_t dp_async_dsss_receiver_state_t::pullin_intervals;
```



Config: intervals at gain 1 first. 
 


        

<hr>



### variable rc 

```C++
dp_RateConverter_state_t* dp_async_dsss_receiver_state_t::rc;
```




<hr>



### variable reacquire 

```C++
bool dp_async_dsss_receiver_state_t::reacquire;
```



Config: release on symbol lock alone and reset to searching, never lost (see [**dp\_async\_dsss\_receiver\_create()**](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_create)). 


        

<hr>



### variable refine\_design\_margin\_db 

```C++
double dp_async_dsss_receiver_state_t::refine_design_margin_db;
```




<hr>



### variable refine\_dll 

```C++
dp_dll_state_t* dp_async_dsss_receiver_state_t::refine_dll;
```




<hr>



### variable refine\_dll\_out\_buf 

```C++
float _Complex* dp_async_dsss_receiver_state_t::refine_dll_out_buf;
```




<hr>



### variable refine\_dll\_out\_cap 

```C++
size_t dp_async_dsss_receiver_state_t::refine_dll_out_cap;
```




<hr>



### variable refine\_max\_error\_db 

```C++
double dp_async_dsss_receiver_state_t::refine_max_error_db;
```




<hr>



### variable refine\_max\_n\_blocks 

```C++
size_t dp_async_dsss_receiver_state_t::refine_max_n_blocks;
```




<hr>



### variable refine\_min\_blocks 

```C++
size_t dp_async_dsss_receiver_state_t::refine_min_blocks;
```



Floor on the refine's dwell, in blocks (default 7). The refine's detection sizing needs fewer blocks the higher the C/N0, but the estimate the carrier chain must pull in from does not sharpen with it (#1265). Set with set\_refine\_min\_blocks(), which carries the measurement. 


        

<hr>



### variable refine\_n\_fft 

```C++
size_t dp_async_dsss_receiver_state_t::refine_n_fft;
```




<hr>



### variable refine\_periods 

```C++
uint64_t dp_async_dsss_receiver_state_t::refine_periods;
```



Diagnostic: code periods the refine stage processed since its chain was built. It stops at the period the carrier estimate decides in, so it is the same whatever the block size (#2395). Not in the state blob: it changes nothing, and a restore restarts it. 


        

<hr>



### variable refine\_rc 

```C++
dp_RateConverter_state_t* dp_async_dsss_receiver_state_t::refine_rc;
```




<hr>



### variable refine\_rc\_out\_buf 

```C++
float _Complex* dp_async_dsss_receiver_state_t::refine_rc_out_buf;
```




<hr>



### variable refine\_rc\_out\_cap 

```C++
size_t dp_async_dsss_receiver_state_t::refine_rc_out_cap;
```




<hr>



### variable refine\_samples\_fed 

```C++
uint64_t dp_async_dsss_receiver_state_t::refine_samples_fed;
```



Raw samples fed into the refine pipeline since it was last (re)built. At the hand-over, fed minus consumed is the tail of the last call that goes straight on to the live chain. The clock dilation is taken over the consumed count, from the refine's blocks, not over this. 


        

<hr>



### variable refine\_samples\_per\_symbol 

```C++
size_t dp_async_dsss_receiver_state_t::refine_samples_per_symbol;
```




<hr>



### variable refine\_segments 

```C++
size_t dp_async_dsss_receiver_state_t::refine_segments;
```



[**dp\_dll\_lookback\_segments()**](dll__core_8h.md#function-dp_dll_lookback_segments) result, computed once at create: config for refine\_dll's layout, needed again by set\_state's check. 


        

<hr>



### variable refine\_sequential 

```C++
bool dp_async_dsss_receiver_state_t::refine_sequential;
```




<hr>



### variable refine\_zero\_pad 

```C++
size_t dp_async_dsss_receiver_state_t::refine_zero_pad;
```




<hr>



### variable refused\_hits 

```C++
uint64_t dp_async_dsss_receiver_state_t::refused_hits;
```



Searching hits the seed domain refused as seeds, since create or reset. In the blob with state\_samples, so a resumed receiver's status matches an unbroken run's. 
 


        

<hr>



### variable rx 

```C++
dp_mpsk_receiver_state_t* dp_async_dsss_receiver_state_t::rx;
```




<hr>



### variable seed\_chip\_phase 

```C++
double dp_async_dsss_receiver_state_t::seed_chip_phase;
```



The seed's chip phase; the live chain starts from it advanced by the clock dilation over the refine, not from wherever the refine-stage Dll drifted. 
 


        

<hr>



### variable seed\_doppler\_hz\_est 

```C++
double dp_async_dsss_receiver_state_t::seed_doppler_hz_est;
```



Original (unrefined) handoff Doppler estimate. 
 


        

<hr>



### variable segments 

```C++
size_t dp_async_dsss_receiver_state_t::segments;
```



The live-tracking Dll's partial correlations per code period (refine\_segments is the refine collector's). 


        

<hr>



### variable spc 

```C++
size_t dp_async_dsss_receiver_state_t::spc;
```




<hr>



### variable sps 

```C++
size_t dp_async_dsss_receiver_state_t::sps;
```



MpskReceiver's samples per symbol. 
 


        

<hr>



### variable state 

```C++
int dp_async_dsss_receiver_state_t::state;
```



One of the ASYNC\_DSSS\_RX\_SEARCHING .. \_LOST values. 
 


        

<hr>



### variable state\_samples 

```C++
uint64_t dp_async_dsss_receiver_state_t::state_samples;
```



Running: samples fed since the current state was entered. 
 


        

<hr>



### variable sym\_lockdet 

```C++
dp_lockdet_state_t dp_async_dsss_receiver_state_t::sym_lockdet;
```




<hr>



### variable symbol\_rate 

```C++
double dp_async_dsss_receiver_state_t::symbol_rate;
```




<hr>



### variable tlm 

```C++
dp_tlm_t* dp_async_dsss_receiver_state_t::tlm;
```



NULL = detached. 
 


        

<hr>



### variable tlm\_decim 

```C++
uint32_t dp_async_dsss_receiver_state_t::tlm_decim;
```



The attach's decimation. 
 


        

<hr>



### variable tlm\_prefix 

```C++
char dp_async_dsss_receiver_state_t::tlm_prefix[DP_TLM_NAME_MAX];
```



The attach's prefix. 
 


        

<hr>



### variable tlm\_sym\_lock 

```C++
int32_t dp_async_dsss_receiver_state_t::tlm_sym_lock;
```



"&lt;prefix&gt;.sym.lock" id, -1 detached. 
 


        

<hr>



### variable tlm\_sym\_locked 

```C++
int32_t dp_async_dsss_receiver_state_t::tlm_sym_locked;
```



"&lt;prefix&gt;.sym.locked" id, -1 detached. 


        

<hr>



### variable track\_dll\_out\_buf 

```C++
float _Complex* dp_async_dsss_receiver_state_t::track_dll_out_buf;
```




<hr>



### variable track\_dll\_out\_cap 

```C++
size_t dp_async_dsss_receiver_state_t::track_dll_out_cap;
```




<hr>



### variable track\_rc\_out\_buf 

```C++
float _Complex* dp_async_dsss_receiver_state_t::track_rc_out_buf;
```




<hr>



### variable track\_rc\_out\_cap 

```C++
size_t dp_async_dsss_receiver_state_t::track_rc_out_cap;
```




<hr>



### variable tsamps 

```C++
size_t dp_async_dsss_receiver_state_t::tsamps;
```



code\_len\*spc  one code period, samples. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/async_dsss_receiver/async_dsss_receiver_core.h`

