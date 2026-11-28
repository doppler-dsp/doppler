

# Struct async\_dsss\_receiver\_status\_t



[**ClassList**](annotated.md) **>** [**async\_dsss\_receiver\_status\_t**](structasync__dsss__receiver__status__t.md)



_One consistent picture of what the receiver is doing, by value._ [More...](#detailed-description)

* `#include <async_dsss_receiver_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  uint64\_t | [**both\_down\_samples**](#variable-both_down_samples)  <br> |
|  double | [**car\_last\_error**](#variable-car_last_error)  <br> |
|  uint64\_t | [**carrier\_lock\_n\_noncoh**](#variable-carrier_lock_n_noncoh)  <br> |
|  int | [**carrier\_locked**](#variable-carrier_locked)  <br> |
|  double | [**chip\_phase**](#variable-chip_phase)  <br> |
|  double | [**cn0\_dbhz\_est**](#variable-cn0_dbhz_est)  <br> |
|  int | [**code\_locked**](#variable-code_locked)  <br> |
|  double | [**code\_rate**](#variable-code_rate)  <br> |
|  int | [**demod\_carrier\_locked**](#variable-demod_carrier_locked)  <br> |
|  double | [**doppler\_hz**](#variable-doppler_hz)  <br> |
|  double | [**lock\_metric**](#variable-lock_metric)  <br> |
|  double | [**lock\_threshold**](#variable-lock_threshold)  <br> |
|  int | [**locked**](#variable-locked)  <br> |
|  double | [**mpsk\_last\_error**](#variable-mpsk_last_error)  <br> |
|  uint64\_t | [**refused\_hits**](#variable-refused_hits)  <br> |
|  int | [**state**](#variable-state)  <br> |
|  uint64\_t | [**state\_samples**](#variable-state_samples)  <br> |
|  int | [**sync\_locked**](#variable-sync_locked)  <br> |












































## Detailed Description


The status record of docs/design/async-dsss-receiver.md §11.3: a holder  the controller of docs/design/async-dsss-receiver.md §8  reads it on demand, because the exclusions it hands the other receivers are keyed on this receiver's CURRENT code phase, not on its seed, and gets every field from one call between `steps()` calls, rather than assembling a picture from the one-at-a-time getters. It is a read of live state, not `get_state()`: the bytes triplet resumes the receiver elsewhere, this describes it here. Its `doppler_hz` is the live carrier (loop 1 plus loop 2), where get\_doppler\_hz() reports the estimate the chain was built from. No timestamp: the counters are in input samples, and the holder, which owns the sample clock, stamps them (docs/design/async-dsss-receiver.md §11.3). 


    
## Public Attributes Documentation




### variable both\_down\_samples 

```C++
uint64_t async_dsss_receiver_status_t::both_down_samples;
```



Input samples both flags have been down without a break (the release clock); in lost it keeps counting  samples since the flags dropped. With reacquire, the symbol lock flag's clock alone. 
 


        

<hr>



### variable car\_last\_error 

```C++
double async_dsss_receiver_status_t::car_last_error;
```



Pre-despread loop's residual, rad (slope 1). 


        

<hr>



### variable carrier\_lock\_n\_noncoh 

```C++
uint64_t async_dsss_receiver_status_t::carrier_lock_n_noncoh;
```



The carrier lock's non-coherent count, the updates one decision sums ([**dp\_async\_dsss\_receiver\_get\_carrier\_lock\_n\_noncoh()**](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_get_carrier_lock_n_noncoh)): fixed at create. 
 


        

<hr>



### variable carrier\_locked 

```C++
int async_dsss_receiver_status_t::carrier_locked;
```



The pre-despread carrier loop's lock detector (loop 1, the NDA tracker), [**dp\_carrier\_nda\_get\_locked()**](carrier__nda__core_8h.md#function-dp_carrier_nda_get_locked). 
 


        

<hr>



### variable chip\_phase 

```C++
double async_dsss_receiver_status_t::chip_phase;
```



Code phase, chips, at the sample after the last one fed: the live Dll's, carried over the partial period the chain holds; while refining, the seed's advanced by the clock dilation since (what the hand-over starts from), so a refining receiver's signal can be excluded too. 
 


        

<hr>



### variable cn0\_dbhz\_est 

```C++
double async_dsss_receiver_status_t::cn0_dbhz_est;
```



C/N0 estimate, dB-Hz (the seed's). 
 


        

<hr>



### variable code\_locked 

```C++
int async_dsss_receiver_status_t::code_locked;
```



Presence flag: the Dll's lock detector. 
 


        

<hr>



### variable code\_rate 

```C++
double async_dsss_receiver_status_t::code_rate;
```



The Dll's code rate, chips per nominal chip (~1.0 at lock), without the carrier aid; a coasting Dll's is its held rate. 
 


        

<hr>



### variable demod\_carrier\_locked 

```C++
int async_dsss_receiver_status_t::demod_carrier_locked;
```



The demodulator's (loop 2's) carrier lock detector, [**dp\_mpsk\_receiver\_get\_locked()**](mpsk__receiver__core_8h.md#function-dp_mpsk_receiver_get_locked); 0 with no demodulator built. 
 


        

<hr>



### variable doppler\_hz 

```C++
double async_dsss_receiver_status_t::doppler_hz;
```



Where the emitter is NOW: the whole carrier estimate, Hz  loop 1's plus what loop 2 has taken up beyond it (the seed while refining, 0 while searching, frozen where it was when lost). Only as good as `locked`: with the carrier unlocked, loop 1 free-runs and this wanders. 


        

<hr>



### variable lock\_metric 

```C++
double async_dsss_receiver_status_t::lock_metric;
```



cos(2\*phi) over the symbols, drives `locked`. 
 


        

<hr>



### variable lock\_threshold 

```C++
double async_dsss_receiver_status_t::lock_threshold;
```



`locked` declares strictly above this. 
 


        

<hr>



### variable locked 

```C++
int async_dsss_receiver_status_t::locked;
```



Health flag: the symbol-lock detector. 
 


        

<hr>



### variable mpsk\_last\_error 

```C++
double async_dsss_receiver_status_t::mpsk_last_error;
```



Post-despread carrier residual, rad. 
 


        

<hr>



### variable refused\_hits 

```C++
uint64_t async_dsss_receiver_status_t::refused_hits;
```



Searching hits refused as seeds by the seed domain (a Doppler at or past fs/2), since create or reset. The search went on over each one's tail. 
 


        

<hr>



### variable state 

```C++
int async_dsss_receiver_status_t::state;
```



ASYNC\_DSSS\_RX\_SEARCHING .. \_LOST  where it is. 
 


        

<hr>



### variable state\_samples 

```C++
uint64_t async_dsss_receiver_status_t::state_samples;
```



Input samples since `state` was entered. 
 


        

<hr>



### variable sync\_locked 

```C++
int async_dsss_receiver_status_t::sync_locked;
```



The demodulator's timing (sync) lock detector, [**dp\_mpsk\_receiver\_get\_sync\_locked()**](mpsk__receiver__core_8h.md#function-dp_mpsk_receiver_get_sync_locked); 0 with no demodulator built. With `locked`, `code_locked`, `carrier_locked` and `demod_carrier_locked`, the five flags the error-rate window opens on, each read as up only while `state` is tracking: the flags of a receiver searching or refining are not its link's ([**dp\_ber\_lock\_all()**](ber__core_8h.md#function-dp_ber_lock_all); async-dsss-receiver.md, "The error-rate
window"). 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/async_dsss_receiver/async_dsss_receiver_core.h`

