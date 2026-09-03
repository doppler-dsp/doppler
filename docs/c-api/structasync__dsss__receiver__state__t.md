

# Struct async\_dsss\_receiver\_state\_t



[**ClassList**](annotated.md) **>** [**async\_dsss\_receiver\_state\_t**](structasync__dsss__receiver__state__t.md)



_Composed receiver state._ [More...](#detailed-description)

* `#include <async_dsss_receiver_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  [**acq\_state\_t**](structacq__state__t.md) \* | [**acq**](#variable-acq)  <br> |
|  [**carrier\_acq\_state\_t**](structcarrier__acq__state__t.md) \* | [**ca**](#variable-ca)  <br> |
|  [**costas\_state\_t**](structcostas__state__t.md) | [**car**](#variable-car)  <br> |
|  float \_Complex \* | [**car\_carry\_buf**](#variable-car_carry_buf)  <br> |
|  size\_t | [**car\_carry\_len**](#variable-car_carry_len)  <br> |
|  [**costas\_state\_t**](structcostas__state__t.md) | [**car\_frozen**](#variable-car_frozen)  <br> |
|  float \_Complex \* | [**car\_wiped\_buf**](#variable-car_wiped_buf)  <br> |
|  double | [**carrier\_freq\_hz**](#variable-carrier_freq_hz)  <br> |
|  double | [**chip\_rate**](#variable-chip_rate)  <br> |
|  double | [**cn0\_dbhz**](#variable-cn0_dbhz)  <br> |
|  double | [**cn0\_dbhz\_est**](#variable-cn0_dbhz_est)  <br> |
|  uint8\_t \* | [**code**](#variable-code)  <br> |
|  size\_t | [**code\_len**](#variable-code_len)  <br> |
|  int | [**differential**](#variable-differential)  <br> |
|  [**dll\_state\_t**](structdll__state__t.md) \* | [**dll**](#variable-dll)  <br> |
|  double | [**doppler\_hz\_est**](#variable-doppler_hz_est)  <br> |
|  double | [**lock\_alpha**](#variable-lock_alpha)  <br> |
|  double | [**lock\_den**](#variable-lock_den)  <br> |
|  double | [**lock\_metric**](#variable-lock_metric)  <br> |
|  double | [**lock\_num**](#variable-lock_num)  <br> |
|  int | [**m**](#variable-m)  <br> |
|  int | [**n**](#variable-n)  <br> |
|  double | [**pd**](#variable-pd)  <br> |
|  double | [**pfa**](#variable-pfa)  <br> |
|  [**RateConverter\_state\_t**](structRateConverter__state__t.md) \* | [**rc**](#variable-rc)  <br> |
|  double | [**refine\_design\_margin\_db**](#variable-refine_design_margin_db)  <br> |
|  [**dll\_state\_t**](structdll__state__t.md) \* | [**refine\_dll**](#variable-refine_dll)  <br> |
|  float \_Complex \* | [**refine\_dll\_out\_buf**](#variable-refine_dll_out_buf)  <br> |
|  size\_t | [**refine\_dll\_out\_cap**](#variable-refine_dll_out_cap)  <br> |
|  double | [**refine\_max\_error\_db**](#variable-refine_max_error_db)  <br> |
|  size\_t | [**refine\_max\_n\_blocks**](#variable-refine_max_n_blocks)  <br> |
|  size\_t | [**refine\_n\_fft**](#variable-refine_n_fft)  <br> |
|  [**RateConverter\_state\_t**](structRateConverter__state__t.md) \* | [**refine\_rc**](#variable-refine_rc)  <br> |
|  float \_Complex \* | [**refine\_rc\_out\_buf**](#variable-refine_rc_out_buf)  <br> |
|  size\_t | [**refine\_rc\_out\_cap**](#variable-refine_rc_out_cap)  <br> |
|  uint64\_t | [**refine\_samples\_fed**](#variable-refine_samples_fed)  <br> |
|  size\_t | [**refine\_samples\_per\_symbol**](#variable-refine_samples_per_symbol)  <br> |
|  size\_t | [**refine\_segments**](#variable-refine_segments)  <br> |
|  bool | [**refine\_sequential**](#variable-refine_sequential)  <br> |
|  size\_t | [**refine\_zero\_pad**](#variable-refine_zero_pad)  <br> |
|  [**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* | [**rx**](#variable-rx)  <br> |
|  uint64\_t | [**samples\_fed**](#variable-samples_fed)  <br> |
|  double | [**seed\_chip\_phase**](#variable-seed_chip_phase)  <br> |
|  double | [**seed\_doppler\_hz\_est**](#variable-seed_doppler_hz_est)  <br> |
|  size\_t | [**segments**](#variable-segments)  <br> |
|  size\_t | [**spc**](#variable-spc)  <br> |
|  size\_t | [**sps**](#variable-sps)  <br> |
|  int | [**state**](#variable-state)  <br> |
|  [**lockdet\_state\_t**](structlockdet__state__t.md) | [**sym\_lockdet**](#variable-sym_lockdet)  <br> |
|  double | [**symbol\_rate**](#variable-symbol_rate)  <br> |
|  size\_t | [**tsamps**](#variable-tsamps)  <br> |












































## Detailed Description


Every child (acq, both refine-stage children, both track-stage children) is allocated at create() with a placeholder seed (phase 0, no Doppler) and REBUILT (not freed to NULL) on every real transition  a fixed shape, independent of `state`, matching `DsssReceiver`'s own serialization-simplicity rationale. Treat all fields as internal (use the getters). 


    
## Public Attributes Documentation




### variable acq 

```C++
acq_state_t* async_dsss_receiver_state_t::acq;
```




<hr>



### variable ca 

```C++
carrier_acq_state_t* async_dsss_receiver_state_t::ca;
```




<hr>



### variable car 

```C++
costas_state_t async_dsss_receiver_state_t::car;
```




<hr>



### variable car\_carry\_buf 

```C++
float _Complex* async_dsss_receiver_state_t::car_carry_buf;
```




<hr>



### variable car\_carry\_len 

```C++
size_t async_dsss_receiver_state_t::car_carry_len;
```




<hr>



### variable car\_frozen 

```C++
costas_state_t async_dsss_receiver_state_t::car_frozen;
```




<hr>



### variable car\_wiped\_buf 

```C++
float _Complex* async_dsss_receiver_state_t::car_wiped_buf;
```




<hr>



### variable carrier\_freq\_hz 

```C++
double async_dsss_receiver_state_t::carrier_freq_hz;
```



nominal RF carrier, Hz; &gt; 0 enables the carrier-&gt;code rate aiding, 0 = off (config, not running state  restored by create). 


        

<hr>



### variable chip\_rate 

```C++
double async_dsss_receiver_state_t::chip_rate;
```




<hr>



### variable cn0\_dbhz 

```C++
double async_dsss_receiver_state_t::cn0_dbhz;
```



Design C/N0  feeds both Acquisition's own sizing and (derated) CarrierAcquisition's design\_snr on every refine-chain (re)build. 


        

<hr>



### variable cn0\_dbhz\_est 

```C++
double async_dsss_receiver_state_t::cn0_dbhz_est;
```



Cached from the winning acquisition hit. 
 


        

<hr>



### variable code 

```C++
uint8_t* async_dsss_receiver_state_t::code;
```




<hr>



### variable code\_len 

```C++
size_t async_dsss_receiver_state_t::code_len;
```




<hr>



### variable differential 

```C++
int async_dsss_receiver_state_t::differential;
```




<hr>



### variable dll 

```C++
dll_state_t* async_dsss_receiver_state_t::dll;
```




<hr>



### variable doppler\_hz\_est 

```C++
double async_dsss_receiver_state_t::doppler_hz_est;
```



Current best estimate: == seed\_ doppler\_hz\_est while refining, the CarrierAcquisition-refined value once tracking. 
 


        

<hr>



### variable lock\_alpha 

```C++
double async_dsss_receiver_state_t::lock_alpha;
```



EMA coeff = 1/dwell (dwell &gt;= 30). 
 


        

<hr>



### variable lock\_den 

```C++
double async_dsss_receiver_state_t::lock_den;
```




<hr>



### variable lock\_metric 

```C++
double async_dsss_receiver_state_t::lock_metric;
```




<hr>



### variable lock\_num 

```C++
double async_dsss_receiver_state_t::lock_num;
```




<hr>



### variable m 

```C++
int async_dsss_receiver_state_t::m;
```




<hr>



### variable n 

```C++
int async_dsss_receiver_state_t::n;
```



MpskReceiver's own carrier-arm count. 
 


        

<hr>



### variable pd 

```C++
double async_dsss_receiver_state_t::pd;
```



Also CarrierAcquisition's own pd. 
 


        

<hr>



### variable pfa 

```C++
double async_dsss_receiver_state_t::pfa;
```



Also CarrierAcquisition's own pfa. 
 


        

<hr>



### variable rc 

```C++
RateConverter_state_t* async_dsss_receiver_state_t::rc;
```




<hr>



### variable refine\_design\_margin\_db 

```C++
double async_dsss_receiver_state_t::refine_design_margin_db;
```




<hr>



### variable refine\_dll 

```C++
dll_state_t* async_dsss_receiver_state_t::refine_dll;
```




<hr>



### variable refine\_dll\_out\_buf 

```C++
float _Complex* async_dsss_receiver_state_t::refine_dll_out_buf;
```




<hr>



### variable refine\_dll\_out\_cap 

```C++
size_t async_dsss_receiver_state_t::refine_dll_out_cap;
```




<hr>



### variable refine\_max\_error\_db 

```C++
double async_dsss_receiver_state_t::refine_max_error_db;
```




<hr>



### variable refine\_max\_n\_blocks 

```C++
size_t async_dsss_receiver_state_t::refine_max_n_blocks;
```




<hr>



### variable refine\_n\_fft 

```C++
size_t async_dsss_receiver_state_t::refine_n_fft;
```




<hr>



### variable refine\_rc 

```C++
RateConverter_state_t* async_dsss_receiver_state_t::refine_rc;
```




<hr>



### variable refine\_rc\_out\_buf 

```C++
float _Complex* async_dsss_receiver_state_t::refine_rc_out_buf;
```




<hr>



### variable refine\_rc\_out\_cap 

```C++
size_t async_dsss_receiver_state_t::refine_rc_out_cap;
```




<hr>



### variable refine\_samples\_fed 

```C++
uint64_t async_dsss_receiver_state_t::refine_samples_fed;
```



Raw samples fed into the refine pipeline since it was last (re)built  the running counter the give-up cap and the tail computation on refine-&gt;track transition both use. 


        

<hr>



### variable refine\_samples\_per\_symbol 

```C++
size_t async_dsss_receiver_state_t::refine_samples_per_symbol;
```




<hr>



### variable refine\_segments 

```C++
size_t async_dsss_receiver_state_t::refine_segments;
```



[**dll\_lookback\_segments()**](dll__core_8h.md#function-dll_lookback_segments) result, cached at the hit that (re)built the refine chain  config for refine\_dll's own layout, needed again by set\_state's check. 


        

<hr>



### variable refine\_sequential 

```C++
bool async_dsss_receiver_state_t::refine_sequential;
```




<hr>



### variable refine\_zero\_pad 

```C++
size_t async_dsss_receiver_state_t::refine_zero_pad;
```




<hr>



### variable rx 

```C++
mpsk_receiver_state_t* async_dsss_receiver_state_t::rx;
```




<hr>



### variable samples\_fed 

```C++
uint64_t async_dsss_receiver_state_t::samples_fed;
```



Running total handed to [**acq\_push()**](acq__core_8h.md#function-acq_push) so far  diffed against acq-&gt;samples\_consumed right after a hit, same technique DsssReceiver's own steps() uses. 
 


        

<hr>



### variable seed\_chip\_phase 

```C++
double async_dsss_receiver_state_t::seed_chip_phase;
```



Original handoff chip phase  reused verbatim to seed the FRESH live-tracking Dll, not wherever the refine-stage Dll drifted to. 
 


        

<hr>



### variable seed\_doppler\_hz\_est 

```C++
double async_dsss_receiver_state_t::seed_doppler_hz_est;
```



Original (unrefined) handoff Doppler estimate. 
 


        

<hr>



### variable segments 

```C++
size_t async_dsss_receiver_state_t::segments;
```



Live-tracking Dll's own segments  distinct from refine\_segments above (see the module docstring / [**dll\_lookback\_segments()**](dll__core_8h.md#function-dll_lookback_segments)'s own doc on the WINDOWS vs TRACK\_WINDOWS split). 


        

<hr>



### variable spc 

```C++
size_t async_dsss_receiver_state_t::spc;
```




<hr>



### variable sps 

```C++
size_t async_dsss_receiver_state_t::sps;
```



MpskReceiver's own samples/symbol. 
 


        

<hr>



### variable state 

```C++
int async_dsss_receiver_state_t::state;
```



0 = searching, 1 = refining, 2 = tracking. 
 


        

<hr>



### variable sym\_lockdet 

```C++
lockdet_state_t async_dsss_receiver_state_t::sym_lockdet;
```




<hr>



### variable symbol\_rate 

```C++
double async_dsss_receiver_state_t::symbol_rate;
```




<hr>



### variable tsamps 

```C++
size_t async_dsss_receiver_state_t::tsamps;
```



code\_len\*spc  one code period, samples. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/async_dsss_receiver/async_dsss_receiver_core.h`

