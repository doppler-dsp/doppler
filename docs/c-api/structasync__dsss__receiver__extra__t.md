

# Struct async\_dsss\_receiver\_extra\_t



[**ClassList**](annotated.md) **>** [**async\_dsss\_receiver\_extra\_t**](structasync__dsss__receiver__extra__t.md)



_The receiver's own fields in its state blob, after the envelope and before the children._ [More...](#detailed-description)

* `#include <async_dsss_receiver_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  uint8\_t | [**\_pad**](#variable-_pad)  <br> |
|  uint64\_t | [**align\_pending**](#variable-align_pending)  <br> |
|  uint64\_t | [**both\_down\_samples**](#variable-both_down_samples)  <br> |
|  uint64\_t | [**car\_carry\_len**](#variable-car_carry_len)  <br> |
|  uint8\_t | [**car\_coasting**](#variable-car_coasting)  <br> |
|  double | [**cn0\_dbhz\_est**](#variable-cn0_dbhz_est)  <br> |
|  double | [**doppler\_hz\_est**](#variable-doppler_hz_est)  <br> |
|  double | [**excl**](#variable-excl)  <br> |
|  uint8\_t | [**had\_lock**](#variable-had_lock)  <br> |
|  double | [**lock\_den**](#variable-lock_den)  <br> |
|  double | [**lock\_metric**](#variable-lock_metric)  <br> |
|  double | [**lock\_num**](#variable-lock_num)  <br> |
|  uint64\_t | [**n**](#variable-n)  <br> |
|  uint64\_t | [**n\_excl**](#variable-n_excl)  <br> |
|  uint64\_t | [**n\_picks**](#variable-n_picks)  <br> |
|  double | [**pick\_cn0**](#variable-pick_cn0)  <br> |
|  double | [**pick\_doppler**](#variable-pick_doppler)  <br> |
|  double | [**pick\_phase**](#variable-pick_phase)  <br> |
|  uint64\_t | [**pick\_want**](#variable-pick_want)  <br> |
|  uint64\_t | [**pooled**](#variable-pooled)  <br> |
|  uint64\_t | [**rank**](#variable-rank)  <br> |
|  uint64\_t | [**refine\_min\_blocks**](#variable-refine_min_blocks)  <br> |
|  uint64\_t | [**refine\_samples\_fed**](#variable-refine_samples_fed)  <br> |
|  uint64\_t | [**refine\_segments**](#variable-refine_segments)  <br> |
|  uint64\_t | [**refused\_hits**](#variable-refused_hits)  <br> |
|  uint64\_t | [**samples\_in**](#variable-samples_in)  <br> |
|  double | [**seed\_chip\_phase**](#variable-seed_chip_phase)  <br> |
|  double | [**seed\_doppler\_hz\_est**](#variable-seed_doppler_hz_est)  <br> |
|  uint64\_t | [**segments**](#variable-segments)  <br> |
|  uint64\_t | [**sps**](#variable-sps)  <br> |
|  uint64\_t | [**srch\_hist\_len**](#variable-srch_hist_len)  <br> |
|  uint64\_t | [**srch\_n**](#variable-srch_n)  <br> |
|  uint64\_t | [**srch\_origin**](#variable-srch_origin)  <br> |
|  uint8\_t | [**state**](#variable-state)  <br> |
|  uint64\_t | [**state\_samples**](#variable-state_samples)  <br> |
|  [**dp\_lockdet\_state\_t**](structdp__lockdet__state__t.md) | [**sym\_lockdet**](#variable-sym_lockdet)  <br> |
|  int64\_t | [**threads**](#variable-threads)  <br> |












































## Detailed Description


Written whole by [**dp\_async\_dsss\_receiver\_get\_state()**](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_get_state); every field mirrors the [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) field of the same name. 


    
## Public Attributes Documentation




### variable \_pad 

```C++
uint8_t async_dsss_receiver_extra_t::_pad[5];
```



eight NAMED bytes with the three above: unnamed padding is indeterminate in a designated initialiser, and a blob that carried it would not round-trip byte for byte under every compiler. 


        

<hr>



### variable align\_pending 

```C++
uint64_t async_dsss_receiver_extra_t::align_pending;
```



v12: 0 or 1, a dwell alignment owed. 
 


        

<hr>



### variable both\_down\_samples 

```C++
uint64_t async_dsss_receiver_extra_t::both_down_samples;
```



The release clock, samples. 
 


        

<hr>



### variable car\_carry\_len 

```C++
uint64_t async_dsss_receiver_extra_t::car_carry_len;
```



Live carry samples, &lt; tsamps. 
 


        

<hr>



### variable car\_coasting 

```C++
uint8_t async_dsss_receiver_extra_t::car_coasting;
```



v4: the carrier loop is held. 
 


        

<hr>



### variable cn0\_dbhz\_est 

```C++
double async_dsss_receiver_extra_t::cn0_dbhz_est;
```



The seed's C/N0, dB-Hz. 
 


        

<hr>



### variable doppler\_hz\_est 

```C++
double async_dsss_receiver_extra_t::doppler_hz_est;
```



Seed or refined Doppler, Hz. 
 


        

<hr>



### variable excl 

```C++
double async_dsss_receiver_extra_t::excl[ASYNC_DSSS_RX_MAX_EXCLUSIONS];
```



... and phases, the unused tail zeroed. 
 


        

<hr>



### variable had\_lock 

```C++
uint8_t async_dsss_receiver_extra_t::had_lock;
```



v4: both flags have been up; the loops coast. 


        

<hr>



### variable lock\_den 

```C++
double async_dsss_receiver_extra_t::lock_den;
```



running state that survives a checkpoint 
 


        

<hr>



### variable lock\_metric 

```C++
double async_dsss_receiver_extra_t::lock_metric;
```



(config  alpha, thresholds  is 


        

<hr>



### variable lock\_num 

```C++
double async_dsss_receiver_extra_t::lock_num;
```



symbol-lock EMAs + hysteretic detector: the 


        

<hr>



### variable n 

```C++
uint64_t async_dsss_receiver_extra_t::n;
```



Layout key: must match the receiver. 


        

<hr>



### variable n\_excl 

```C++
uint64_t async_dsss_receiver_extra_t::n_excl;
```



v13: set\_exclusions()'s count ... 
 


        

<hr>



### variable n\_picks 

```C++
uint64_t async_dsss_receiver_extra_t::n_picks;
```



v15: picks not yet taken ... 
 


        

<hr>



### variable pick\_cn0 

```C++
double async_dsss_receiver_extra_t::pick_cn0[ASYNC_DSSS_RX_MAX_PEAKS];
```



C/N0s, the unused tails zeroed. 
 


        

<hr>



### variable pick\_doppler 

```C++
double async_dsss_receiver_extra_t::pick_doppler[ASYNC_DSSS_RX_MAX_PEAKS];
```



Dopplers and 
 


        

<hr>



### variable pick\_phase 

```C++
double async_dsss_receiver_extra_t::pick_phase[ASYNC_DSSS_RX_MAX_PEAKS];
```



... their phases, 


        

<hr>



### variable pick\_want 

```C++
uint64_t async_dsss_receiver_extra_t::pick_want;
```



v15: set\_search\_only()'s value. 
 


        

<hr>



### variable pooled 

```C++
uint64_t async_dsss_receiver_extra_t::pooled;
```



v15: set\_pooled()'s value (#2022). 
 


        

<hr>



### variable rank 

```C++
uint64_t async_dsss_receiver_extra_t::rank;
```



v13: set\_rank()'s value (#2022). 
 


        

<hr>



### variable refine\_min\_blocks 

```C++
uint64_t async_dsss_receiver_extra_t::refine_min_blocks;
```



v9: set\_refine\_min\_blocks()'s value, a mutator's, so it travels (#2022). 


        

<hr>



### variable refine\_samples\_fed 

```C++
uint64_t async_dsss_receiver_extra_t::refine_samples_fed;
```



Raw samples fed to the refine chain. 


        

<hr>



### variable refine\_segments 

```C++
uint64_t async_dsss_receiver_extra_t::refine_segments;
```



Layout key: must match the receiver. 


        

<hr>



### variable refused\_hits 

```C++
uint64_t async_dsss_receiver_extra_t::refused_hits;
```



v8: the searching hits refused as seeds. 


        

<hr>



### variable samples\_in 

```C++
uint64_t async_dsss_receiver_extra_t::samples_in;
```



v12: input samples since create. 
 


        

<hr>



### variable seed\_chip\_phase 

```C++
double async_dsss_receiver_extra_t::seed_chip_phase;
```



The seed's chip phase, chips. 
 


        

<hr>



### variable seed\_doppler\_hz\_est 

```C++
double async_dsss_receiver_extra_t::seed_doppler_hz_est;
```



The seed's Doppler, Hz. 
 


        

<hr>



### variable segments 

```C++
uint64_t async_dsss_receiver_extra_t::segments;
```



Layout key: must match the receiver. 


        

<hr>



### variable sps 

```C++
uint64_t async_dsss_receiver_extra_t::sps;
```



Layout key: must match the receiver. 


        

<hr>



### variable srch\_hist\_len 

```C++
uint64_t async_dsss_receiver_extra_t::srch_hist_len;
```



v14: the hand-off history's live count. 


        

<hr>



### variable srch\_n 

```C++
uint64_t async_dsss_receiver_extra_t::srch_n;
```



v14: search samples since it. 
 


        

<hr>



### variable srch\_origin 

```C++
uint64_t async_dsss_receiver_extra_t::srch_origin;
```



v14: the search resampler's origin, an input index. 
 


        

<hr>



### variable state 

```C++
uint8_t async_dsss_receiver_extra_t::state;
```



async\_dsss\_rx\_state\_t value. 
 


        

<hr>



### variable state\_samples 

```C++
uint64_t async_dsss_receiver_extra_t::state_samples;
```



Samples since the state was entered. 


        

<hr>



### variable sym\_lockdet 

```C++
dp_lockdet_state_t async_dsss_receiver_extra_t::sym_lockdet;
```



restored by create()). 
 


        

<hr>



### variable threads 

```C++
int64_t async_dsss_receiver_extra_t::threads;
```



v12: set\_threads()'s result (#2022). 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/async_dsss_receiver/async_dsss_receiver_core.h`

